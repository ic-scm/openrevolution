//C++ BRSTM encoder
//Copyright (C) 2020 Extrasklep
//https://github.com/Extrasklep/brstm

//This file requires the same variables as brstm.h to be declared in the including file

//unsigned char* brstm_encoded_data;
//unsigned long  brstm_encoded_data_size;
//declare in including file

#include <math.h>

void brstm_encoder_writebytes(unsigned char* buf,const unsigned char* data,unsigned int bytes,unsigned long& off) {
    for(unsigned int i=0;i<bytes;i++) {
        buf[i+off] = data[i];
    }
    off += bytes;
}

void brstm_encoder_writebytes_i(unsigned char* buf,unsigned char* data,unsigned int bytes,unsigned long& off) {
    brstm_encoder_writebytes(buf,data,bytes,off);
    delete[] data;
}

void brstm_encoder_writebyte(unsigned char* buf,const unsigned char data,unsigned long& off) {
    unsigned char arr[1] = {data};
    brstm_encoder_writebytes(buf,arr,1,off);
}

//Returns integer as big endian bytes
unsigned char* BEint;
unsigned char* brstm_encoder_getBEuint(uint64_t num,uint8_t bytes) {
    delete[] BEint;
    BEint = new unsigned char[bytes];
    unsigned long pwr;
    unsigned char pwn = bytes-1;
    for(unsigned char i = 0; i < bytes; i++) {
        pwr = pow(256,pwn--);
        while(num >= pwr) {
            BEint[i]++;
            num -= pwr;
        }
    }
    return BEint;
}

unsigned char* brstm_encoder_getBEint16(int16_t num) {
    uint16_t unum = num;
    return brstm_encoder_getBEuint(unum,2);
}

unsigned char brstm_encode() {
    delete[] brstm_encoded_data;
    unsigned char* buffer = new unsigned char[HEAD1_total_samples*HEAD3_num_channels+((HEAD1_total_samples*HEAD3_num_channels/14336)*2)+HEAD3_num_channels*256+8192];
    unsigned long  bufpos = 0;
    
    //Header
    brstm_encoder_writebytes(buffer,(unsigned char*)"RSTM",4,bufpos);
    //Big endian byte order mark and version
    brstm_encoder_writebytes_i(buffer,new unsigned char[4]{0xFE,0xFF,0x01,0x00},4,bufpos);
    //File size (will be actually written later)
    brstm_encoder_writebytes_i(buffer,new unsigned char[4]{0x00,0x00,0x00,0x00},4,bufpos);
    //Header size, number of chunks
    brstm_encoder_writebytes_i(buffer,new unsigned char[4]{0x00,0x40,0x00,0x02},4,bufpos);
    //Sizes and offsets of chunks (will be written later) + 24 byte zero padding
    for(unsigned int i=0;i<12;i++) { brstm_encoder_writebytes_i(buffer,new unsigned char[4]{0x00,0x00,0x00,0x00},4,bufpos); }
    
    //HEAD chunk
    brstm_encoder_writebytes(buffer,(unsigned char*)"HEAD",4,bufpos);
    
    
    //Finalize file (write proper filesize etc)
    //Filesize
    unsigned long off;
    brstm_encoder_writebytes(buffer,brstm_encoder_getBEuint(bufpos,4),4,off=0x08);
    
    //copy finished file to brstm_encoded_data
    brstm_encoded_data = new unsigned char[bufpos];
    memcpy(brstm_encoded_data,buffer,bufpos);
    delete[] buffer;
    brstm_encoded_data_size = bufpos;
    
    return 0;
}
