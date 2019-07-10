/* 
 * File:   io.h
 * Author: Scott
 *
 * Created on November 16, 2018, 3:39 PM
 */

#ifndef IO_H
#define	IO_H

#include <stdint.h>
#include <stdbool.h>


/* CHECK OTA COMMAND FROM SERVER */
bool ota_receive_ready(uint8_t temp);
/* CHECK HEADER RECEIVED FROM SERVER */
bool header_received(uint8_t temp); 
/* UART */
void add_to_crc(uint8_t val);
uint16_t _CRC_16( uint16_t crcVal, uint8_t data);
void init_uart();
void disable_uart();
void write_uart(unsigned char data);
void receive_data(uint8_t temp);
void send_bad_ota();
void init_mem();
void start_timer();
void stop_timer();
void check_termination();
bool erase_block_ok(uint16_t index);
void en_boot_sel_int();
void dis_boot_sel_int();
void pulse_boot_sel();
void start_ack_timer();
void stop_ack_timer();
bool reprogram_pic_ok(uint8_t addrL, uint8_t addrH, uint8_t addrU, uint8_t len);
bool program_block_ok(uint8_t addr[], uint16_t len);
bool write_config_ok(uint8_t *data);
bool start_tx_ok();
bool run_pic_ok();
bool start_bootloader_ok();
/* SPI */
void init_spi();
uint8_t send_spi(uint8_t data);
void enable_write();
void disable_write();
void erase_mem();
uint8_t read_status();
bool write_in_progress();
void init_mem();
uint8_t* read(uint8_t addr[], uint8_t numBytes);
void page_program(uint8_t pageNum, uint16_t dataLen);
void set_burst_length(uint8_t len);


/* TEST */
void pulse_bootsel();


bool receivedHeader = false, dataReady = false;
bool timedOut = false, UARTtimedOut = false;
bool receivedAck = false, okToReceiveAck = false;
bool programmingConfigBits = false;





/* *** */
    uint8_t check = 0;

#endif	/* IO_H */
