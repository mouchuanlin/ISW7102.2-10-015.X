#include "config.h"

void init_spi()
{
    SDO_MEM_TRIS = 0;
    SDI_MEM_TRIS = 1;
    SCK_MEM_TRIS = 0;
    nCS_MEM_TRIS = 0;
    nCS_MEM = 1;
    
    SSP1STAT = 0x00;
    SSP1CON1 = 0x00;
    SSP1CON1bits.SSPM = 0b0000;     // SCK = F_osc/4
    SSP1CON1bits.CKP = 0;
    SSP1STATbits.CKE = 1;
    SSP1CON2 = 0x00;
    
    //---------PIE1----------
    PIE1bits.SSP1IE = 0;
    
    //---------PIR1----------
    PIR1bits.SSP1IF = 0;
    
    SSP1CON1bits.SSPEN = 1;
}


uint8_t send_spi(uint8_t data)
{
    uint8_t temp;

    SSP1IF = 0;
    SSP1BUF = data;
    while(!SSP1IF);
    temp = SSPBUF;//read the buffer
    return temp;//return the buffered byte

    //while(BF) continue;// wait until complete this bit transmission
}