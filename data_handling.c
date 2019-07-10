#include "config.h"
#include "io.h"

uint16_t currentNumBytes = 0;
bool areConfigBits = false;


/* TEST ONLY */
uint8_t buffer[100] = {0};
uint8_t bpos = 0;
uint8_t currentChecksum = 0;

uint8_t extCnt = 0;
uint8_t cnt = 0;

void ascii_to_mem(uint8_t* dataBuf, uint8_t len) {
    ascii_to_hex(dataBuf, len); // 1,3,5... array positions filled
    intel_to_mem(dataBuf, (uint8_t) ((len - 1) / 2)); // => memData[]
    if (checksumOK && !isExtendedAddr && (prevState == WaitForNBreak && currentAddr >= lastAddr)) {
        checksumOK = false;
        if (reachedEOF && !areConfigBits) {
            // TEST***
            terminationCharCnt = 3;
            // must program into memory here
            //            save_to_mem(dataBuf, currentNumBytes);
            check_crc();
        } else if (!reachedEOF && !areConfigBits)
            save_to_mem(dataBuf, currentNumBytes);
        else if (areConfigBits) {
            areConfigBits = false;
            save_config(dataBuf, currentNumBytes);
        }
        lastAddr = currentAddr;
        //        write_uart(ACK);
        prevState = state;
        state = WaitForABreak;
    } else if (!checksumOK)//&& !isExtendedAddr)??
    {
        //        reset_ota();
        savedAddr = lastAddr;
        prevState = state;
//        state = WaitForNBreak;
    }
}

void ascii_to_hex(uint8_t* dataBuf, uint8_t len) {
    /* TEST */uint8_t revCnt = 0;
    uint8_t temp = 0, tempBuf[sizeof (rawData)];
    for (uint8_t i = 0; i < len; i++) {
        if (*dataBuf >= 0x30 && *dataBuf <= 0x39)
            *dataBuf -= 0x30;
        else if (*dataBuf >= 0x41 && *dataBuf <= 0x46)
            *dataBuf = (uint8_t) (*dataBuf - 0x41 + 0x0A);
        if ((i % 2) == 0)
            tempBuf[i / 2] = (uint8_t) ((*dataBuf << 4) & 0xF0);
        else
            tempBuf[i / 2] |= (uint8_t) (*dataBuf & 0x0F);
        dataBuf++;
    }
    dataBuf -= (len); // reset pointer to 0th index
    //    if (tempBuf[0] == 1)        /* TEST */
    //        INTCONbits.GIE = 0;
    for (uint8_t j = 0; j < len / 2 + 1; j++) {
        temp = tempBuf[j];
        *dataBuf = temp;
        dataBuf++;
        /* TEST */
        revCnt++;
    }
    /* TEST */
    INTCONbits.GIE = 1;
    //    dataBuf -= revCnt;
    //    if (*dataBuf == 0x10)
    //        NOP();
}

void intel_to_mem(uint8_t* dataBuf, uint8_t len) {
    uint8_t numBytes, checksum, recordType;
    uint16_t checksumL = 0;
    uint8_t tempBuf[sizeof (rawData)] = {0};

    for (uint8_t i = 0; i < len; i++) {
        tempBuf[i] = *dataBuf; // transfer all contents of dataBuf to tempBuf
        dataBuf++;
    }
    dataBuf -= len;
    numBytes = tempBuf[0];
    currentNumBytes = numBytes; // save the number of bytes for memory
    // 7 = data offset; 2 = checksum offset from end of data
    checksumL = tempBuf[(uint8_t) (numBytes + 3 + 1)]; // + 2)];
    recordType = tempBuf[3];
    if (isExtendedAddr) {
        isExtendedAddr = false;
        currentAddr = (uint32_t) ((lastAddr << 16) | ((tempBuf[1] << 8) | (tempBuf[2])));
        if (currentAddr >= 0x300000 && currentAddr < 0x30000F) {
            areConfigBits = true;
        }
        //        INTCONbits.GIE = 1;
    } else
        currentAddr = (uint32_t) ((tempBuf[1] << 8) | (tempBuf[2]));
    if (recordType == 0x04) {
        isExtendedAddr = true;
    }
    if (isExtendedAddr)
        currentAddr = (uint32_t) ((tempBuf[4] << 8) | tempBuf[5]);


    if (currentAddr == 0 && numBytes == 0)
        if (numBytes == 0x00 && recordType == 0x01 && tempBuf[4] == 0xFF)
            reachedEOF = true;

    //checksum needs to be verified--only add odd array positions i.e. 1,3,5...
    for (uint8_t j = 0; j < numBytes + 3 + 1; j++)
        checksumL += tempBuf[j];
    checksum = (uint8_t) (checksumL & 0x00FF);
    for (uint8_t i = 0; i < numBytes; i++) {
        *dataBuf = tempBuf[(uint8_t) (i + 4)];
        dataBuf++;
    }
    dataBuf -= numBytes;
    if (checksum == 0)
        checksumOK = true;
    else
        checksumOK = false;
}

void save_to_mem(uint8_t *dataBuf, uint8_t len) {
    uint8_t addr[3] = {0}, tempAddr[3] = {0};
    uint8_t i = 0, j = 0;
    addr[0] = 0; // can only program in blocks of 256 bytes  //(uint8_t)(currentAddr & 0x0000FF);
    addr[1] = (uint8_t) ((currentAddr & 0x00FF00) >> 8);
    addr[2] = 0; //unused in this PIC18 chip   (uint8_t)((currentAddr & 0xFF0000) >> 16);
    currentPage = addr[1];

    if (mdpos == 0xFC)
        NOP();

    if (addr[1] == 0xFF)// && ((currentAddr & 0x00FF00) >> 8) == 0xFC)
        NOP();

    currentAddr = currentAddr & 0x00FFFF;
    uint24_t ttt = (uint24_t) ((lastAddr & 0xFF00) + mdpos);
    // if the addr of current data does not line up with the next anticipated address
    if (currentAddr != (uint24_t) ((lastAddr & 0xFF00) + mdpos)) // note mdpos must be incremented at end of each addition to the array,
    { // except when resetting the position in the array.
        // if addr of current data is still within the same block
        if (currentAddr > (uint24_t) (lastAddr) && currentAddr < (uint24_t) (lastAddr & 0xFF00 + 255)) {
            // fill buffer from current position to desired position with 0xFF
            for (i = mdpos; i <= (currentAddr % 256); i++)
                memBlock[i] = 0xFF;
            mdpos = (uint8_t) (currentAddr % 256);
            for (j = 0; j < len; j++) {
                memBlock[mdpos++] = *dataBuf;
                dataBuf++;
                if (mdpos >= 256) {
                    /*if (*/page_program(currentPage, mdpos); //)
                    //                    {
                    mdpos = 0;
                    addr[1]++; // addr[1] = number of 256-byte blocks
                    //                    }
                    //                    else
                    //                        ;// error
                }
            }
        }            // if addr of current data is outside this block,
        else if (currentAddr > (uint24_t) (lastAddr & 0xFF00 + 255)) {
            tempAddr[0] = 0x00;
            tempAddr[1] = (uint8_t) ((lastAddr & 0xFF00) >> 8);
            tempAddr[2] = 0x00;

            // program the last block in first,
            /*if (*/page_program(tempAddr[1], mdpos); //)
            //            {
            // set pointer at starting data address
            mdpos = currentAddr % 256;

            // fill the initial unused portion of the block with 0xFF
            for (i = 0; i < mdpos; i++)
                memBlock[i] = 0xFF;
            for (j = 0; j < len; j++) {
                // store all available data in consecutive array spaces
                if (i < 255) {
                    memBlock[i++] = *dataBuf;
                    dataBuf++;
                    mdpos++;
                }                    // if the space runs out, program the buffer into memory
                    // and wrap the data.
                else {
                    /*if (!*/page_program(addr[1], mdpos); //)
                    ; // error
                    addr[1]++;
                    i = 0;
                    mdpos = 0;
                }
            }
            /* *** */
            NOP();
            //            }
            //            else
            //                ;   // error
        }
    }        // normal fill of memory buf with data at consecutive memory spaces
    else {
        if (!reachedEOF) {
            for (i = 0; i < len; i++) {
                memBlock[mdpos++] = *dataBuf;
                dataBuf++;
                if (mdpos >= sizeof (memBlock)) {
                    /*if (!*/page_program(currentPage, mdpos); //)
                    //                        ;   // error
                    addr[1]++;
                    mdpos = 0;
                }
            }
            //            write_byte_test();
        } else {
            tempAddr[0] = 0x00;
            tempAddr[1] = (uint8_t) ((lastAddr & 0xFF00) >> 8);
            tempAddr[2] = 0x00;
            /*if (!*/page_program(tempAddr[1], mdpos); //)
            //                ;   // error
            mdpos = 0;
        }
    }
}

void save_config(uint8_t dataBuf[], uint8_t len) {
    uint8_t addr = (uint8_t) (currentAddr - 0x300000);
    uint8_t i;
    bool okToStartHere = false;
    for (i = 0; i < len; i++) {
        if (addr == i)
            okToStartHere = true;
        if (okToStartHere)
            configBytes[i] = dataBuf[i]; // skip over address & num bytes
    }
}

void check_crc()//uint8_t dataBuf[])
{
    //    uint16_t tempCRC = (uint16_t)(dataBuf[0] << 8 | dataBuf[1]); 
    if (crcResult == serverCRCVal)
        crcOK = true;
    else
        crcOK = false;
}

void check_termination() {
    uint8_t temp;
    if (terminationCharCnt >= 3) {
        terminationCharCnt = 0;
        receivedTermination = true;
        serverCRCVal = 0;
        //        for (uint8_t i = (uint8_t)(rdpos-6); i <= (uint8_t)(rdpos-3); i++)
        for (uint8_t i = (uint8_t) (rdpos - 7); i <= (uint8_t) (rdpos - 4); i++) {
            temp = rawData[i];
            if (temp >= 0x30 && temp <= 0x39)
                temp -= 0x30;
            else if (temp >= 0x41 && temp <= 0x46)
                temp = (uint8_t) (temp - 0x41 + 0x0A);
            serverCRCVal |= (uint8_t) (temp & 0x0F);
            serverCRCVal <<= 4;
        }
    }
}

void fill_array(uint8_t addrL, uint8_t addrH, uint8_t *data) {
    uint8_t addr[2];
    addr[0] = addrL;
    addr[1] = addrH;
    *data = (uint8_t *)(read(addr, NUM_WORDS));
    NOP();
}

/* *** */
void write_byte_test() {
    init_uart();
    write_uart(mdpos);
    __delay_ms(1);
    for (uint16_t i = 230; i < 256; i++)
        write_uart(memBlock[i]);
}

void reset_ota() {
    MUX_CTRL = 0;
    send_bad_ota();
    stop_timer();
    prevState = state;
    state = Passthrough;
    sendAckNack = false;
    receivedTermination = false;
    //    rdpos = 0;
    init_mem();
}

bool program_page_ok(uint16_t index) {
    uint8_t j = 0;
    uint8_t addrL, addrH, addrU, crcL, crcH;
    bool progOK = false;
    // inner loop until boot_select goes low
    for (uint8_t h = 0; h < 10; h++) {
        while (!start_tx_ok());
        //end inner loop
        //                    en_boot_sel_int();// interrupt on pos-going edge of boot_sel
        okToReceiveAck = true;
        start_ack_timer();
        write_uart(0x00);
        write_uart(0x00);
        write_uart(0x00);
        write_uart(ETX); // wait
        t0cnt = 0;
        while (!receivedAck && !timedOut) {
            if (!BOOT_SELI) {
                __delay_us(50);
                if (!BOOT_SELI) {
                    while (!timedOut && !receivedAck) {
                        if (BOOT_SELI)
                            receivedAck = true; //okToReceiveAck = true;      // enables the ability to set receiveAck = true
                    }
                }
            }
        }
        stop_ack_timer();
        if (receivedAck) {
            receivedAck = false;
            progOK = true;
            h = 10;
        }
        //                    dis_boot_sel_int();
        __delay_ms(10);
    }
    timedOut = !progOK;

    // end outer loop
    currentAddr = (uint24_t) (index * 64);
    addrL = (uint8_t) (currentAddr & 0x0000FF);
    addrH = (uint8_t) ((currentAddr & 0x00FF00) >> 8);
    addrU = 0;

    if (!timedOut) {

        // ERASE BLOCK IN PIC
        // outer loop until boot_select goes low
        for (uint8_t h = 0; h < 10; h++) {
            // inner loop until boot_select goes low
            while (!start_tx_ok());
            // end inner loop
            crcResult = 0;
            rawData[0] = 0x03; // temporarily use rawData here
            rawData[1] = addrL;
            rawData[2] = addrH;
            rawData[3] = addrU;
            rawData[4] = 0x00;
            rawData[5] = 0x01; // work on 4-page (256-byte) basis
            for (j = 0; j < 6; j++)
                add_to_crc(rawData[j]);
            crcL = (uint8_t) (crcResult & 0x00FF);
            crcH = (uint8_t) ((crcResult & 0xFF00) >> 8);
            rawData[6] = crcL;
            rawData[7] = crcH;
            rawData[8] = ETX;
            for (j = 0; j < 8; j++) {
                if (rawData[j] == STX || rawData[j] == ETX || rawData[j] == DLE)
                    write_uart(DLE);
                write_uart(rawData[j]);
            }
            write_uart(ETX);
            // wait
            start_ack_timer();

            while (!receivedAck && !timedOut) {
                if (!BOOT_SELI) {
                    __delay_us(50);
                    if (!BOOT_SELI) {
                        while (!timedOut && !receivedAck) {
                            if (BOOT_SELI)
                                receivedAck = true; //okToReceiveAck = true;      // enables the ability to set receiveAck = true
                        }
                    }
                }
            }
            stop_ack_timer();
            if (receivedAck) {
                receivedAck = false;
                progOK = true;
                h = 10;
            }
            //                    dis_boot_sel_int();
            __delay_ms(10);
        }
        timedOut = !progOK;
    }

    // end outer loop
    // READ PAGE FROM MEMORY
    if (addrL == 0x00) {
        disable_write();
        MEM_SPI_BEGIN();
        send_spi(0x03);
        send_spi(addrL);
        send_spi(addrH);
        send_spi(addrU);
        for (uint16_t g = 0; g < sizeof (memBlock); g++)
            memBlock[g] = send_spi(0xFF);
        MEM_SPI_END();
    }


    if (!timedOut) {
        // PROGRAM PIC
        crcResult = 0;
        add_to_crc(0x04);
        add_to_crc(addrL);
        add_to_crc(addrH);
        add_to_crc(addrU);
        add_to_crc(0x00);
        add_to_crc(0x01); // program 4 pages at once
        for (j = 0; j < 64; j++) // calculate CRC outside of UART write to ensure consistent baud
            add_to_crc(memBlock[(uint16_t) (addrL + j)]); // addrL is always in 64-byte increments
        crcL = (uint8_t) (crcResult & 0x00FF);
        crcH = (uint8_t) ((crcResult & 0xFF00) >> 8);

        // outer loop until BOOT_SEL goes low or times out
        for (uint8_t h = 0; h < 10; h++) {
            // inner loop until BOOT_SEL goes low or times out
            while (!start_tx_ok());
            // end inner loop
            write_uart(0x05);
            write_uart(0x04);
            if (addrL == STX || addrL == ETX || addrL == DLE)
                write_uart(0x05);
            write_uart(addrL);
            if (addrH == STX || addrH == ETX || addrH == DLE)
                write_uart(0x05);
            write_uart(addrH);
            if (addrU == STX || addrU == ETX || addrU == DLE)
                write_uart(0x05);
            write_uart(addrU);
            write_uart(0x00);
            write_uart(0x01);
            for (j = 0; j < 64; j++) {
                if (memBlock[addrL + j] == STX || memBlock[addrL + j] == ETX || memBlock[addrL + j] == DLE)
                    write_uart(0x05);
                write_uart(memBlock[(uint16_t) (addrL + j)]);
            }
            if (crcL == STX || crcL == ETX || crcL == DLE)
                write_uart(0x05);
            write_uart(crcL);
            if (crcH == STX || crcH == ETX || crcH == DLE)
                write_uart(0x05);
            write_uart(crcH);
            write_uart(ETX);
            // wait
            start_ack_timer();
            while (!receivedAck && !timedOut) {
                if (!BOOT_SELI) {
                    __delay_us(50);
                    if (!BOOT_SELI) {
                        while (!timedOut && !receivedAck) {
                            if (BOOT_SELI)
                                receivedAck = true; //okToReceiveAck = true;      // enables the ability to set receiveAck = true
                        }
                    }
                }
            }
            stop_ack_timer();
            if (receivedAck) {
                receivedAck = false;
                progOK = true;
                h = 10;
            }
            //                    dis_boot_sel_int();
            __delay_ms(10);
        }
        timedOut = !progOK;

    } // end outer loop
    return progOK;
}

void tell_hub_close_connection() {
    pulse_boot_sel();
    start_ack_timer();
    t0cnt = 0;
    CLRWDT();
    while (!receivedAck && !timedOut) {
        if (!BOOT_SELI) {
            __delay_us(100);
            if (!BOOT_SELI) {
                while (!timedOut && !receivedAck) {
                    if (BOOT_SELI)
                        receivedAck = true; //okToReceiveAck = true;      // enables the ability to set receiveAck = true
                }
            }
        }
    }
}

void parse_new_data() {
    start_timer();
    
    if (modemChar != '#' && modemChar != '\n' && modemChar != '\n' && modemChar != BREAK_CHAR)
        add_to_crc(modemChar);
    else if (modemChar == '#')
        terminationCharCnt++;
    else if ((lastModemChar == '\n' && modemChar == '\n') && !reachedEOF)
    {
        // TEST***
        doneProcessing = false;
        INTCONbits.GIE = 0;
        ascii_to_mem((uint8_t*) & rawData[0], rdpos);
        //***TEST
        INTCONbits.GIE = 1;
        doneProcessing = true;
        rdpos = 0;
    }
    else
        check_termination();
    if (modemChar != BREAK_CHAR)
        lastModemChar = modemChar;
    else
        write_uart(ACK);
}


void handle_eof()
{
    uint8_t memStatus;
 // crcOK = true;//false;***
    if (crcOK) {
        // ***to be added when hub code is correctly modified
        // tell hub when the data connection can be closed
        prevState = state;
        state = WaitToCloseConnection;
//        nMD_PWR = 1; // turn off this chip's control over modem
    } else {
        //                    send_bad_ota();
        reset_ota();
    }

    memStatus = 0;
    while (memStatus != 0x02) {
        enable_write();
        memStatus = read_status() & 0x03;
    }
}