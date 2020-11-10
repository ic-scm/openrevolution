//Some functions used by brstm.h, brstm_encode.h and other parts of the OpenRevolution libraries
//Copyright (C) 2020 IC

//Bool endian: 0 = little endian, 1 = big endian

#pragma once
#include <math.h>

//TODO: Do not use global variables for slicing function results
unsigned char* brstm_slice;
char* brstm_slicestring;

long brstm_clamp(long value, long min, long max) {
  return value <= min ? min : value >= max ? max : value;
}

long brstm_clamp16(long value) {
  return value <= -32768 ? -32768 : value >= 32767 ? 32767 : value;
}

//Get slice of data
unsigned char* brstm_getSlice(const unsigned char* data,unsigned long start,unsigned long length) {
    delete[] brstm_slice;
    brstm_slice = new unsigned char[length];
    for(unsigned int i=0;i<length;i++) {
        brstm_slice[i]=data[i+start];
    }
    return brstm_slice;
}

//Get slice and convert it to a number
unsigned long brstm_getSliceAsNumber(const unsigned char* data,unsigned long start,unsigned long length,bool endian) {
    if(length>4) {length=4;}
    unsigned long number=0;
    unsigned char* bytes=brstm_getSlice(data,start,length);
    unsigned char pos;
    if(endian) {
        pos=length-1; //Read as big endian
    } else {
        pos = 0; //Read as little endian
    }
    unsigned long pw=1; //Multiply by 1,256,65536...
    for(unsigned int i=0;i<length;i++) {
        if(i>0) {pw*=256;}
        number+=bytes[pos]*pw;
        if(endian) {pos--;} else {pos++;}
    }
    return number;
}

//Get slice as signed 16 bit number
signed int brstm_getSliceAsInt16Sample(const unsigned char * data,unsigned long start,bool endian) {
    unsigned long number=0;
    unsigned char bytes[2]={data[start],data[start+1]};
    unsigned char little=bytes[endian];
    signed   char big=bytes[!endian];
    number=little+big*256;
    return number;
}

//Get slice as a null terminated string
char* brstm_getSliceAsString(const unsigned char* data,unsigned long start,unsigned long length) {
    unsigned char slicestr[length+1];
    unsigned char* bytes=brstm_getSlice(data,start,length);
    for(unsigned int i=0;i<length;i++) {
        slicestr[i]=bytes[i];
        if(slicestr[i]=='\0') {slicestr[i]=' ';}
    }
    slicestr[length]='\0';
    delete[] brstm_slice;
    brstm_slicestring = new char[length+1];
    for(unsigned int i=0;i<length+1;i++) {
        brstm_slicestring[i]=slicestr[i];
    }
    return brstm_slicestring;
}

#define PACKET_NIBBLES 16
#define PACKET_SAMPLES 14
#define PACKET_BYTES 8

unsigned int brstm_getBytesForAdpcmSamples(int samples) {
    int extraBytes = 0;
    int packets = samples / PACKET_SAMPLES;
    int extraSamples = samples % PACKET_SAMPLES;

    if (extraSamples != 0) {
        extraBytes = (extraSamples / 2) + (extraSamples % 2) + 1;
    }

    return PACKET_BYTES * packets + extraBytes;
}

//Encoder utils

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
//TODO: Improve the speed of this function and brstm_encoder_getByteInt16
//      while keeping compatibility with both Little-endian and Big-endian machines
//TODO: Do not use a global variable for storing the result
unsigned char* brstm_encoder_ByteInt;
unsigned char* brstm_encoder_getByteUint(uint64_t num,uint8_t bytes,bool BOM) {
    delete[] brstm_encoder_ByteInt;
    brstm_encoder_ByteInt = new unsigned char[bytes];
    unsigned long pwr;
    unsigned char pwn = bytes-1;
    for(unsigned char i = 0; i < bytes; i++) {
        pwr = pow(256,pwn--);
        unsigned int pos = (BOM ? i : abs(i-bytes+1));
        brstm_encoder_ByteInt[pos]=0;
        while(num >= pwr) {
            brstm_encoder_ByteInt[pos]++;
            num -= pwr;
        }
    }
    return brstm_encoder_ByteInt;
}

unsigned char* brstm_encoder_getByteInt16(int16_t num, bool BOM) {
    return brstm_encoder_getByteUint((uint16_t)num,2,BOM);
}

char brstm_encoder_nextspinner(char& spinner) {
    switch(spinner) {
        case '/':  spinner = '-';  break;
        case '-':  spinner = '\\'; break;
        case '\\': spinner = '|';  break;
        case '|':  spinner = '/';  break;
    }
    return spinner;
}

//Calculates block sizes and sample counts
void brstm_encoder_calculateStandardBlockInfo(Brstm* brstmi) {
    brstmi->blocks_samples = (brstmi->codec == 2 ? 14336 : brstmi->codec == 1 ? 4096 : 8192);
    brstmi->blocks_size = 8192;
    
    brstmi->total_blocks = brstmi->total_samples / brstmi->blocks_samples;
    if(brstmi->total_samples % brstmi->blocks_samples != 0) brstmi->total_blocks++;
    
    //Final block
    brstmi->final_block_samples = brstmi->total_samples % brstmi->blocks_samples;
    if(brstmi->final_block_samples == 0) brstmi->final_block_samples = brstmi->blocks_samples;
    brstmi->final_block_size = (
        brstmi->codec == 2 ? brstm_getBytesForAdpcmSamples(brstmi->final_block_samples)
        : brstmi->codec == 1 ? brstmi->final_block_samples * 2
        : brstmi->final_block_samples
    );
    
    //Padded final block size
    brstmi->final_block_size_p = brstmi->final_block_size;
    while(brstmi->final_block_size_p % 0x20 != 0) {brstmi->final_block_size_p++;}
}

