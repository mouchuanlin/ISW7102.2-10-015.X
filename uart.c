
#include "config.h"
#include "io.h"


void init_uart()
{
    ATBUS_TX_TRIS = 0;
    ATBUS_TX = 1;
    __delay_ms(10);
    ATBUS_TX = 0;
    __delay_ms(10);
    ATBUS_IM_TRIS = 1;
    APFCON0bits.RXDTSEL = 0;        // Rx on RC5
    APFCON0bits.TXCKSEL = 0;        // Tx on RC4
    TXSTA = 0x26;       //00100110
    RCSTA = 0x90;       //10010000
    BAUDCON = 0b00001010;     //11001000
    SPBRG = 207;//6;//103;        //19200bps
    SPBRGH = 0;
    BAUDCONbits.BRG16 = 1;
//    BAUDCONbits.WUE = 1;
//    SPBRG = 16;         // NOTE: ensure the hub code is always operating at 115.2kbaud uart to the modem, for now.
//    SPBRGH = 0;   

//    RXD_IN = 1;         /* Set RX pin as input */
//    TXD_IN = 0;         /* Set TX pin as output */
//    
//    SYNC = 0;           /* Config uart as asynchronous */
//    SPEN = 1;           /* Enable UART module */
//    
    /* Enable both Rx and Tx ports */
    CREN = 1;
    TXEN = 1;
    SYNC = 0;
    SPEN = 1;
    
//    BAUDCONbits.WUE = 1;
    PIE1bits.RCIE = 1;
    INTCONbits.PEIE = 1;
    INTCONbits.GIE = 1;
}

void disable_uart()
{
    SPEN = 0;
    CREN = 0;
    TXEN = 0;
    PIE1bits.RCIE = 0;
}


void write_uart(unsigned char data)
{
    while (!TRMT);              // Wait for UART TX buffer to empty completely
    TXREG = data;
}


/* 
 * Converts ASCII to hex from modem UART. 
 * Calculates running CRC.
 * Stores hex in 1-1 memory location in external memory chip.
 */
void send_data(uint8_t dataBuf[], uint8_t len)
{
    init_uart();
    for (uint8_t i = 0; i < len; i++)
    {
        write_uart(dataBuf[i]);
    }
}



void add_to_crc(uint8_t val)
{
    crcResult = (uint16_t)(_CRC_16(crcResult, val));
}



/************* CRC16 *******************
*   Calculates the CRC-16 of a single byte the rf protocols.
*   @param  uint16_t crcVal  This iteration of CRC calculations will use this value to start out
*   @param  uint8_t data    This is the data that will generate the resulting CRC
*   @param  uint16_t poly   the polynomial to use for the CRC16
*   @return uint16_t crcVal  The resulting CRC
**************************************************/
uint16_t _CRC_16( uint16_t crcVal, uint8_t data){
    bool carryBit = false;
    for( uint8_t bitMask = 0x80; bitMask != 0; bitMask >>= 1 )
    {
        carryBit = (bool)((crcVal & 0x8000) != 0);// !! is a convenient way to convert a value to a logical true of false value in C only
        crcVal <<= 1;
        if( ( bitMask & data ) != 0 )               //bit is a 1
        {
            if( !carryBit )
                crcVal ^= CRC16;
        }
        else                                      //bit is a 0
        {
            if( carryBit )
                crcVal ^= CRC16;
        }
    }
    return crcVal;
}



void send_bad_ota()
{
    uint8_t badString[] = "OFA\r\n";    // ota failure
    send_data(badString, sizeof(badString));
}


bool erase_block_ok(uint16_t index)       // erases all flash addresses on block # index
{
    uint16_t localCRCval = 0, cnt;
    uint8_t stream[9];
    uint8_t i = 0;
    bool progOK = false;
    stream[0] = STX;
    stream[1] = 0x03;
    stream[2] = (uint8_t)((index * 64) & 0x0000FF);//(uint8_t)((index * 256) & 0x0000FF);
    stream[3] = (uint8_t)(((index * 64) & 0x00FF00) >> 8);//(uint8_t)(((index * 256) & 0x00FF00) >> 8);
    stream[4] = (uint8_t)(((index * 64) & 0xFF0000) >> 16);//(uint8_t)(((index * 256) & 0xFF0000) >> 16);
    stream[5] = 0x00;
    stream[6] = 0x01;//4;//10;
    crcResult = 0;
    for (i = 1; i < 7; i++)
        add_to_crc(stream[i]);
    stream[7] = (uint8_t)(crcResult & 0x00FF);//(uint8_t)(localCRCval & 0x00FF);
    stream[8] = (uint8_t)((crcResult & 0xFF00) >> 8);//(uint8_t)((localCRCval & 0xFF00) >> 8);
    
    
    for (uint8_t j = 0; j < 10; j++)        // retry 10 times
    {
        if (start_tx_ok())
        {
            __delay_ms(5);
            for (i = 1; i < sizeof(stream); i++)
            {
                if (stream[i] == STX || stream[i] == ETX || stream[i] == DLE)       // use DLE if data looks like command char
                    write_uart(0x05);
                write_uart(stream[i]);
            }
        }
        write_uart(ETX);
        CLRWDT();
        cnt = 16000;
        while(BOOT_SELI && --cnt > 0)
        {
            if (!BOOT_SELI)
                __delay_us(5);
        }
        if (cnt != 0 && !BOOT_SELI)
        {
            cnt = 8000;
            __delay_ms(5);//1);
            while (!BOOT_SELI && --cnt > 0)
            {
                if (BOOT_SELI)
                    __delay_us(5);
            }
            if (cnt != 0 && BOOT_SELI)
            {
                j = 10;
                progOK = true;
            }
        }
//            start_ack_timer();
//            en_boot_sel_int();// interrupt on pos-going edge of boot_sel
//            while (!receivedAck && !timedOut)
//            {
//                if (!BOOT_SELI)
//                {
//                    __delay_us(100);
//                    if (!BOOT_SELI)
//                        okToReceiveAck = true;      // enables the ability to set receiveAck = true
//                }
//            }
//            stop_ack_timer();
//            if (receivedAck)
//            {
//                progOK = true;
//                j = 10;
//            }
//            dis_boot_sel_int();
        __delay_ms(10);
    }
    return progOK;
}




bool start_tx_ok()
{
    uint16_t cnt = 0;
    for (uint8_t h = 0; h < 5; h++)
    {
        for (uint8_t i = 0; i < 10; i++)
        {
            CLRWDT();
            write_uart(STX);
//            cnt = 1000;
//            while(BOOT_SELI && --cnt > 0)
//            {
//                if (!BOOT_SELI)
//                    __delay_us(5);
//            }
//            if (cnt != 0 && !BOOT_SELI)
//            {
//                cnt = 8000;
//                __delay_ms(1);
//                while (!BOOT_SELI && --cnt > 0)
//                {
//                    if (BOOT_SELI)
//                        __delay_us(5);
//                }
//                if (cnt != 0 && BOOT_SELI)
//                    i = 10;
//            }
//            __delay_ms(10);
//        }
////        write_uart(0x00);
////        write_uart(0x00);
////        write_uart(0x00);
////        write_uart(ETX);
            cnt = 1000;
            while(BOOT_SELI && --cnt > 0)
            {
                if (!BOOT_SELI)
                    __delay_us(5);
            }
            if (cnt != 0 && !BOOT_SELI)
            {
                cnt = 8000;
                __delay_ms(1);
                while (!BOOT_SELI && --cnt > 0)
                {
                    if (BOOT_SELI)
                        __delay_us(5);
                }
                if (cnt != 0 && BOOT_SELI)
                    return true;
            }
            __delay_ms(10);
        }
    }
    return false;
}



bool program_block_ok(uint8_t addr[], uint16_t len)
{
    uint16_t cnt = 1000;
    uint8_t prefix[];
    uint8_t sd[64];
    
    
    uint8_t crcL, crcH, i;
    bool progOK = false;
    
    
//    /* TEST */
//    addr[2] = 0x00;
//    addr[1] = 0xFF;
//    addr[0] = 0xC0;
    
    
    
        MEM_SPI_BEGIN();
        uint8_t temp = send_spi(0x9F);//rdid
        uint8_t ver = send_spi(0xFF);
        uint8_t ver1 = send_spi(0xFF);
        uint8_t ver2 = send_spi(0xFF);
        MEM_SPI_END();
        
        
    uint8_t memStatus = 0x02;
    while (memStatus != 0x00)
    {
        disable_write();
        memStatus = (uint8_t)(read_status() & 0x03);//** if this line works then replace all memTemps with memStatus
    }
    
    // Send data to be programmed 
    MEM_SPI_BEGIN();
    send_spi(0x03);
    for (i = 0; i < 3; i++)
    {
        send_spi(addr[i]);
    }
    for (uint8_t j = 0; j < sizeof(sd); j++)
    {
        sd[j] = send_spi(0xFF);
    }
    MEM_SPI_END();
    
    
    
    // for now, test only:
//    for (i = 0; i < sizeof(savedData); i++)
//        savedData[i] = i;
    crcResult = 0;
    add_to_crc(PWRITE);
    for (i = 0; i < 3; i++)
        add_to_crc(addr[i]);
    add_to_crc(0x00);                   // addr upper
    add_to_crc(0x01);                   // program only 1 block at a time
    for (i = 0; i < len; i++)
        add_to_crc(sd[i]);
    crcL = (uint8_t)(crcResult & 0x00FF);
    crcH = (uint8_t)((crcResult & 0xFF00) >> 8);
    // assume data is 64bytes long
    for (uint8_t h = 0; h < 10; h++)
    {
        if (start_tx_ok())
        {
            write_uart(0x05);
            write_uart(PWRITE);
            if (addr[0] == DLE || addr[0] == ETX || addr[0] == STX)
                write_uart(0x05);
            write_uart(addr[0]);
            if (addr[1] == DLE || addr[1] == ETX || addr[1] == STX)
                write_uart(0x05);
            write_uart(addr[1]);
            if (addr[2] == DLE || addr[2] == ETX || addr[2] == STX)
                write_uart(0x05);
            write_uart(addr[2]);
            write_uart(0x00);
            write_uart(0x01);       // num blocks to write
            for (i = 0; i < NUM_WORDS; i++)
            {
                if (sd[i] == DLE || sd[i] == ETX || sd[i] == STX)
                    write_uart(0x05);
                write_uart(sd[i]);
            }
            write_uart(crcL);
            write_uart(crcH);
            write_uart(ETX);
            
            /* WAIT FOR BOOT SEL TO GO LOW AND HIGH AGAIN *** may need to drive via interrupt */
//            cnt = 1000;
//            while(BOOT_SELI && --cnt > 0)
//            {
//                if (!BOOT_SELI)
//                    __delay_us(5);
//            }
            start_ack_timer();
            en_boot_sel_int();// interrupt on pos-going edge of boot_sel
            while (!receivedAck && !timedOut)
            {
                if (!BOOT_SELI)
                {
                    __delay_us(100);
                    if (!BOOT_SELI)
                        okToReceiveAck = true;      // enables the ability to set receiveAck = true
                }
            }
            stop_ack_timer();
            if (receivedAck)
            {
                progOK = true;
                h = 10;
            }
            dis_boot_sel_int();
//            if (cnt != 0 && !BOOT_SELI)
//            {
//                cnt = 8000;
//                __delay_ms(1);
//                while (!BOOT_SELI && --cnt > 0)
//                {
//                    if (BOOT_SELI)
//                        __delay_us(5);
//                }
//                if (cnt != 0 && BOOT_SELI)
//                {
//                    progOK = true;
//                    h = 10;
//                }
//            }
        }
        __delay_ms(10);
    }
    if (!progOK)
    {
        reset_ota();
    }
    return progOK;
}



bool write_config_ok(uint8_t *data)
{
    uint8_t crcL, crcH, numBytes = 0x0E;
    uint8_t addrH = 0x30, addrM = 0x00, addrL = 0x00;
    uint16_t cnt = 1000;
    
    crcResult = 0;
    add_to_crc(0x07);
    add_to_crc(addrL);
    add_to_crc(addrM);
    add_to_crc(addrH);
    add_to_crc(0x00);
    add_to_crc(numBytes);
    for (uint8_t i = 0; i < numBytes; i++)
    {
        add_to_crc(*data);
        data++;
    }
    data -= numBytes;
    crcL = (uint8_t)(crcResult & 0x00FF);
    crcH = (uint8_t)((crcResult & 0xFF00) >> 8);
    
    for (uint8_t h = 0; h < 10; h++)
    {
        if (start_tx_ok())
        {
            __delay_ms(5);
            write_uart(0x07);
            if (addrL == DLE || addrL == ETX || addrL == STX)
                write_uart(0x05);
            write_uart(addrL);
            if (addrM == DLE || addrM == ETX || addrM == STX)
                write_uart(0x05);
            write_uart(addrM);
            if (addrH == DLE || addrH == ETX || addrH == STX)
                write_uart(0x05);
            write_uart(addrH);
            write_uart(0x00);
            write_uart(numBytes);
            for (uint8_t i = addrL; i < numBytes; i++)
            {
                if (*data == DLE || *data == ETX || *data == STX)
                    write_uart(DLE);
                write_uart(*data);
                data++;
            }
            if (crcL == DLE || crcL == ETX || crcL == STX)
                write_uart(0x05);
            write_uart(crcL);
            if (crcH == DLE || crcH == ETX || crcH == STX)
                write_uart(0x05);
            write_uart(crcH);
            write_uart(ETX);
            programmingConfigBits = true;
            start_ack_timer();
            en_boot_sel_int();// interrupt on pos-going edge of boot_sel
            while (!receivedAck && !timedOut)
            {
                if (!BOOT_SELI)
                {
                    __delay_us(100);
                    if (!BOOT_SELI)
                        okToReceiveAck = true;      // enables the ability to set receiveAck = true
                }
            }
            stop_ack_timer();
            if (receivedAck)
                h = 10;
            dis_boot_sel_int();
            
        }
    }
    return receivedAck;
}

// NOT YET USED
//bool write_eeprom_ok(uint8_t addrL, uint8_t addrM, uint8_t addrH, uint16_t len)
//{
//    uint16_t lenL, lenH;
//    uint8_t crcL, crcH;
//    lenL = (uint8_t)(len & 0x00FF);
//    lenH = (uint8_t)((len & 0xFF00) >> 8);
//    crcResult = 0;
//    add_to_crc(0x06);
//    add_to_crc(addrL);
//    add_to_crc(addrM);
//    add_to_crc(addrH);
//    add_to_crc(0x00);
//    add_to_crc(lenL);
//    add_to_crc(lenH);
//    for (uint16_t i = 0; i < len; i++)
//    {
//        add_to_crc(*data);
//        data++;
//    }
//    crcL = (uint8_t)(crcResult & 0x00FF);
//    crcH = (uint8_t)((crcResult & 0xFF00) >> 8);
//    for (uint8_t i = 0; i < 10; i++)
//    {
//        if (start_tx_ok())
//        {
//            write_uart(0x06);
//            if (addrL == DLE || addrL == ETX || addrL == STX)
//                write_uart(DLE);
//            write_uart(addrL);
//            if (addrM == DLE || addrM == ETX || addrM == STX)
//                write_uart(DLE);
//            write_uart(addrM);
//            if (addrH == DLE || addrH == ETX || addrH == STX)
//                write_uart(DLE);
//            write_uart(addrH);
//            write_uart(0x00);
//            if (lenL == DLE || lenL == ETX || lenL == STX)
//                write_uart(DLE);
//            write_uart(lenL);
//            if (lenH == DLE || lenH == ETX || lenH == STX)
//                write_uart(DLE);
//            write_uart(lenH);
//            for (uint16_t j = 0; j < len; j++)
//            {
//                if (*data == DLE || *data == ETX || *data == STX)
//                    write_uart(DLE);
//                write_uart(*data);
//                data++;
//            }
//            write_uart(crcL);
//            write_uart(crcH);
//            write_uart(ETX);
//            start_ack_timer();
//            en_boot_sel_int();// interrupt on pos-going edge of boot_sel
//            while (!receivedAck && !timedOut)
//            {
//                if (!BOOT_SELI)
//                {
//                    __delay_us(100);
//                    if (!BOOT_SELI)
//                        okToReceiveAck = true;      // enables the ability to set receiveAck = true
//                }
//            }
//            stop_ack_timer();
//            if (receivedAck)
//                i = 10;
//            dis_boot_sel_int();
//        }
//        __delay_ms(10);
//    }
//    return receivedAck;
//}


bool run_pic_ok()
{
    uint8_t crcL, crcH;
    BOOT_CTRL = 0;
    crcResult = 0;
    add_to_crc(0x08);
    crcL = (uint8_t)(crcResult & 0x00FF);
    crcH = (uint8_t)((crcResult & 0xFF00) >> 8);
    for (uint8_t i = 0; i < 10; i++)
    {
        if (start_tx_ok())
        {
            write_uart(0x08);
            if (crcL == DLE || crcL == ETX || crcL == STX)
                write_uart(DLE);
            write_uart(crcL);
            if (crcH == DLE || crcH == ETX || crcH == STX)
                write_uart(DLE);
            write_uart(crcH);
            write_uart(ETX);
            start_ack_timer();
            en_boot_sel_int();// interrupt on pos-going edge of boot_sel
            while (!receivedAck && !timedOut)
            {
                if (!BOOT_SELI)
                {
                    __delay_us(100);
                    if (!BOOT_SELI)
                        okToReceiveAck = true;      // enables the ability to set receiveAck = true
                }
            }
            stop_ack_timer();
            if (!receivedAck)
            {
                i = 10;         // shouldn't get an ACK for this command
                receivedAck = true;
            }
            dis_boot_sel_int();
        }
        __delay_ms(10);
        CLRWDT();
    }
    return receivedAck;
}



bool start_bootloader_ok()
{
    bool progOK = false;
    // assume data is 64bytes long
    for (uint8_t h = 0; h < 10; h++)
    {
        if (start_tx_ok())
        {
            write_uart(0x00);
            write_uart(0x00);
            write_uart(0x00);
            write_uart(ETX);
            
            start_ack_timer();
            en_boot_sel_int();// interrupt on pos-going edge of boot_sel
            while (!receivedAck && !timedOut)
            {
                if (!BOOT_SELI)
                {
                    __delay_us(100);
                    if (!BOOT_SELI)
                        okToReceiveAck = true;      // enables the ability to set receiveAck = true
                }
            }
            stop_ack_timer();
            if (receivedAck)
            {
                progOK = true;
                h = 10;
            }
            dis_boot_sel_int();
//            if (cnt != 0 && !BOOT_SELI)
//            {
//                cnt = 8000;
//                __delay_ms(1);
//                while (!BOOT_SELI && --cnt > 0)
//                {
//                    if (BOOT_SELI)
//                        __delay_us(5);
//                }
//                if (cnt != 0 && BOOT_SELI)
//                {
//                    progOK = true;
//                    h = 10;
//                }
//            }
        }
        __delay_ms(10);
    }
    if (!progOK)
    {
        reset_ota();
    }
    return progOK;
}