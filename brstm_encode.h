//C++ BRSTM encoder
//Copyright (C) 2020 Extrasklep
//https://github.com/Extrasklep/brstm

//This file requires the same variables as brstm.h to be declared in the including file

//unsigned char* brstm_encoded_data;
//unsigned long  brstm_encoded_data_size;
//declare in including file

#include <math.h>
#include "dspadpcm_encoder.c"
#define PACKET_NIBBLES 16
#define PACKET_SAMPLES 14
#define PACKET_BYTES 8

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
        BEint[i]=0;
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

char brstm_encoder_nextspinner(char& spinner) {
    switch(spinner) {
        case '/':  spinner = '-';  break;
        case '-':  spinner = '\\'; break;
        case '\\': spinner = '|';  break;
        case '|':  spinner = '/';  break;
    }
    return spinner;
}

unsigned int GetBytesForAdpcmSamples(int samples) {
    int extraBytes = 0;
    int packets = samples / PACKET_SAMPLES;
    int extraSamples = samples % PACKET_SAMPLES;

    if (extraSamples != 0) {
        extraBytes = (extraSamples / 2) + (extraSamples % 2) + 1;
    }

    return PACKET_BYTES * packets + extraBytes;
}

unsigned char brstm_encode() {
    //Check for invalid requests
    //Too many tracks
    if(HEAD2_num_tracks > 8) {
        return 248;
    }
    //Too many channels
    if(HEAD1_num_channels > 16 || HEAD3_num_channels > 16) {
        return 249;
    }
    //Unsupported codec
    if(HEAD1_codec != 2) {
        return 220;
    }
    //Unsupported track description type
    if(!(HEAD2_track_type >= 0 && HEAD2_track_type <= 1)) {
        return 244;
    }
    delete[] brstm_encoded_data;
    unsigned char* buffer = new unsigned char[(HEAD1_total_samples*HEAD3_num_channels)+((HEAD1_total_samples*HEAD3_num_channels/14336)*4)+HEAD3_num_channels*256+8192];
    unsigned long  bufpos = 0;
    unsigned long  off; //for argument 4 of brstm_encoder_writebytes
    
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
    unsigned int HEADchunkoffset = bufpos;
    brstm_encoder_writebytes(buffer,(unsigned char*)"HEAD",4,bufpos);
    //HEAD chunk size (will be written later)
    brstm_encoder_writebytes_i(buffer,new unsigned char[4]{0x00,0x00,0x00,0x00},4,bufpos);
    //HEAD chunk header
    for(unsigned int i=0;i<3;i++) {
        brstm_encoder_writebytes_i(buffer,new unsigned char[4]{0x01,0x00,0x00,0x00},4,bufpos); //Marker
        brstm_encoder_writebytes_i(buffer,new unsigned char[4]{0x00,0x00,0x00,0x00},4,bufpos); //HEAD chunk part offsets (will be written later)
    }
    
    //HEAD1
    //Write HEAD1 offset to HEAD header
    unsigned int HEAD1offset = bufpos - HEADchunkoffset - 8;
    brstm_encoder_writebytes(buffer,brstm_encoder_getBEuint(HEAD1offset,4),4,off=HEADchunkoffset+0x0C);
    //HEAD1 data
    brstm_encoder_writebyte(buffer,HEAD1_codec = 2,bufpos); //Support for other codecs in the future maybe?
    brstm_encoder_writebyte(buffer,HEAD1_loop,bufpos);
    brstm_encoder_writebyte(buffer,HEAD1_num_channels,bufpos);
    brstm_encoder_writebyte(buffer,0,bufpos); //padding
    brstm_encoder_writebytes(buffer,brstm_encoder_getBEuint(HEAD1_sample_rate,2),2,bufpos);
    brstm_encoder_writebytes_i(buffer,new unsigned char[2]{0x00,0x00},2,bufpos); //padding
    brstm_encoder_writebytes(buffer,brstm_encoder_getBEuint(HEAD1_loop_start,4),4,bufpos);
    brstm_encoder_writebytes(buffer,brstm_encoder_getBEuint(HEAD1_total_samples,4),4,bufpos);
    brstm_encoder_writebytes(buffer,brstm_encoder_getBEuint(0,4),4,bufpos); //ADPCM offset, will be written later
    HEAD1_total_blocks = HEAD1_total_samples / 14336;
    if(HEAD1_total_samples % 14336 != 0) HEAD1_total_blocks++;
    brstm_encoder_writebytes(buffer,brstm_encoder_getBEuint(HEAD1_total_blocks,4),4,bufpos);
    brstm_encoder_writebytes(buffer,brstm_encoder_getBEuint(HEAD1_blocks_size = 8192,4),4,bufpos);
    brstm_encoder_writebytes(buffer,brstm_encoder_getBEuint(HEAD1_blocks_samples = 14336,4),4,bufpos);
    HEAD1_final_block_samples = HEAD1_total_samples % 14336;
    brstm_encoder_writebytes(buffer,brstm_encoder_getBEuint(HEAD1_final_block_size = HEAD1_final_block_samples / 1.75 + 2,4),4,bufpos); //Final block size
    brstm_encoder_writebytes(buffer,brstm_encoder_getBEuint(HEAD1_final_block_samples,4),4,bufpos); 
    HEAD1_final_block_size_p = HEAD1_final_block_size;
    while(HEAD1_final_block_size_p % 16 != 0) {HEAD1_final_block_size_p++;}
    brstm_encoder_writebytes(buffer,brstm_encoder_getBEuint(HEAD1_final_block_size_p,4),4,bufpos); //Padded final block size
    brstm_encoder_writebytes(buffer,brstm_encoder_getBEuint(14336,4),4,bufpos);  //ADPC samples per entry
    brstm_encoder_writebytes(buffer,brstm_encoder_getBEuint(4,4),4,bufpos); //ADPC bytes per entry
    
    //HEAD2
    //Write HEAD2 offset to HEAD header
    unsigned int HEAD2offset = bufpos - HEADchunkoffset - 8;
    brstm_encoder_writebytes(buffer,brstm_encoder_getBEuint(HEAD2offset,4),4,off=HEADchunkoffset+0x14);
    //HEAD2 header
    brstm_encoder_writebyte(buffer,HEAD2_num_tracks,bufpos);
    brstm_encoder_writebyte(buffer,HEAD2_track_type,bufpos);
    brstm_encoder_writebytes_i(buffer,new unsigned char[2]{0x00,0x00},2,bufpos); //padding
    //offset table
    unsigned long HEAD2_track_info_offsets[8] = {0,0,0,0,0,0,0,0};
    for(unsigned int i=0;i<HEAD2_num_tracks;i++) {
        brstm_encoder_writebyte(buffer,1,bufpos);
        brstm_encoder_writebyte(buffer,HEAD2_track_type,bufpos);
        brstm_encoder_writebytes_i(buffer,new unsigned char[2]{0x00,0x00},2,bufpos); //padding
        brstm_encoder_writebytes_i(buffer,new unsigned char[4]{0x00,0x00,0x00,0x00},4,bufpos); //Offset to track description, will be written later from HEAD2_track_info_offsets
    }
    //track descriptions
    for(unsigned int i=0;i<HEAD2_num_tracks;i++) {
        //write offset to offset table
        HEAD2_track_info_offsets[i] = bufpos - HEADchunkoffset - 8;
        brstm_encoder_writebytes(buffer,brstm_encoder_getBEuint(HEAD2_track_info_offsets[i],4),4,off=HEADchunkoffset + HEAD2offset + 12 + 8*i + 4);
        //write additional type 1 data
        if(HEAD2_track_type == 1) {
            brstm_encoder_writebyte(buffer,HEAD2_track_volume[i],bufpos);
            brstm_encoder_writebyte(buffer,HEAD2_track_panning[i],bufpos);
            brstm_encoder_writebytes_i(buffer,new unsigned char[6]{0x00,0x00,0x00,0x00,0x00,0x00},6,bufpos); //padding
        }
        //standard data
        brstm_encoder_writebyte(buffer,HEAD2_track_num_channels[i],bufpos);
        brstm_encoder_writebyte(buffer,HEAD2_track_lchannel_id [i],bufpos);
        brstm_encoder_writebyte(buffer,HEAD2_track_rchannel_id [i],bufpos);
        brstm_encoder_writebyte(buffer,0,bufpos); //padding
    }
    
    //HEAD3
    HEAD3_num_channels = HEAD1_num_channels;
    int16_t  HEAD3_int16_adpcm  [16][16];
    int16_t* ADPC_hsamples_1    [16];
    int16_t* ADPC_hsamples_2    [16];
    //Write HEAD3 offset to HEAD header
    unsigned int HEAD3offset = bufpos - HEADchunkoffset - 8;
    brstm_encoder_writebytes(buffer,brstm_encoder_getBEuint(HEAD3offset,4),4,off=HEADchunkoffset+0x1C);
    //HEAD3 header
    brstm_encoder_writebyte(buffer,HEAD3_num_channels,bufpos);
    brstm_encoder_writebytes_i(buffer,new unsigned char[3]{0x00,0x00,0x00},3,bufpos); //padding
    //offset table
    unsigned long HEAD3_ch_info_offsets  [16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    for(unsigned int i=0;i<HEAD3_num_channels;i++) {
        brstm_encoder_writebytes_i(buffer,new unsigned char[4]{0x01,0x00,0x00,0x00},4,bufpos); //Marker
        brstm_encoder_writebytes_i(buffer,new unsigned char[4]{0x00,0x00,0x00,0x00},4,bufpos); //Offset to channel information, will be written later from HEAD3_ch_info_offsets
    }
    //channel info
    for(unsigned int i=0;i<HEAD3_num_channels;i++) {
        //write offset to offset table
        HEAD3_ch_info_offsets[i] = bufpos - HEADchunkoffset - 8;
        brstm_encoder_writebytes(buffer,brstm_encoder_getBEuint(HEAD3_ch_info_offsets[i],4),4,off=HEADchunkoffset + HEAD3offset + 12 + 8*i + 4);
        //write channel info
        brstm_encoder_writebytes_i(buffer,new unsigned char[4]{0x01,0x00,0x00,0x00},4,bufpos); //Marker
        brstm_encoder_writebytes_i(buffer,new unsigned char[4]{0x00,0x00,0x00,0x00},4,bufpos); //Offset to ADPCM coefs?
        //Calculate coefs
        DSPCorrelateCoefs(PCM_samples[i],HEAD1_total_samples,HEAD3_int16_adpcm[i]);
        //Write coefs
        for(unsigned int a=0;a<16;a++) {
            brstm_encoder_writebytes(buffer,brstm_encoder_getBEint16(HEAD3_int16_adpcm[i][a]),2,bufpos);
        }
        brstm_encoder_writebytes_i(buffer,new unsigned char[2]{0x00,0x00},2,bufpos); //Gain, always zero
        brstm_encoder_writebytes_i(buffer,new unsigned char[2]{0x00,0x00},2,bufpos); //Initial scale, will be written later
        brstm_encoder_writebytes_i(buffer,new unsigned char[2]{0x00,0x00},2,bufpos); //HS1, always zero
        brstm_encoder_writebytes_i(buffer,new unsigned char[2]{0x00,0x00},2,bufpos); //HS2, always zero
        brstm_encoder_writebytes_i(buffer,new unsigned char[2]{0x00,0x00},2,bufpos); //Loop predictor scale, will be written later
        brstm_encoder_writebytes  (buffer,brstm_encoder_getBEint16( HEAD1_loop_start > 0 ? PCM_samples[i][HEAD1_loop_start-1] : 0 ),2,bufpos); //Loop HS1
        brstm_encoder_writebytes  (buffer,brstm_encoder_getBEint16( HEAD1_loop_start > 1 ? PCM_samples[i][HEAD1_loop_start-2] : 0 ),2,bufpos); //Loop HS2
        brstm_encoder_writebytes_i(buffer,new unsigned char[2]{0x00,0x00},2,bufpos); //Padding
    }
    
    
    unsigned int HEADchunksize = bufpos - HEADchunkoffset;
    //Padding
    while(bufpos % 16 != 0) {
        brstm_encoder_writebyte(buffer,0,bufpos);
        HEADchunksize = bufpos - HEADchunkoffset;
    }
    //Write HEAD chunk length
    brstm_encoder_writebytes(buffer,brstm_encoder_getBEuint(HEADchunksize,4),4,off=HEADchunkoffset+4);
    
    
    //ADPC chunk
    unsigned int ADPCchunkoffset = bufpos;
    brstm_encoder_writebytes(buffer,(unsigned char*)"ADPC",4,bufpos);
    //ADPC chunk size (will be written later)
    brstm_encoder_writebytes_i(buffer,new unsigned char[4]{0x00,0x00,0x00,0x00},4,bufpos);
    //Write dummy ADPC data because we don't have any audio yet
    {
        unsigned long l = HEAD3_num_channels * HEAD1_total_blocks;
        for(unsigned long i=0;i<l;i++) {brstm_encoder_writebytes_i(buffer,new unsigned char[4]{0x00,0x00,0x00,0x00},4,bufpos);}
    }
    unsigned int ADPCchunksize = bufpos - ADPCchunkoffset;
    //Padding
    while(bufpos % 16 != 0) {
        brstm_encoder_writebyte(buffer,0,bufpos);
        ADPCchunksize = bufpos - ADPCchunkoffset;
    }
    //Write ADPC chunk length
    brstm_encoder_writebytes(buffer,brstm_encoder_getBEuint(ADPCchunksize,4),4,off=ADPCchunkoffset+4);
    
    
    //DATA chunk
    unsigned int DATAchunkoffset = bufpos;
    brstm_encoder_writebytes(buffer,(unsigned char*)"DATA",4,bufpos);
    //DATA chunk size (will be written later)
    brstm_encoder_writebytes_i(buffer,new unsigned char[4]{0x00,0x00,0x00,0x00},4,bufpos);
    //Padding
    brstm_encoder_writebytes_i(buffer,new unsigned char[4]{0x00,0x00,0x00,0x18},4,bufpos);
    for(unsigned int i=0;i<5;i++) {brstm_encoder_writebytes_i(buffer,new unsigned char[4]{0x00,0x00,0x00,0x00},4,bufpos);}
    
    //Encode ADPCM for each channel
    unsigned char* ADPCMdata[16];
    unsigned long  ADPCMdataPos[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    char spinner = '/';
    for(unsigned char c=0;c<HEAD3_num_channels;c++) {
        ADPCMdata[c] = new unsigned char[HEAD1_total_samples];
        //Store coefs like this for the DSPEncodeFrame function
        int16_t coefs[8][2]; for(unsigned int i=0;i<16;i++) {coefs[i/2][i%2] = HEAD3_int16_adpcm[c][i];}
        
        unsigned long packetCount = HEAD1_total_samples / PACKET_SAMPLES + (HEAD1_total_samples % PACKET_SAMPLES != 0);
        int16_t convSamps[16] = {0};
        unsigned char block[8];
        for (unsigned long p=0;p<packetCount;p++) {
            memset(convSamps + 2, 0, PACKET_SAMPLES * sizeof(int16_t));
            int numSamples = MIN(HEAD1_total_samples - p * PACKET_SAMPLES, PACKET_SAMPLES);
            for (unsigned int s=0; s<numSamples; ++s)
                convSamps[s+2] = PCM_samples[c][p*PACKET_SAMPLES+s];
            
            DSPEncodeFrame(convSamps, PACKET_SAMPLES, block, coefs);
            
            convSamps[0] = convSamps[14];
            convSamps[1] = convSamps[15];
            
            brstm_encoder_writebytes(ADPCMdata[c],block,GetBytesForAdpcmSamples(numSamples),ADPCMdataPos[c]);
            
            //console output
            if (!(p%512)) std::cout << "\r" << brstm_encoder_nextspinner(spinner) << " Encoding DSPADPCM data... (CH " << (unsigned int)c << " " << floor(((float)p/packetCount) * 100) << "%)          ";
        }
        std::cout << "\r" << brstm_encoder_nextspinner(spinner) << " Encoding DSPADPCM data... (CH " << (unsigned int)c << " 100%)          \n";
    }
    //Write APDCM data
    for(unsigned long b=0;b<HEAD1_total_blocks-1;b++) {
        for(unsigned int c=0;c<HEAD3_num_channels;c++) {
            for(unsigned int i=0;i<HEAD1_blocks_size;i++) {
                brstm_encoder_writebyte(buffer,ADPCMdata[c][i+b*HEAD1_blocks_size],bufpos);
            }
        }
    }
    //Final block
    for(unsigned int c=0;c<HEAD3_num_channels;c++) {
        unsigned int i;
        for(i=0;i<HEAD1_final_block_size;i++) {
            brstm_encoder_writebyte(buffer,ADPCMdata[c][i+(HEAD1_total_blocks-1)*HEAD1_blocks_size],bufpos);
        }
        //padding
        while(i<HEAD1_final_block_size_p) {
            brstm_encoder_writebyte(buffer,0x00,bufpos);
            i++;
        }
    }
    
    unsigned int DATAchunksize = bufpos - DATAchunkoffset;
    //Write DATA chunk length
    brstm_encoder_writebytes(buffer,brstm_encoder_getBEuint(DATAchunksize,4),4,off=DATAchunkoffset+4);
    
    
    //Finalize file (write proper filesize etc)
    //Filesize
    brstm_encoder_writebytes(buffer,brstm_encoder_getBEuint(bufpos,4),4,off=0x08);
    //HEAD offset and size
    brstm_encoder_writebytes(buffer,brstm_encoder_getBEuint(HEADchunkoffset,4),4,off=0x10);
    brstm_encoder_writebytes(buffer,brstm_encoder_getBEuint(HEADchunksize,4),4,off=0x14);
    //ADPC offset and size
    brstm_encoder_writebytes(buffer,brstm_encoder_getBEuint(ADPCchunkoffset,4),4,off=0x18);
    brstm_encoder_writebytes(buffer,brstm_encoder_getBEuint(ADPCchunksize,4),4,off=0x1C);
    //DATA offset and size
    brstm_encoder_writebytes(buffer,brstm_encoder_getBEuint(DATAchunkoffset,4),4,off=0x20);
    brstm_encoder_writebytes(buffer,brstm_encoder_getBEuint(DATAchunksize,4),4,off=0x24);
    //ADPCM offset in HEAD1
    brstm_encoder_writebytes(buffer,brstm_encoder_getBEuint(DATAchunkoffset+0x20,4),4,off=HEADchunkoffset + 8 + HEAD1offset + 0x10);
    
    //copy finished file to brstm_encoded_data
    brstm_encoded_data = new unsigned char[bufpos];
    memcpy(brstm_encoded_data,buffer,bufpos);
    delete[] buffer;
    brstm_encoded_data_size = bufpos;
    
    return 0;
}
