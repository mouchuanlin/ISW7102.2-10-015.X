/* 
 * File:   config.h
 * Author: Scott
 *
 * Created on November 16, 2018, 12:52 PM
 */

#ifndef CONFIG_H
#define	CONFIG_H

// CONFIG1
#pragma config FOSC = INTOSC    // Oscillator Selection (INTOSC oscillator: I/O function on CLKIN pin)
#pragma config WDTE = OFF//SWDTEN    // Watchdog Timer Enable (WDT controlled by the SWDTEN bit in the WDTCON register)
#pragma config PWRTE = OFF      // Power-up Timer Enable (PWRT disabled)
#pragma config MCLRE = ON       // MCLR Pin Function Select (MCLR/VPP pin function is MCLR)
#pragma config CP = OFF         // Flash Program Memory Code Protection (Program memory code protection is enabled)
#pragma config CPD = OFF        // Data Memory Code Protection (Data memory code protection is enabled)
#pragma config BOREN = OFF      // Brown-out Reset Enable (Brown-out Reset disabled)
#pragma config CLKOUTEN = OFF   // Clock Out Enable (CLKOUT function is disabled. I/O or oscillator function on the CLKOUT pin)
#pragma config IESO = OFF       // Internal/External Switchover (Internal/External Switchover mode is disabled)
#pragma config FCMEN = ON       // Fail-Safe Clock Monitor Enable (Fail-Safe Clock Monitor is enabled)

// CONFIG2
#pragma config WRT = OFF        // Flash Memory Self-Write Protection (Write protection off)
#pragma config PLLEN = OFF      // PLL Enable (4x PLL disabled)
#pragma config STVREN = ON      // Stack Overflow/Underflow Reset Enable (Stack Overflow or Underflow will cause a Reset)
#pragma config BORV = LO        // Brown-out Reset Voltage Selection (Brown-out Reset Voltage (Vbor), low trip point selected.)
#pragma config LVP = OFF        // Low-Voltage Programming Enable (High-voltage on MCLR/VPP must be used for programming)

// #pragma config statements should precede project file includes.
// Use project enums instead of #define for ON and OFF.

#include <xc.h>
#include <stdint.h>
#include <stdbool.h>

#define _XTAL_FREQ      8000000

#define MEM_HOLD        LATAbits.LATA0
#define BOOT_CTRL       LATAbits.LATA2
#define nCS_MEM         LATCbits.LATC3
#define SCK_MEM_TRIS    TRISCbits.TRISC0
#define SDI_MEM_TRIS    TRISCbits.TRISC1
#define SDO_MEM_TRIS    TRISCbits.TRISC2
#define nCS_MEM_TRIS    TRISCbits.TRISC3
#define ATBUS_TX_TRIS   TRISCbits.TRISC4
#define ATBUS_IM_TRIS   TRISCbits.TRISC5
#define BOOT_SEL_TRIS   TRISAbits.TRISA4
#define ATBUS_TX        LATCbits.LATC4
#define BOOT_SEL        LATAbits.LATA4
#define BOOT_SELI       PORTAbits.RA4
#define nMD_PWR         LATAbits.LATA5
#define BOOT_SEL_PU     WPUAbits.WPUA4
#define MUX_CTRL        LATAbits.LATA1
//#define              LATAbits.LATA4
//#define          PORTAbits.RA5
//#define ATBUS_IM_TRIS   TRISCbits.TRISC5
//#define    TRISCbits.TRISC4

//#define LED_ON()        LED=0
//#define LED_OFF()       LED=1
#define MEM_SPI_BEGIN() nCS_MEM=0
#define MEM_SPI_END()   nCS_MEM=1

#define CRC16           0x1021//XMODEM          8005

#define _1MIN           2440//0x7A        // for timer 2 with 1:64 prescale & 1:1 postscale
#define _10MIN          24400
#define LONG_TIME       0xFFFE
#define _100MS          390
#define _10MS           39// for timer 0
#define _20MS           78// for timer 0
#define _10S            39000

#define NUM_BLOCKS      1024//16
#define NUM_WORDS       64
#define NUM_ADDRS       4

#define STX             0x0F
#define ETX             0x04
#define DLE             0x05
#define PWRITE          0x04

#define BREAK_CHAR      0x17
#define ACK             0x06
#define NACK            0x15


void init_pic();
void check_state();
void ascii_to_mem(uint8_t dataBuf[], uint8_t len);
void ascii_to_hex(uint8_t dataBuf[], uint8_t len);
void intel_to_mem(uint8_t dataBuf[], uint8_t len);
void save_to_mem(uint8_t *dataBuf, uint8_t len);
void save_config(uint8_t dataBuf[], uint8_t len);
void check_crc();//uint8_t dataBuf[]);
void blink();
void reset_ota();
void prepare_for_sleep();
void setup_program_hub();
void tell_hub_close_connection();
void parse_new_data();
bool program_page_ok(uint16_t index);
void handle_eof();


/****TEST*/
void write_byte_test();


enum OperationalState {
    Passthrough,
    WaitReady,
    Datasave,
    WaitForABreak,  //ACK
    WaitForNBreak,  //NACK
    WaitToCloseConnection,
    ProgramHub,
//    ProgramPart
};
enum OperationalState state;
enum OperationalState prevState;

uint8_t modemChar = 0, lastModemChar = 0;
uint8_t tempUARTBuf[4] = {0};
uint8_t rawData[60] = {0};
uint8_t unalteredData[60] = {0};
uint8_t ppos = 0;
uint8_t memBlock[256] = {0};//64] = {0};
uint8_t configBytes[14] = {0};
uint8_t rdpos = 0, currentPage = 0;//mdposi = 0, mdposj = 0, currentPage = 0;
uint8_t terminationCharCnt = 0;
uint8_t startingAddrL = 0, startingAddrH = 0, startingAddrU = 0;
uint8_t errCnt = 0;
uint8_t cAddrL, cAddrM, cAddrH;
uint8_t dataLen;
uint8_t colonCount = 0;
uint16_t _1minTimerCnt = 0;
uint16_t mdpos = 0;
uint16_t crcResult = 0, serverCRCVal = 0;
uint24_t currentAddr = 0;
uint24_t cAddr = 0, t0cnt = 0;
uint32_t lastAddr = 0, savedAddr = 0;

bool checksumOK = false, crcOK = false;
bool okToAddChar = false;
bool reachedEOF = false;
bool receivedTermination = false;
bool isExtendedAddr = false;
bool allPagesProgrammed = false;
bool programmedOK = false;
bool minTimerOn = false, readyForNextChar = true;
bool fullProgram = false, sendAckNack = false;
bool doneProcessing = true;

#endif	/* CONFIG_H */
