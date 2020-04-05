//Some functions used by brstm.h, brstm_encode.h and other parts of the Revolution libs
//Copyright (C) 2020 Extrasklep

//Bool endian: 0 = little endian, 1 = big endian

#pragma once

unsigned char* brstm_slice;
char* brstm_slicestring;

long brstm_clamp(long value, long min, long max) {
  return value <= min ? min : value >= max ? max : value;
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
    unsigned int length=2;
    unsigned long number=0;
    unsigned char* bytes=brstm_getSlice(data,start,length);
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

