//C++ BRSTM encoder
//Copyright (C) 2020 Extrasklep
//https://github.com/Extrasklep/brstm

//This file requires brstm.h to be included too

#include <math.h>
#include "dspadpcm_encoder.c"
#define PACKET_NIBBLES 16
#define PACKET_SAMPLES 14
#define PACKET_BYTES 8

//Utils
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
unsigned char* brstm_encoder_BEint;
unsigned char* brstm_encoder_getBEuint(uint64_t num,uint8_t bytes) {
    delete[] brstm_encoder_BEint;
    brstm_encoder_BEint = new unsigned char[bytes];
    unsigned long pwr;
    unsigned char pwn = bytes-1;
    for(unsigned char i = 0; i < bytes; i++) {
        pwr = pow(256,pwn--);
        brstm_encoder_BEint[i]=0;
        while(num >= pwr) {
            brstm_encoder_BEint[i]++;
            num -= pwr;
        }
    }
    return brstm_encoder_BEint;
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

unsigned int brstm_encoder_GetBytesForAdpcmSamples(int samples) {
    int extraBytes = 0;
    int packets = samples / PACKET_SAMPLES;
    int extraSamples = samples % PACKET_SAMPLES;

    if (extraSamples != 0) {
        extraBytes = (extraSamples / 2) + (extraSamples % 2) + 1;
    }

    return PACKET_BYTES * packets + extraBytes;
}

/* 
 * Build a BRSTM file and encode audio data
 * 
 * brstmi: Your BRSTM struct pointer
 * debugLevel:
 *    -1 = Never output anything
 *     0 = Only output errors/warnings
 *     1 = Log encoding progress
 * encodeADPCM:
 *     0 = Use ADPCM data from ADPCM_data
 *     1 = Encode PCM_samples to ADPCM
 * 
 * Returns error code (>127) or warning code (<128):
 *        0 = No error
 *      249 = Too many channels
 *      248 = Too many tracks
 *      244 = Unknown track info type
 *      220 = Unsupported or unknown audio codec
 *      200 = Unknown error (this should never happen)
 * 
 * Write your audio data in PCM_samples and other BRSTM header information in the brstm.h variables, more info in README
 * The created file will be in brstm_encoded_data with size brstm_encoded_data_size.
 */
unsigned char brstm_encode(Brstm* brstmi,signed int debugLevel,uint8_t encodeADPCM) {
    //Check for invalid requests
    //Too many tracks
    if(brstmi->num_tracks > 8) {
        if(debugLevel>=0) std::cout << "Too many tracks. Max supported is 8.\n";
        return 248;
    }
    //Too many channels
    if(brstmi->num_channels > 16) {
        if(debugLevel>=0) std::cout << "Too many channels. Max supported is 16.\n";
        return 249;
    }
    //Unsupported codec
    if(brstmi->codec != 2) {
        if(debugLevel>=0) std::cout << "Unsupported codec.\n";
        return 220;
    }
    //Unsupported track description type
    if(!(brstmi->track_desc_type >= 0 && brstmi->track_desc_type <= 1)) {
        if(debugLevel>=0) std::cout << "Invalid track description type.\n";
        return 244;
    }
    char spinner = '/';
    
    if(debugLevel>0) std::cout << "\r" << brstm_encoder_nextspinner(spinner) << " Starting BRSTM encode...                ";
    
    delete[] brstmi->encoded_file;
    unsigned char* buffer = new unsigned char[(brstmi->total_samples*brstmi->num_channels)+((brstmi->total_samples*brstmi->num_channels/14336)*4)+brstmi->num_channels*256+8192];
    unsigned long  bufpos = 0;
    unsigned long  off; //for argument 4 of brstm_encoder_writebytes when we don't write to the end of the buffer
    
    
    if(debugLevel>0) std::cout << "\r" << brstm_encoder_nextspinner(spinner) << " Building headers... (RSTM)             ";
    
    
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
    
    
    if(debugLevel>0) std::cout << "\r" << brstm_encoder_nextspinner(spinner) << " Building headers... (HEAD)             ";
    
    
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
    brstm_encoder_writebyte(buffer,brstmi->codec = 2,bufpos); //Support for other codecs in the future maybe?
    brstm_encoder_writebyte(buffer,brstmi->loop_flag,bufpos);
    brstm_encoder_writebyte(buffer,brstmi->num_channels,bufpos);
    brstm_encoder_writebyte(buffer,0,bufpos); //padding
    brstm_encoder_writebytes(buffer,brstm_encoder_getBEuint(brstmi->sample_rate,2),2,bufpos);
    brstm_encoder_writebytes_i(buffer,new unsigned char[2]{0x00,0x00},2,bufpos); //padding
    brstm_encoder_writebytes(buffer,brstm_encoder_getBEuint(brstmi->loop_start,4),4,bufpos);
    brstm_encoder_writebytes(buffer,brstm_encoder_getBEuint(brstmi->total_samples,4),4,bufpos);
    brstm_encoder_writebytes(buffer,brstm_encoder_getBEuint(0,4),4,bufpos); //Audio offset, will be written later
    brstmi->total_blocks = brstmi->total_samples / 14336;
    if(brstmi->total_samples % 14336 != 0) brstmi->total_blocks++;
    brstm_encoder_writebytes(buffer,brstm_encoder_getBEuint(brstmi->total_blocks,4),4,bufpos);
    brstm_encoder_writebytes(buffer,brstm_encoder_getBEuint(brstmi->blocks_size = 8192,4),4,bufpos);
    brstm_encoder_writebytes(buffer,brstm_encoder_getBEuint(brstmi->blocks_samples = 14336,4),4,bufpos);
    brstmi->final_block_samples = brstmi->total_samples % 14336;
    brstm_encoder_writebytes(buffer,brstm_encoder_getBEuint(brstmi->final_block_size = brstmi->final_block_samples / 1.75 + 2,4),4,bufpos); //Final block size
    brstm_encoder_writebytes(buffer,brstm_encoder_getBEuint(brstmi->final_block_samples,4),4,bufpos); 
    brstmi->final_block_size_p = brstmi->final_block_size;
    while(brstmi->final_block_size_p % 16 != 0) {brstmi->final_block_size_p++;}
    brstm_encoder_writebytes(buffer,brstm_encoder_getBEuint(brstmi->final_block_size_p,4),4,bufpos); //Padded final block size
    brstm_encoder_writebytes(buffer,brstm_encoder_getBEuint(14336,4),4,bufpos);  //ADPC samples per entry
    brstm_encoder_writebytes(buffer,brstm_encoder_getBEuint(4,4),4,bufpos); //ADPC bytes per entry
    
    //HEAD2
    //Write HEAD2 offset to HEAD header
    unsigned int HEAD2offset = bufpos - HEADchunkoffset - 8;
    brstm_encoder_writebytes(buffer,brstm_encoder_getBEuint(HEAD2offset,4),4,off=HEADchunkoffset+0x14);
    //HEAD2 header
    brstm_encoder_writebyte(buffer,brstmi->num_tracks,bufpos);
    brstm_encoder_writebyte(buffer,brstmi->track_desc_type,bufpos);
    brstm_encoder_writebytes_i(buffer,new unsigned char[2]{0x00,0x00},2,bufpos); //padding
    //offset table
    unsigned long HEAD2_track_info_offsets[8] = {0,0,0,0,0,0,0,0};
    for(unsigned int i=0;i<brstmi->num_tracks;i++) {
        brstm_encoder_writebyte(buffer,1,bufpos);
        brstm_encoder_writebyte(buffer,brstmi->track_desc_type,bufpos);
        brstm_encoder_writebytes_i(buffer,new unsigned char[2]{0x00,0x00},2,bufpos); //padding
        brstm_encoder_writebytes_i(buffer,new unsigned char[4]{0x00,0x00,0x00,0x00},4,bufpos); //Offset to track description, will be written later from HEAD2_track_info_offsets
    }
    //track descriptions
    for(unsigned int i=0;i<brstmi->num_tracks;i++) {
        //write offset to offset table
        HEAD2_track_info_offsets[i] = bufpos - HEADchunkoffset - 8;
        brstm_encoder_writebytes(buffer,brstm_encoder_getBEuint(HEAD2_track_info_offsets[i],4),4,off=HEADchunkoffset + HEAD2offset + 12 + 8*i + 4);
        //write additional type 1 data
        if(brstmi->track_desc_type == 1) {
            brstm_encoder_writebyte(buffer,brstmi->track_volume[i],bufpos);
            brstm_encoder_writebyte(buffer,brstmi->track_panning[i],bufpos);
            brstm_encoder_writebytes_i(buffer,new unsigned char[6]{0x00,0x00,0x00,0x00,0x00,0x00},6,bufpos); //padding
        }
        //standard data
        brstm_encoder_writebyte(buffer,brstmi->track_num_channels[i],bufpos);
        brstm_encoder_writebyte(buffer,brstmi->track_lchannel_id [i],bufpos);
        brstm_encoder_writebyte(buffer,brstmi->track_rchannel_id [i],bufpos);
        brstm_encoder_writebyte(buffer,0,bufpos); //padding
    }
    
    //History samples (will be needed for HEAD3)
    int16_t LoopHS1[brstmi->num_channels];
    int16_t LoopHS2[brstmi->num_channels];
    int16_t HS1[brstmi->num_channels][brstmi->total_blocks];
    int16_t HS2[brstmi->num_channels][brstmi->total_blocks];
    if(encodeADPCM) {
        //Get history samples
        for(unsigned char c=0;c<brstmi->num_channels;c++) {
            LoopHS1[c] = brstmi->loop_start > 0 ? brstmi->PCM_samples[c][brstmi->loop_start-1] : 0;
            LoopHS2[c] = brstmi->loop_start > 1 ? brstmi->PCM_samples[c][brstmi->loop_start-2] : 0;
            for(unsigned long b=0;b<brstmi->total_blocks;b++) {
                if(b==0) {
                    //First block history samples are always zero
                    HS1[c][b] = 0;
                    HS2[c][b] = 0;
                    continue;
                }
                HS1[c][b] = brstmi->PCM_samples[c][(b*brstmi->blocks_samples)-1];
                HS2[c][b] = brstmi->PCM_samples[c][(b*brstmi->blocks_samples)-2];
            }
        }
    } else {
        if(debugLevel>0) std::cout << "\r" << brstm_encoder_nextspinner(spinner) << " Decoding ADPCM to get history samples...            ";
        
        for(unsigned char c=0;c<brstmi->num_channels;c++) {
            //Decode 4 bit ADPCM
            unsigned char* blockData = brstmi->ADPCM_data[c];
            unsigned long currentBlockSamples = brstmi->total_samples;
            unsigned long b = 0;
            unsigned long currentSample = 0;
            
            //Magic adapted from brawllib's ADPCMState.cs
            signed int 
            cps = blockData[0],
            cyn1 = 0,
            cyn2 = 0;
            unsigned long dataIndex = 0;
            
            int16_t* coefs = brstmi->ADPCM_coefs[c];
            
            for (unsigned long sampleIndex=0;sampleIndex<currentBlockSamples;) {
                long outSample = 0;
                if (sampleIndex % 14 == 0) {
                    cps = blockData[dataIndex++];
                }
                if ((sampleIndex++ & 1) == 0) {
                    outSample = blockData[dataIndex] >> 4;
                } else {
                    outSample = blockData[dataIndex++] & 0x0f;
                }
                if (outSample >= 8) {
                    outSample -= 16;
                }
                const long scale = 1 << (cps & 0x0f);
                const long cIndex = (cps >> 4) << 1;
                
                outSample = (0x400 + ((scale * outSample) << 11) + coefs[brstm_clamp(cIndex, 0, 15)] * cyn1 + coefs[brstm_clamp(cIndex + 1, 0, 15)] * cyn2) >> 11;
                
                if(currentSample % brstmi->blocks_samples == 0) {
                    if(b == 0) {
                        HS1[c][b] = 0;
                        HS2[c][b] = 0;
                    } else {
                        HS1[c][b] = cyn1;
                        HS2[c][b] = cyn2;
                    }
                    b++;
                }
                if(currentSample == brstmi->loop_start) {
                    if(currentSample > 0) {LoopHS1[c] = cyn1;} else {LoopHS1[c] = 0;}
                    if(currentSample > 1) {LoopHS2[c] = cyn2;} else {LoopHS2[c] = 0;}
                }
                
                cyn2 = cyn1;
                cyn1 = brstm_clamp(outSample, -32768, 32767);
                
                currentSample++;
            }
        }
        
        if(debugLevel>0) std::cout << "\r" << brstm_encoder_nextspinner(spinner) << " Building headers... (HEAD)                          ";
    }
    
    //HEAD3
    //Write HEAD3 offset to HEAD header
    unsigned int HEAD3offset = bufpos - HEADchunkoffset - 8;
    brstm_encoder_writebytes(buffer,brstm_encoder_getBEuint(HEAD3offset,4),4,off=HEADchunkoffset+0x1C);
    //HEAD3 header
    brstm_encoder_writebyte(buffer,brstmi->num_channels,bufpos);
    brstm_encoder_writebytes_i(buffer,new unsigned char[3]{0x00,0x00,0x00},3,bufpos); //padding
    //offset table
    unsigned long HEAD3_ch_info_offsets  [16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    for(unsigned int i=0;i<brstmi->num_channels;i++) {
        brstm_encoder_writebytes_i(buffer,new unsigned char[4]{0x01,0x00,0x00,0x00},4,bufpos); //Marker
        brstm_encoder_writebytes_i(buffer,new unsigned char[4]{0x00,0x00,0x00,0x00},4,bufpos); //Offset to channel information, will be written later from HEAD3_ch_info_offsets
    }
    //channel info
    for(unsigned int i=0;i<brstmi->num_channels;i++) {
        //write offset to offset table
        HEAD3_ch_info_offsets[i] = bufpos - HEADchunkoffset - 8;
        brstm_encoder_writebytes(buffer,brstm_encoder_getBEuint(HEAD3_ch_info_offsets[i],4),4,off=HEADchunkoffset + HEAD3offset + 12 + 8*i + 4);
        //write channel info
        brstm_encoder_writebytes_i(buffer,new unsigned char[4]{0x01,0x00,0x00,0x00},4,bufpos); //Marker
        brstm_encoder_writebytes  (buffer,brstm_encoder_getBEuint(bufpos - HEADchunkoffset - 4,4),4,bufpos); //Offset to ADPCM coefs?
        //Calculate coefs
        if(encodeADPCM == 1) DSPCorrelateCoefs(brstmi->PCM_samples[i],brstmi->total_samples,brstmi->ADPCM_coefs[i]);
        //Write coefs
        for(unsigned int a=0;a<16;a++) {
            brstm_encoder_writebytes(buffer,brstm_encoder_getBEint16(brstmi->ADPCM_coefs[i][a]),2,bufpos);
        }
        brstm_encoder_writebytes_i(buffer,new unsigned char[2]{0x00,0x00},2,bufpos); //Gain, always zero
        brstm_encoder_writebytes_i(buffer,new unsigned char[2]{0x00,0x00},2,bufpos); //Initial scale, will be written later
        brstm_encoder_writebytes_i(buffer,new unsigned char[2]{0x00,0x00},2,bufpos); //HS1, always zero
        brstm_encoder_writebytes_i(buffer,new unsigned char[2]{0x00,0x00},2,bufpos); //HS2, always zero
        brstm_encoder_writebytes_i(buffer,new unsigned char[2]{0x00,0x00},2,bufpos); //Loop predictor scale, will be written later
        brstm_encoder_writebytes  (buffer,brstm_encoder_getBEint16(LoopHS1[i]),2,bufpos); //Loop HS1
        brstm_encoder_writebytes  (buffer,brstm_encoder_getBEint16(LoopHS2[i]),2,bufpos); //Loop HS2
        brstm_encoder_writebytes_i(buffer,new unsigned char[2]{0x00,0x00},2,bufpos); //Padding
    }
    
    unsigned int HEADchunksize = bufpos - HEADchunkoffset;
    //Padding
    brstm_encoder_writebytes_i(buffer,new unsigned char[6]{0x00,0x00,0x00,0x00,0x00,0x00},6,bufpos); //Write some useless padding because all the BRSTM encoders seem to do that and some BRSTM readers don't work without it
    while(bufpos % 16 != 0) {
        brstm_encoder_writebyte(buffer,0,bufpos);
        HEADchunksize = bufpos - HEADchunkoffset;
    }
    //Write HEAD chunk length
    brstm_encoder_writebytes(buffer,brstm_encoder_getBEuint(HEADchunksize,4),4,off=HEADchunkoffset+4);
    
    
    if(debugLevel>0) std::cout << "\r" << brstm_encoder_nextspinner(spinner) << " Building headers... (ADPC)             ";
    
    
    //ADPC chunk
    unsigned int ADPCchunkoffset = bufpos;
    brstm_encoder_writebytes(buffer,(unsigned char*)"ADPC",4,bufpos);
    //ADPC chunk size (will be written later)
    brstm_encoder_writebytes_i(buffer,new unsigned char[4]{0x00,0x00,0x00,0x00},4,bufpos);
    //Write ADPC history samples
    for(unsigned long b=0;b<brstmi->total_blocks;b++) {
        for(unsigned char c=0;c<brstmi->num_channels;c++) {
            brstm_encoder_writebytes(buffer,brstm_encoder_getBEint16(HS1[c][b]),2,bufpos); //HS1
            brstm_encoder_writebytes(buffer,brstm_encoder_getBEint16(HS2[c][b]),2,bufpos); //HS2
        }
    }
    unsigned int ADPCchunksize = bufpos - ADPCchunkoffset;
    //Padding
    while(bufpos % 16 != 0) {
        brstm_encoder_writebyte(buffer,0,bufpos);
        ADPCchunksize = bufpos - ADPCchunkoffset;
    }
    //Write ADPC chunk length
    brstm_encoder_writebytes(buffer,brstm_encoder_getBEuint(ADPCchunksize,4),4,off=ADPCchunkoffset+4);
    
    
    if(debugLevel>0) std::cout << "\r" << brstm_encoder_nextspinner(spinner) << " Building headers... (DATA)             ";
    
    
    //DATA chunk
    unsigned int DATAchunkoffset = bufpos;
    brstm_encoder_writebytes(buffer,(unsigned char*)"DATA",4,bufpos);
    //DATA chunk size (will be written later)
    brstm_encoder_writebytes_i(buffer,new unsigned char[4]{0x00,0x00,0x00,0x00},4,bufpos);
    //Padding
    brstm_encoder_writebytes_i(buffer,new unsigned char[4]{0x00,0x00,0x00,0x18},4,bufpos);
    for(unsigned int i=0;i<5;i++) {brstm_encoder_writebytes_i(buffer,new unsigned char[4]{0x00,0x00,0x00,0x00},4,bufpos);}
    
    unsigned char** ADPCMdata;
    unsigned long   ADPCMdataPos[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    
    if(encodeADPCM == 1) {
        ADPCMdata = new unsigned char* [16];
        //Encode ADPCM for each channel
        for(unsigned char c=0;c<brstmi->num_channels;c++) {
            ADPCMdata[c] = new unsigned char[brstmi->total_samples];
            //Store coefs like this for the DSPEncodeFrame function
            int16_t coefs[8][2]; for(unsigned int i=0;i<16;i++) {coefs[i/2][i%2] = brstmi->ADPCM_coefs[c][i];}
            
            unsigned long packetCount = brstmi->total_samples / PACKET_SAMPLES + (brstmi->total_samples % PACKET_SAMPLES != 0);
            int16_t convSamps[16] = {0};
            unsigned char block[8];
            for (unsigned long p=0;p<packetCount;p++) {
                memset(convSamps + 2, 0, PACKET_SAMPLES * sizeof(int16_t));
                int numSamples = MIN(brstmi->total_samples - p * PACKET_SAMPLES, PACKET_SAMPLES);
                for (unsigned int s=0; s<numSamples; ++s)
                    convSamps[s+2] = brstmi->PCM_samples[c][p*PACKET_SAMPLES+s];
                
                DSPEncodeFrame(convSamps, PACKET_SAMPLES, block, coefs);
                
                convSamps[0] = convSamps[14];
                convSamps[1] = convSamps[15];
                
                brstm_encoder_writebytes(ADPCMdata[c],block,brstm_encoder_GetBytesForAdpcmSamples(numSamples),ADPCMdataPos[c]);
                
                //console output
                if(!(p%512) && debugLevel>0) std::cout << "\r" << brstm_encoder_nextspinner(spinner) << " Encoding DSPADPCM data... (CH " << (unsigned int)c+1 << "/" << brstmi->num_channels << " " << floor(((float)p/packetCount) * 100) << "%)          ";
            }
            //Write ADPCM information to HEAD3
            brstm_encoder_writebytes(buffer,brstm_encoder_getBEuint(ADPCMdata[c][0],2),2,off=HEAD3_ch_info_offsets[c] + 8 + HEADchunkoffset + 42); //Initial scale
            brstm_encoder_writebytes(buffer,brstm_encoder_getBEuint(ADPCMdata[c][(unsigned long)(brstmi->loop_start / 1.75)],2),2,off=HEAD3_ch_info_offsets[c] + 8 + HEADchunkoffset + 48); //Loop initial scale
            
            if(debugLevel>0) std::cout << "\r" << brstm_encoder_nextspinner(spinner) << " Encoding DSPADPCM data... (CH " << (unsigned int)c+1 << "/" << brstmi->num_channels << " 100%)          ";
        }
    } else {
        ADPCMdata = brstmi->ADPCM_data;
        for(unsigned int c=0;c<brstmi->num_channels;c++) {
            //Write ADPCM information to HEAD3
            brstm_encoder_writebytes(buffer,brstm_encoder_getBEuint(ADPCMdata[c][0],2),2,off=HEAD3_ch_info_offsets[c] + 8 + HEADchunkoffset + 42); //Initial scale
            brstm_encoder_writebytes(buffer,brstm_encoder_getBEuint(ADPCMdata[c][(unsigned long)(brstmi->loop_start / 1.75)],2),2,off=HEAD3_ch_info_offsets[c] + 8 + HEADchunkoffset + 48); //Loop initial scale
        }
    }
    
    if(debugLevel>0) std::cout << "\r" << brstm_encoder_nextspinner(spinner) << " Writing ADPCM data...                                                                        ";
    
    //Write APDCM data to output file buffer
    for(unsigned long b=0;b<brstmi->total_blocks-1;b++) {
        for(unsigned int c=0;c<brstmi->num_channels;c++) {
            brstm_encoder_writebytes(buffer,&ADPCMdata[c][b*brstmi->blocks_size],brstmi->blocks_size,bufpos);
        }
    }
    //Final block
    for(unsigned int c=0;c<brstmi->num_channels;c++) {
        unsigned int i=brstmi->final_block_size;
        brstm_encoder_writebytes(buffer,&ADPCMdata[c][(brstmi->total_blocks-1)*brstmi->blocks_size],brstmi->final_block_size,bufpos);
        if(encodeADPCM == 1) delete[] ADPCMdata[c]; //delete the ADPCM data only if we made it locally
        //padding
        while(i<brstmi->final_block_size_p) {
            brstm_encoder_writebyte(buffer,0x00,bufpos);
            i++;
        }
    }
    if(encodeADPCM == 1) delete[] ADPCMdata; //delete the ADPCM data only if we made it locally
    
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
    brstmi->encoded_file = new unsigned char[bufpos];
    memcpy(brstmi->encoded_file,buffer,bufpos);
    delete[] buffer;
    brstmi->encoded_file_size = bufpos;
    
    if(debugLevel>0) std::cout << "\r" << "BRSTM encoding done                                                     \n";
    
    return 0;
}
