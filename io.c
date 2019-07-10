#include "config.h"
#include "io.h"


bool ota_receive_ready(uint8_t temp)
{
    uint8_t copyBuf[sizeof(tempUARTBuf)];
    for (uint8_t j = 0; j < sizeof(tempUARTBuf); j++)
        copyBuf[j] = tempUARTBuf[j];
    for (uint8_t i = 0; i < sizeof(tempUARTBuf) - 1; i++)
        tempUARTBuf[(uint8_t)(i + 1)] = copyBuf[i];             // shift all entries by one
    tempUARTBuf[0] = temp;                          // store last-received byte
    if (tempUARTBuf[0] == 'Q'/*'&'*/ && tempUARTBuf[1] == 'F'/*'&'*/ && tempUARTBuf[2] == 'R'/*'&'*/)  // check for OTA req (in this case, RFQ)
        return true;
    else
        return false;
}


bool header_received(uint8_t temp)
{
    uint8_t copyBuf[sizeof(tempUARTBuf)];
    for (uint8_t j = 0; j < sizeof(tempUARTBuf); j++)
        copyBuf[j] = tempUARTBuf[j];
    for (uint8_t i = 0; i < sizeof(tempUARTBuf) - 1; i++)
        tempUARTBuf[(uint8_t)(i + 1)] = copyBuf[i];             // shift all entries by one
    tempUARTBuf[0] = temp;                          // store last-received byte
    if (tempUARTBuf[0] == '\n'/*'&'*/ && tempUARTBuf[1] == 'D'/*'&'*/ && tempUARTBuf[2] == 'E'/*'&'*/ && tempUARTBuf[3] == 'R')  // check for header (in this case, RED)
        return true;
    else
        return false;
}



void init_mem()
{
    init_spi();
    erase_mem();
}


//void start_timer()
//{
//    T1CONbits.nT1SYNC = 1;
//    T1CONbits.TMR1CS = 0b00;//10
//    T1CONbits.T1CKPS = 0b11;
//    PIE1bits.TMR1IE = 1;
//    INTCONbits.PEIE = 1;
//    PIR1bits.TMR1IF = 0;
//    INTCONbits.GIE = 1;
//    T1CONbits.TMR1ON = 1;
//}
//
//
//void stop_timer()
//{
//    T1CONbits.TMR1ON = 0;
//    PIE1bits.TMR1IE = 0;
//    PIR1bits.TMR1IF = 0;
//    timedOut = false;
//}


void erase_mem()
{
    uint8_t memStatus = 2;
//    init_spi();
    enable_write();
    MEM_SPI_BEGIN();
    send_spi(0xC7);//60);
    MEM_SPI_END();
    // The following is a placeholder to allow 10s max chip erase time.
    // Should be replaced with check for Write In Progress bit.
    for(uint8_t i = 0; i < 10; i++)
    {
        __delay_ms(100);
        CLRWDT();
    }
    
    while ((memStatus & 0x01) != 0x00)
        memStatus = read_status();
    NOP();     
}


void enable_write()
{
    uint8_t memStatus = 0;
    while ((memStatus & 0x03) != 0x02)
    {
        MEM_SPI_BEGIN();
        send_spi(0x06);     // WREN
        MEM_SPI_END();
        __delay_ms(1);
        memStatus = read_status();
    }
}

void disable_write()
{
    uint8_t memStatus = 2, memTemp = 2;
    memStatus = read_status();
    memTemp = (uint8_t)(memStatus & 0x02);
    while (memTemp != 0x00)
    {
        MEM_SPI_BEGIN();
        send_spi(0x04);     // WRDI
        MEM_SPI_END();
        memStatus = read_status();
        memTemp = (uint8_t)(memStatus & 0x03);
    }
}


uint8_t read_status()
{
    uint8_t status = 0;
    MEM_SPI_BEGIN();
    send_spi(0x05);     // RDSR
    status = send_spi(0xFF);
    MEM_SPI_END();
    return status;
}


void set_burst_length(uint8_t len)      // for reading bytes
{
    MEM_SPI_BEGIN();
    send_spi(0xC0);
    send_spi(len);//0x00);     // buffering byte
    send_spi(len);
    MEM_SPI_END();
}



bool reprogram_pic_ok(uint8_t addrL, uint8_t addrH, uint8_t addrU, uint8_t len)
{
    bool progOK = false;
    uint8_t addr[3];
    addr[0] = addrL;
    addr[1] = addrH;
    addr[2] = addrU;
    
    progOK = program_block_ok(addr, len);
    
    return progOK;
}

// program external flash memory
void page_program(uint8_t pageNum, uint16_t dataLen)        // assume data stored in memBlock
{
    uint8_t memStatus = 0;
    uint8_t addr[3];
    addr[0] = (uint8_t)((uint16_t)(pageNum * 256) & 0x00FF);
    addr[1] = (uint8_t)(((uint16_t)(pageNum * 256) & 0xFF00) >> 8);
    addr[2] = 0;
    
    
    init_spi();
    while (memStatus != 0x02)// *** Needs timeout
    {
        enable_write();
        memStatus = (uint8_t)(read_status() & 0x03);
    }
    
    /* TEMP */
        MEM_SPI_BEGIN();
        uint8_t temp = send_spi(0x9F);//rdid
        uint8_t ver = send_spi(0xFF);
        uint8_t ver1 = send_spi(0xFF);
        uint8_t ver2 = send_spi(0xFF);
        MEM_SPI_END();
        
        
    memStatus = 0;
    while (memStatus != 0x02)// *** Needs timeout
    {
        enable_write();
        memStatus = (uint8_t)(read_status() & 0x03);
    }
    
    // Send data to be programmed 
    MEM_SPI_BEGIN();
    send_spi(0x02);
    for (uint8_t i = 0; i < 3; i++)
        send_spi(addr[i]);
    for (uint16_t i = 0; i < dataLen; i++)           // terminates at dataLen; the rest of the page values are 0xFF.
        send_spi(memBlock[i]);
    if (dataLen < 255)
        send_spi(0xFF);
    
    MEM_SPI_END();
}




uint8_t* read(uint8_t addr[], uint8_t numBytes)
{
    uint8_t data[64] = {0};
    uint8_t memStatus = 0;
    bool writeInProgress = true;
    init_spi();
    __delay_ms(3);
    memStatus = (uint8_t)(read_status() & 0x03);
    while (memStatus == 0x03)
    {
        disable_write();
        memStatus = (uint8_t)(read_status() & 0x03);
    }
    while (writeInProgress == true)     // wait until not writing any more
        writeInProgress = write_in_progress();
    set_burst_length(numBytes);
    MEM_SPI_BEGIN();
    send_spi(0x03);
    for (uint8_t i = 0; i < 3; i++)
        send_spi(addr[i]);
    for (uint8_t j = 0; j < numBytes; j++)
    {
        __delay_us(10);
        data[j] = send_spi(0xFF);
    }
    MEM_SPI_END();
    return (uint8_t *)&data[0];
}



bool write_in_progress()
{
    uint8_t memStatus = 0, memTemp = 0;
    memStatus = read_status();
    memTemp = (uint8_t)(memStatus & 0x02);
    if (memTemp == 0x02)
        enable_write();
    memTemp = (uint8_t)(memStatus & 0x01);
    bool inProgress = (bool)(memTemp == 0x01);
    return inProgress;
}



void setup_program_hub()
{
    disable_uart();
    BOOT_CTRL = 1;
    ATBUS_TX = 0;
    __delay_ms(100);
    CLRWDT();
    __delay_ms(100);
    CLRWDT();
    BOOT_CTRL = 0;
    __delay_ms(20);
    BOOT_SEL_TRIS = 1;      // release boot_sel
    BOOT_SEL_PU = 1;
    ATBUS_TX = 1;
    init_uart();
}



void en_boot_sel_int()
{
    IOCAPbits.IOCAP4 = 1;
    INTCONbits.IOCIE = 1;
    INTCONbits.IOCIF = 0;
    IOCAFbits.IOCAF4 = 0;
    INTCONbits.GIE = 1;
    receivedAck = false;
    okToReceiveAck = false;
}

void dis_boot_sel_int()
{
    IOCAPbits.IOCAP4 = 0;
    INTCONbits.IOCIF = 0;
    INTCONbits.IOCIE = 0;
}


void pulse_boot_sel()
{
    BOOT_SEL_PU = 0;
    BOOT_SEL_TRIS = 0;
    BOOT_SEL = 0;
    __delay_us(400);
    BOOT_SEL = 1;
    BOOT_SEL_TRIS = 1;
    BOOT_SEL_PU = 1;
}


void prepare_for_sleep()
{
    BAUDCONbits.WUE = 1;
    PIE1bits.RCIE = 1;
    INTCONbits.PEIE = 1;
    INTCONbits.GIE = 1;
}



void start_timer()          // 
{
    _1minTimerCnt = 0;
    
    T2CONbits.T2CKPS = 0b11;        // 1:64 prescale
    T2CONbits.T2OUTPS = 0b00;       // 1:1 postscale
    T2CONbits.TMR2ON = 1;
    PIE1bits.TMR2IE = 1;
    INTCONbits.PEIE = 1;
    INTCONbits.GIE = 1;
}


void stop_timer()
{
    UARTtimedOut = false;
    _1minTimerCnt = 0;
    minTimerOn = false;
    T2CONbits.TMR2ON = 0;
    PIE1bits.TMR2IE = 0;
}


void start_ack_timer()              // 10ms timeout typ.; 20ms for config
{
    timedOut = false;
    t0cnt = 0;
    OPTION_REGbits.PS = 0b000;
    OPTION_REGbits.PSA = 0b0;
    OPTION_REGbits.T0CS = 0b0;      // src is Fosc/4
    INTCONbits.TMR0IE = 1;
    INTCONbits.TMR0IF = 0;
    INTCONbits.GIE = 1;
}

void stop_ack_timer()
{
    timedOut = false;
    INTCONbits.TMR0IE = 0;
    INTCONbits.TMR0IF = 0;
}