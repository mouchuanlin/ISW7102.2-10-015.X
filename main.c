/* 
 * File:   main.c
 * Author: Scott
 *
 * Created on November 16, 2018, 12:52 PM
 * 011->013 - 01 Jul 2019. JV. Added CRC to end of hex reception.
 * 013->014 - 05 Jul 2019. JV. Added logic to control MUX during OTA. TEST ONLY.
 * 014->015 - 08 Jul 2019. JV. Cleaned up state machine and code for readability.
 */

#include "config.h"
#include "io.h"

/*
 * 
 */

void __interrupt isr()
{
    if (PIR1bits.RCIF)
    {
        PIR1bits.RCIF = 0;
        modemChar = RCREG;
        if (state != Datasave && state != WaitForNBreak && state != WaitForABreak && state != WaitToCloseConnection)
            TXREG = modemChar;
        if (modemChar != BREAK_CHAR)
        {
            okToAddChar = true;
            rawData[rdpos++] = modemChar;
        }
        
        else if (modemChar == BREAK_CHAR && (state == Datasave || state == WaitForNBreak || state == WaitForABreak))
            sendAckNack = true;
        
        if (rdpos >= sizeof(rawData) || modemChar == ':')
            rdpos = 0;
        unalteredData[ppos++] = modemChar;
        if (ppos >= sizeof(unalteredData) || modemChar == ':')
            ppos = 0;
    }
    
    if (PIR1bits.TMR2IF)
    {
        PIR1bits.TMR2IF = 0;
        _1minTimerCnt++;
        if (_1minTimerCnt >= LONG_TIME)
        {
            UARTtimedOut = true;
            stop_timer();
        }
    }
    
    if (IOCAFbits.IOCAF4)
    {
        IOCAFbits.IOCAF4 = 0;
        if (okToReceiveAck)
            receivedAck = true;
    }
    
    // run second timer for OTA BOOT_SEL time out
    if (INTCONbits.TMR0IF)
    {
        INTCONbits.TMR0IF = 0;
        if (!programmingConfigBits)
        {
            if (t0cnt++ >= _100MS)//_10MS)// TIMING NEEDS TO BE TESTED
            {
                timedOut = true;
                t0cnt = 0;
            }
        }
        else
        {
            if (t0cnt++ >= _100MS)//_20MS)// TIMING NEEDS TO BE TESTED
            {
                timedOut = true;
                t0cnt = 0;
                programmingConfigBits = false;
            }
        }
    }
}

void main()
{
    init_pic();     // enable WDT
//    init_uart();    // enable UART
//    
//    init_mem();
//    
//    MEM_SPI_BEGIN();
//    uint8_t rdid = send_spi(0x9F);//rdid
//    rdid = send_spi(0xff);
//    rdid = send_spi(0xff);
//    rdid = send_spi(0xff);
//    rdid = send_spi(0xff);
//    MEM_SPI_END();
//    
//    
//    MEM_SPI_BEGIN();
//    send_spi(0xB9);     // Deep PWDN
//    MEM_SPI_END();
        
        
    while(1)
    {
//        if (RCSTAbits.OERR)
//        {
//            CREN = 0;
//            __delay_us(3);
//            CREN = 1;
//        }
//        if (RCSTAbits.FERR)
//        {
//            uint8_t dummy = RCREG;
//        }
//        check_state();
        if (!minTimerOn)
        {
//            prepare_for_sleep();
                CLRWDT();
                SLEEP();
                NOP();
        }
                    // *** TEST
                    MUX_CTRL = 1;
//                    write_uart(0x06);
                    MUX_CTRL = 0;
//        __delay_ms(10);
        CLRWDT();
    }
}


void init_pic()
{
    OSCCON = 0b01111000;        // 16MHz INTOSC select (01110000 for 8MHz)
    OPTION_REG = 0b01000111;    // WPU enabled, 1:256 Timer0 prescaler,
                                // interrupt on rising edge of INT pin
    
    ANSELA = 0x00;
    ANSELC = 0x00;
    TRISA = 0b00001000;
    WPUA = 0b00000001;
//    start_ack_timer();          // MEM_HOLD has WPU enabled
    start_timer();              // UART timeout
    TRISC = 0b00010000;
    
    PORTA = 0b00111000;
    PORTC = 0b00100000;
    
    WDTCONbits.WDTPS = 0b00111;
    WDTCONbits.SWDTEN = 1;
}

void check_state()
{
    uint8_t memStatus = 0, badTryCnt = 0;
    uint16_t j = 0;
    switch (state)
    {
        case Passthrough:
            if (okToAddChar && readyForNextChar)
            {
                okToAddChar = false;
                if (ota_receive_ready(modemChar))
                {
                    prevState = state;
                    state = WaitReady;
                    BOOT_SEL = 0;
                    nMD_PWR = 0;
                    start_timer();      // how long to wait?
                }
                minTimerOn = true;
                start_timer();
            }
            break;
        case WaitReady:
            if (okToAddChar)// && !UARTtimedOut)
            {
                okToAddChar = false;
                UARTtimedOut = false;
                timedOut = false;
                if (header_received(modemChar))
                {
                    prevState = state;
                    state = Datasave;
                    rdpos = 0;
                    MUX_CTRL = 1;
                }
                start_timer();      // how long to wait?
            }
            break;
        case Datasave:
            if (okToAddChar)
            {
                okToAddChar = false;
                parse_new_data();
            }
            if (reachedEOF && receivedTermination)
            {
                check_crc();
                reachedEOF = false;
                handle_eof();
            }
            break;
        case WaitToCloseConnection:
            badTryCnt = 0;
            while (!receivedAck && badTryCnt++ < 20)
                tell_hub_close_connection();
            if (receivedAck)
            {
                receivedAck = false;
                setup_program_hub();
                prevState = state;
                state = ProgramHub;
                init_spi();
            }
            break;
        case WaitForABreak:
            if (sendAckNack)
            {
                MUX_CTRL = 1;
                sendAckNack = false;
                write_uart(ACK);
                prevState = state;
                state = Datasave;
                MUX_CTRL = 0;
            }
            if (UARTtimedOut)       // timed out without receiving new UART chars
                reset_ota();
            break;
        case WaitForNBreak:
            if (sendAckNack)
            {
                MUX_CTRL = 1;
                sendAckNack = false;
                write_uart(NACK);
//            if (currentAddr >= savedAddr)
                prevState = state;
                state = Datasave;
                MUX_CTRL = 0;
            }
            if (UARTtimedOut)       // timed out without receiving new UART chars
                reset_ota();
            break;
            
            // need to add code here to wait until receive boot_sel pulse from hub before programming
        case ProgramHub:
            for (uint16_t i = 32; i < 1024; i++)		// on a 64-byte (1-page) basis, start at 0x800
            {
                badTryCnt = 0;
                if (!program_page_ok(i))
                {
                    badTryCnt ++;
                    if (badTryCnt >= 5)
                        reset_ota();
                }
            }
            
        while (!write_config_ok((uint8_t *)&configBytes[0]));
        while (!run_pic_ok());
        reset_ota();
        break;
    }
}