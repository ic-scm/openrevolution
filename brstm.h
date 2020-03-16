//C++ BRSTM reader
//Copyright (C) 2020 Extrasklep
//https://github.com/Extrasklep/brstm

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
unsigned long brstm_getSliceAsNumber(const unsigned char* data,unsigned long start,unsigned long length) {
    if(length>4) {length=4;}
    unsigned long number=0;
    unsigned char* bytes=brstm_getSlice(data,start,length);
    unsigned char pos=length-1; //Read as big endian
    unsigned long pw=1; //Multiply by 1,256,65536...
    //std::cout << length << '\n';
    for(unsigned int i=0;i<length;i++) {
        if(i>0) {pw*=256;}
        number+=bytes[pos]*pw;
        pos--;
    }
    return number;
}

//Get slice as signed 16 bit number
signed int brstm_getSliceAsInt16Sample(const unsigned char * data,unsigned long start) {
    unsigned int length=2;
    unsigned long number=0;
    unsigned char* bytes=brstm_getSlice(data,start,length);
    unsigned char little=bytes[1];
    signed   char big=bytes[0];
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

/*
int16_t  HEAD3_int16_adpcm  [16][16];
int16_t* ADPC_hsamples_1[16];
int16_t* ADPC_hsamples_2[16];
*/// declared in including file

/* 
 * Read the BRSTM file headers and optionally decode the audio data.
 * 
 * fileData: BRSTM file
 * debugLevel:
 *    -1 = Never output anything
 *     0 = Only output errors/warnings
 *     1 = Output information about the BRSTM
 *     2 = Output all information about the BRSTM (offsets, sizes, ADPCM information etc.)
 * decodeADPCM:
 *     0 = Don't decode the audio data (Don't read the DATA chunk)
 *     1 = Decode audio data into PCM_samples
 *     2 = Write the raw ADPCM data into ADPCM_data
 * 
 * Returns error code (>127) or warning code (<128):
 *        0 = No error
 *      255 = Invalid BRSTM file (Doesn't begin with RSTM)
 *      250 = Invalid HEAD chunk (Doesn't begin with HEAD)
 *      249 = Too many channels
 *      248 = Too many tracks
 *      244 = Unknown track info type
 *      240 = Invalid ADPC chunk (Doesn't begin with ADPC)
 *      230 = Invalid DATA chunk (Doesn't begin with DATA)
 *      220 = Unsupported or unknown audio codec
 *      200 = Unknown error (this should never happen)
 */
unsigned char brstm_read(const unsigned char* fileData,signed int debugLevel,uint8_t decodeADPCM) {
    //Read the headers
    //Header
    unsigned long file_size;
    unsigned int header_size;
    unsigned int num_chunks;
    unsigned long HEAD_offset;
    unsigned long HEAD_size;
    unsigned long ADPC_offset;
    unsigned long ADPC_size;
    unsigned long DATA_offset;
    unsigned long DATA_size;
    //HEAD
    unsigned long HEAD_length;
    unsigned long HEAD1_offset;
    unsigned long HEAD2_offset;
    unsigned long HEAD3_offset;
    //HEAD1
    /*unsigned int  HEAD1_codec; //char
    unsigned int  HEAD1_loop;  //char
    unsigned int  HEAD1_num_channels; //char
    unsigned int  HEAD1_sample_rate;
    unsigned long HEAD1_loop_start;
    unsigned long HEAD1_total_samples;
    unsigned long HEAD1_ADPCM_offset;
    unsigned long HEAD1_total_blocks;
    unsigned long HEAD1_blocks_size;
    unsigned long HEAD1_blocks_samples;
    unsigned long HEAD1_final_block_size;
    unsigned long HEAD1_final_block_samples;
    unsigned long HEAD1_final_block_size_p;
    unsigned long HEAD1_samples_per_ADPC;
    unsigned long HEAD1_bytes_per_ADPC;*/ //Should be declared in main file
    //HEAD2
    /*unsigned int  HEAD2_num_tracks;
    unsigned int  HEAD2_track_type;*/ //Should be declared in main file
    
    unsigned long HEAD2_track_info_offsets[8] = {0,0,0,0,0,0,0,0};
    
    /*unsigned int  HEAD2_track_num_channels[8] = {0,0,0,0,0,0,0,0};
    unsigned int  HEAD2_track_lchannel_id [8] = {0,0,0,0,0,0,0,0};
    unsigned int  HEAD2_track_rchannel_id [8] = {0,0,0,0,0,0,0,0};
    //type 1 only
    unsigned int  HEAD2_track_volume      [8] = {0,0,0,0,0,0,0,0};
    unsigned int  HEAD2_track_panning     [8] = {0,0,0,0,0,0,0,0};
    //HEAD3
    unsigned int  HEAD3_num_channels;*/ //Should be declared in main file
    unsigned long HEAD3_ch_info_offsets  [16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    
      /*signed int  HEAD3_int16_adpcm  [16][16];*/ //declared as global
    unsigned int  HEAD3_ch_gain          [16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    unsigned int  HEAD3_ch_initial_scale [16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
      signed int  HEAD3_ch_hsample_1     [16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
      signed int  HEAD3_ch_hsample_2     [16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    unsigned int  HEAD3_ch_loop_ini_scale[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
      signed int  HEAD3_ch_loop_hsample_1[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
      signed int  HEAD3_ch_loop_hsample_2[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    //ADPC
    unsigned long ADPC_total_length;
    unsigned long ADPC_total_entries;
    /*signed   int* ADPC_hsamples_1[16];
    signed   int* ADPC_hsamples_2[16];*/ //declared as global
    //DATA
    unsigned long DATA_total_length;
    //int16_t* PCM_samples[16]; //Should be declared in main file
    //unsigned char* ADPCM_data[16];
    //unsigned char* ADPCM_buffer[16]; //not used yet
    
    //Check if the header matches RSTM
    char* magicstr=brstm_getSliceAsString(fileData,0,4);
    char  emagic1[5]="RSTM";
    char  emagic2[5]="HEAD";
    char  emagic3[5]="ADPC";
    char  emagic4[5]="DATA";
    if(strcmp(magicstr,emagic1) == 0) {
        //Start reading header
        file_size   = brstm_getSliceAsNumber(fileData,0x08,4);
        header_size = brstm_getSliceAsNumber(fileData,0x0C,2);
        num_chunks  = brstm_getSliceAsNumber(fileData,0x0E,2);
        HEAD_offset = brstm_getSliceAsNumber(fileData,0x10,4);
        HEAD_size   = brstm_getSliceAsNumber(fileData,0x14,4);
        ADPC_offset = brstm_getSliceAsNumber(fileData,0x18,4);
        ADPC_size   = brstm_getSliceAsNumber(fileData,0x1C,4);
        DATA_offset = brstm_getSliceAsNumber(fileData,0x20,4);
        DATA_size   = brstm_getSliceAsNumber(fileData,0x24,4);
        
        if(debugLevel>1) {std::cout << "File size: " << file_size << "\nHeader size: " << header_size << "\nChunks: " << num_chunks << "\nHEAD offset: " << HEAD_offset << "\nHEAD size: " << HEAD_size << "\nADPC offset: " << ADPC_offset << "\nADPC size: " << ADPC_size << "\nDATA offset: " << DATA_offset << "\nDATA size: " << DATA_size << "\n\n";}
        
        //HEAD
        magicstr=brstm_getSliceAsString(fileData,HEAD_offset,4);
        if(strcmp(magicstr,emagic2) == 0) {
            //Start reading HEAD
            HEAD_length  = brstm_getSliceAsNumber(fileData,HEAD_offset+0x04,4);
            HEAD1_offset = brstm_getSliceAsNumber(fileData,HEAD_offset+0x0C,4);
            HEAD2_offset = brstm_getSliceAsNumber(fileData,HEAD_offset+0x14,4);
            HEAD3_offset = brstm_getSliceAsNumber(fileData,HEAD_offset+0x1C,4);
            
            if(debugLevel>1) {std::cout << "HEAD length: " << HEAD_length << "\nHEAD1 offset: " << HEAD1_offset << "\nHEAD2 offset: " << HEAD2_offset << "\nHEAD3 offset: " << HEAD3_offset << "\n\n";}
            
            //HEAD1
            HEAD1_offset+=8;
            HEAD1_codec               = brstm_getSliceAsNumber(fileData,HEAD_offset+HEAD1_offset+0x00,1);
            HEAD1_loop                = brstm_getSliceAsNumber(fileData,HEAD_offset+HEAD1_offset+0x01,1);
            HEAD1_num_channels        = brstm_getSliceAsNumber(fileData,HEAD_offset+HEAD1_offset+0x02,1);
            HEAD1_sample_rate         = brstm_getSliceAsNumber(fileData,HEAD_offset+HEAD1_offset+0x04,2);
            HEAD1_loop_start          = brstm_getSliceAsNumber(fileData,HEAD_offset+HEAD1_offset+0x08,4);
            HEAD1_total_samples       = brstm_getSliceAsNumber(fileData,HEAD_offset+HEAD1_offset+0x0C,4);
            HEAD1_ADPCM_offset        = brstm_getSliceAsNumber(fileData,HEAD_offset+HEAD1_offset+0x10,4);
            HEAD1_total_blocks        = brstm_getSliceAsNumber(fileData,HEAD_offset+HEAD1_offset+0x14,4);
            HEAD1_blocks_size         = brstm_getSliceAsNumber(fileData,HEAD_offset+HEAD1_offset+0x18,4);
            HEAD1_blocks_samples      = brstm_getSliceAsNumber(fileData,HEAD_offset+HEAD1_offset+0x1C,4);
            HEAD1_final_block_size    = brstm_getSliceAsNumber(fileData,HEAD_offset+HEAD1_offset+0x20,4);
            HEAD1_final_block_samples = brstm_getSliceAsNumber(fileData,HEAD_offset+HEAD1_offset+0x24,4);
            HEAD1_final_block_size_p  = brstm_getSliceAsNumber(fileData,HEAD_offset+HEAD1_offset+0x28,4);
            HEAD1_samples_per_ADPC    = brstm_getSliceAsNumber(fileData,HEAD_offset+HEAD1_offset+0x2C,4);
            HEAD1_bytes_per_ADPC      = brstm_getSliceAsNumber(fileData,HEAD_offset+HEAD1_offset+0x30,4);
            HEAD1_offset-=8;
            
            if(debugLevel>0) {std::cout << "Codec: " << HEAD1_codec << "\nLoop: " << HEAD1_loop << "\nChannels: " << HEAD1_num_channels << "\nSample rate: " << HEAD1_sample_rate << "\nLoop start: " << HEAD1_loop_start << "\nTotal samples: " << HEAD1_total_samples << "\nOffset to ADPCM data: " << HEAD1_ADPCM_offset << "\nTotal blocks: " << HEAD1_total_blocks << "\nBlock size: " << HEAD1_blocks_size << "\nSamples per block: " << HEAD1_blocks_samples << "\nFinal block size: " << HEAD1_final_block_size << "\nFinal block samples: " << HEAD1_final_block_samples << "\nFinal block size with padding: " << HEAD1_final_block_size_p << "\nSamples per entry in ADPC: " << HEAD1_samples_per_ADPC << "\nBytes per entry in ADPC: " << HEAD1_bytes_per_ADPC << "\n\n";}
            
            //safety
            if(HEAD1_num_channels>16) { if(debugLevel>=0) {std::cout << "Too many channels. Max supported is 16.\n";} return 249;}
            
            //HEAD2
            HEAD2_offset+=8;
            HEAD2_num_tracks = brstm_getSliceAsNumber(fileData,HEAD_offset+HEAD2_offset+0x00,1);
            HEAD2_track_type = brstm_getSliceAsNumber(fileData,HEAD_offset+HEAD2_offset+0x01,1);
            
            //safety
            if(HEAD2_num_tracks>8) { if(debugLevel>=0) {std::cout << "Too many tracks. Max supported is 8.\n";} return 248;}
            
            //read info for each track
            for(unsigned char i=0;i<HEAD2_num_tracks;i++) {
                unsigned int readOffset = HEAD_offset+HEAD2_offset+0x04+4+(i*8);
                unsigned int infoOffset = brstm_getSliceAsNumber(fileData,readOffset,4);
                HEAD2_track_info_offsets[i]=infoOffset+8;
                if(HEAD2_track_type==0) {
                    HEAD2_track_num_channels[i] = brstm_getSliceAsNumber(fileData,HEAD_offset+HEAD2_track_info_offsets[i]+0x00,1);
                    HEAD2_track_lchannel_id [i] = brstm_getSliceAsNumber(fileData,HEAD_offset+HEAD2_track_info_offsets[i]+0x01,1);
                    HEAD2_track_rchannel_id [i] = brstm_getSliceAsNumber(fileData,HEAD_offset+HEAD2_track_info_offsets[i]+0x02,1);
                } else if(HEAD2_track_type==1) {
                    HEAD2_track_num_channels[i] = brstm_getSliceAsNumber(fileData,HEAD_offset+HEAD2_track_info_offsets[i]+0x08,1);
                    HEAD2_track_lchannel_id [i] = brstm_getSliceAsNumber(fileData,HEAD_offset+HEAD2_track_info_offsets[i]+0x09,1);
                    HEAD2_track_rchannel_id [i] = brstm_getSliceAsNumber(fileData,HEAD_offset+HEAD2_track_info_offsets[i]+0x0A,1);
                    //type 1 stuff
                    HEAD2_track_volume      [i] = brstm_getSliceAsNumber(fileData,HEAD_offset+HEAD2_track_info_offsets[i]+0x00,1);
                    HEAD2_track_panning     [i] = brstm_getSliceAsNumber(fileData,HEAD_offset+HEAD2_track_info_offsets[i]+0x01,1);
                } else { if(debugLevel>=0) {std::cout << "Unknown track type.\n";} return 244;}
                HEAD2_track_info_offsets[i]-=8;
            }
            
            if(debugLevel>0) {std::cout << "Tracks: " << HEAD2_num_tracks << "\nTrack type: " << HEAD2_track_type << "\n";}
            if(debugLevel>1) {for(unsigned char i=0;i<HEAD2_num_tracks;i++) {
                std::cout << "\nTrack " << i+1 << "\nOffset: " << HEAD2_track_info_offsets[i] << "\nVolume: " << HEAD2_track_volume[i] << "\nPanning: " << HEAD2_track_panning[i] << "\nChannels: " << HEAD2_track_num_channels[i] << "\nLeft channel ID:  " << HEAD2_track_lchannel_id[i] << "\nRight channel ID: " << HEAD2_track_rchannel_id[i] << "\n\n";
            }}
            
            HEAD2_offset-=8;
            
            //HEAD3
            HEAD3_offset+=8;
            HEAD3_num_channels = brstm_getSliceAsNumber(fileData,HEAD_offset+HEAD3_offset+0x00,1);
            
            //safety
            if(HEAD3_num_channels>16) { if(debugLevel>=0) {std::cout << "Too many channels. Max supported is 16.\n";} return 249;}
            
            for(unsigned char i=0;i<HEAD3_num_channels;i++) {
                unsigned int readOffset = HEAD_offset+HEAD3_offset+0x04+4+(i*8);
                unsigned int infoOffset = brstm_getSliceAsNumber(fileData,readOffset,4);
                HEAD3_ch_info_offsets[i]=infoOffset+8;
                for(unsigned char x=0;x<32;x+=2) {
                    HEAD3_int16_adpcm[i][x/2] = brstm_getSliceAsInt16Sample(fileData,HEAD_offset+HEAD3_ch_info_offsets[i]+0x08+x);
                }
                HEAD3_ch_gain          [i] = brstm_getSliceAsNumber(fileData,HEAD_offset+HEAD3_ch_info_offsets[i]+0x28,2);
                HEAD3_ch_initial_scale [i] = brstm_getSliceAsNumber(fileData,HEAD_offset+HEAD3_ch_info_offsets[i]+0x2A,2);
                HEAD3_ch_hsample_1     [i] = brstm_getSliceAsInt16Sample(fileData,HEAD_offset+HEAD3_ch_info_offsets[i]+0x2C);
                HEAD3_ch_hsample_2     [i] = brstm_getSliceAsInt16Sample(fileData,HEAD_offset+HEAD3_ch_info_offsets[i]+0x2E);
                HEAD3_ch_loop_ini_scale[i] = brstm_getSliceAsNumber(fileData,HEAD_offset+HEAD3_ch_info_offsets[i]+0x30,2);
                HEAD3_ch_loop_hsample_1[i] = brstm_getSliceAsInt16Sample(fileData,HEAD_offset+HEAD3_ch_info_offsets[i]+0x32);
                HEAD3_ch_loop_hsample_2[i] = brstm_getSliceAsInt16Sample(fileData,HEAD_offset+HEAD3_ch_info_offsets[i]+0x34);
                HEAD3_ch_info_offsets[i]-=8;
            }
            
            if(debugLevel>0) {std::cout << "Channels: " << HEAD3_num_channels << "\n";}
            if(debugLevel>1) {for(unsigned char i=0;i<HEAD3_num_channels;i++) {
                std::cout << "\nChannel " << i+1 << "\nOffset: " << HEAD3_ch_info_offsets[i] << "\nGain: " << HEAD3_ch_gain[i] << "\nInitial scale: " << HEAD3_ch_initial_scale[i] << "\nHistory sample 1: " << HEAD3_ch_hsample_1[i] << "\nHistory sample 2: " << HEAD3_ch_hsample_2[i] << "\nLoop initial scale: " << HEAD3_ch_loop_ini_scale[i] << "\nLoop history sample 1: " << HEAD3_ch_loop_hsample_1[i] << "\nLoop history sample 2: " << HEAD3_ch_loop_hsample_2[i] << "\nADPCM coefficients: ";
                for(unsigned char x=0;x<16;x++) {
                    std::cout << HEAD3_int16_adpcm[i][x] << ' ';
                }
                std::cout << "\n\n";
            }}
            
            HEAD3_offset-=8;
            
            //ADPC chunk
            magicstr=brstm_getSliceAsString(fileData,ADPC_offset,4);
            if(strcmp(magicstr,emagic3) == 0) {
                //Start reading ADPC
                ADPC_total_length  = brstm_getSliceAsNumber(fileData,ADPC_offset+0x04,4);
                ADPC_total_entries = (ADPC_total_length-8)/HEAD1_bytes_per_ADPC;
                for(unsigned int n=0;n<HEAD3_num_channels;n++) {
                    ADPC_hsamples_1[n] = new int16_t[ADPC_total_entries/HEAD3_num_channels];
                    ADPC_hsamples_2[n] = new int16_t[ADPC_total_entries/HEAD3_num_channels];
                    
                    if(debugLevel>1) {std::cout << "Channel " << n << ": ";}
                    
                    unsigned int it;
                    for(unsigned int i=0;i<ADPC_total_entries/HEAD3_num_channels;i++) {
                        unsigned int offset=ADPC_offset+8+(n*HEAD1_bytes_per_ADPC)+((i*HEAD1_bytes_per_ADPC)*HEAD3_num_channels);
                        ADPC_hsamples_1[n][i] = brstm_getSliceAsInt16Sample(fileData,offset);
                        ADPC_hsamples_2[n][i] = brstm_getSliceAsInt16Sample(fileData,offset+2);
                        it=i+1;
                    }
                    if(debugLevel>1) {std::cout << it << " history sample pairs read.\n";}
                }
                
                if(debugLevel>1) {std::cout << "ADPC length: " << ADPC_total_length << "\nTotal entries: " << ADPC_total_entries << "\n\n";}
                
                //DATA chunk
                magicstr=brstm_getSliceAsString(fileData,DATA_offset,4);
                if(strcmp(magicstr,emagic4) == 0) {
                    //Start reading DATA
                    DATA_total_length = brstm_getSliceAsNumber(fileData,DATA_offset+0x04,4);
                    
                    if(debugLevel>1) {std::cout << "DATA length: " << DATA_total_length << '\n';}
                    
                    if(HEAD1_codec!=2) { if(debugLevel>=0) {std::cout << "Unsupported codec.\n";} return 220;}
                    
                    if(decodeADPCM) {
                        //Read the ADPCM data
                        unsigned long decoded_samples=0;
                        
                        unsigned long posOffset=0;
                        
                        for(unsigned int c=0;c<HEAD3_num_channels;c++) {
                            //Create new array of samples for the current channel
                            switch(decodeADPCM) {
                                case 1: PCM_samples[c] = new int16_t[((DATA_total_length-32)*2)/HEAD3_num_channels]; break;
                                case 2: ADPCM_data [c] = new unsigned char[((DATA_total_length-32))/HEAD3_num_channels]; break;
                            }
                            
                            posOffset=0+(HEAD1_blocks_size*c);
                            unsigned long outputPos = 0; //position in PCM samples or ADPCM data output array
                            for(unsigned long b=0;b<HEAD1_total_blocks;b++) {
                                //Read every block
                                unsigned int currentBlockSize    = HEAD1_blocks_size;
                                unsigned int currentBlockSamples = HEAD1_blocks_samples;
                                //Final block
                                if(b==HEAD1_total_blocks-1) {
                                    currentBlockSize    = HEAD1_final_block_size;
                                    currentBlockSamples = HEAD1_final_block_samples;
                                }
                                if(b>=HEAD1_total_blocks-1 && c>0) {
                                    //Go back to the previous position
                                    posOffset-=HEAD1_blocks_size*HEAD3_num_channels;
                                    //Go to the next block in position of first channel
                                    posOffset+=HEAD1_blocks_size*(HEAD3_num_channels-c);
                                    //Jump to the correct channel in the final block
                                    posOffset+=HEAD1_final_block_size_p*c;
                                }
                                //Get data from just the current block
                                unsigned char* blockData = brstm_getSlice(fileData,HEAD1_ADPCM_offset+posOffset,currentBlockSize);
                                
                                if(decodeADPCM == 1) {
                                    //Decode 4 bit ADPCM
                                    const unsigned char ps = blockData[0];
                                    const   signed int  yn1 = ADPC_hsamples_1[c][b], yn2 = ADPC_hsamples_2[c][b];
                                    
                                    //Magic adapted from brawllib's ADPCMState.cs
                                    signed int 
                                    cps = ps,
                                    cyn1 = yn1,
                                    cyn2 = yn2;
                                    unsigned long dataIndex = 0;
                                    
                                    for (unsigned int sampleIndex=0;sampleIndex<currentBlockSamples;) {
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
                                        
                                        outSample = (0x400 + ((scale * outSample) << 11) + HEAD3_int16_adpcm[c][brstm_clamp(cIndex, 0, 15)] * cyn1 + HEAD3_int16_adpcm[c][brstm_clamp(cIndex + 1, 0, 15)] * cyn2) >> 11;
                                        
                                        cyn2 = cyn1;
                                        cyn1 = brstm_clamp(outSample, -32768, 32767);
                                        
                                        PCM_samples[c][outputPos++] = cyn1;
                                        decoded_samples++;
                                    }
                                } else {
                                    //Write raw data to ADPCM_data
                                    for(unsigned int i=0; i<currentBlockSize; i++) {
                                        ADPCM_data[c][outputPos++] = blockData[i];
                                    }
                                }
                                
                                posOffset+=HEAD1_blocks_size*HEAD3_num_channels;
                            }
                        }
                        if(debugLevel>0) {std::cout << "Decoded PCM samples: " << decoded_samples << '\n';}
                    }
                    //end
                    return 0;
                    
                } else { if(debugLevel>=0) {std::cout << "Invalid DATA chunk.\n";} return 230;}
                
            } else { if(debugLevel>=0) {std::cout << "Invalid ADPC chunk.\n";} return 240;}
            
        } else { if(debugLevel>=0) {std::cout << "Invalid HEAD chunk.\n";} return 250;}
        
    } else { if(debugLevel>=0) {std::cout << "Invalid BRSTM file.\n";} return 255;}
    return 0;
}

//backwards comaptibility
unsigned char readBrstm(const unsigned char* fileData,signed int debugLevel,bool decodeADPCM) {
    if(debugLevel>=0) {std::cout << "Warning: readBrstm is deprecated, use brstm_read instead\n";}
    return brstm_read(fileData,debugLevel,decodeADPCM);
}

unsigned char readBrstm(const unsigned char* fileData,unsigned char debugLevel) {
    return readBrstm(fileData,debugLevel,true);
}

//int16_t* PCM_buffer[16]; Should be declared in main file

//block cache
int16_t* PCM_blockbuffer[16];
int PCM_blockbuffer_currentBlock = -1;

bool brstm_getbuffer_useBuffer = true;

//This function is used by brstm_getbuffer
unsigned char* brstm_getblock(const unsigned char* fileData,bool dataType,unsigned long start,unsigned long length);

/* 
 * Get a buffer of audio data
 * 
 * fileData: BRSTM file
 * sampleOffset: Offset to the first sample in the buffer
 * bufferSamples: Amount of samples in the buffer (don't make this more than the amount of samples per block!)
 * 
 */
void brstm_getbuffer(const unsigned char* fileData,unsigned long sampleOffset,unsigned int bufferSamples);

/*
 * Get a buffer of audio data (fstream mode)
 * 
 * stream: std::ifstream with an open BRSTM file
 * sampleOffset: Offset to the first sample in the buffer
 * bufferSamples: Amount of samples in the buffer (don't make this more than the amount of samples per block!)
 * 
 */
void brstm_fstream_getbuffer(std::ifstream& stream,unsigned long sampleOffset,unsigned int bufferSamples);

/*
 * Main function for both memory modes
 * dataType will be 1 for disk streaming mode (fileData will be null) so brstm_getblock
 * will know to do disk streaming stuff instead of just getting a slice of fileData
 */
void brstm_getbuffer_main(const unsigned char* fileData,bool dataType,unsigned long sampleOffset,unsigned int bufferSamples) {
    //safety
    if(sampleOffset>HEAD1_total_samples) {
        for(unsigned int c=0;c<HEAD3_num_channels;c++) {
            delete[] PCM_buffer[c];
            PCM_buffer[c] = new int16_t[bufferSamples];
            for(unsigned int i=0;i<bufferSamples;i++) {
                PCM_buffer[c][i] = 0;
            }
        }
        return;
    }
    //decode a new block if we don't have the current block in the blockbuffer cache
    if(PCM_blockbuffer_currentBlock != sampleOffset/HEAD1_blocks_samples) {
        //calculate block number
        unsigned long b=sampleOffset/HEAD1_blocks_samples;
        //Read the ADPCM data
        unsigned long posOffset=0;
        if(HEAD1_codec!=2) {exit(220);}
        for(unsigned int c=0;c<HEAD3_num_channels;c++) {
            //Create new array of samples for the current channel
            delete[] PCM_blockbuffer[c];
            PCM_blockbuffer[c] = new int16_t[HEAD1_blocks_samples];
            
            posOffset=0+(HEAD1_blocks_size*c);
            unsigned long outputPos = 0; //position in PCM block samples output array
            
            unsigned long c_writtensamples = 0;
            
            posOffset+=b*(HEAD1_blocks_size*HEAD3_num_channels);
            
            //Read block
            unsigned int currentBlockSize    = HEAD1_blocks_size;
            unsigned int currentBlockSamples = HEAD1_blocks_samples;
            //Final block
            if(b==HEAD1_total_blocks-1) {
                currentBlockSize    = HEAD1_final_block_size;
                currentBlockSamples = HEAD1_final_block_samples;
            }
            if(b>=HEAD1_total_blocks-1 && c>0) {
                //Go back to the previous position
                posOffset-=HEAD1_blocks_size*HEAD3_num_channels;
                //Go to the next block in position of first channel
                posOffset+=HEAD1_blocks_size*(HEAD3_num_channels-c);
                //Jump to the correct channel in the final block
                posOffset+=HEAD1_final_block_size_p*c;
            }
            //Get data from just the current block
            unsigned char* blockData = brstm_getblock(fileData,dataType,HEAD1_ADPCM_offset+posOffset,currentBlockSize);
            
            //4 bit ADPCM - No comments, no one knows what this code does :^) Stolen from that node module
            const unsigned char ps = blockData[0];
            const   signed int  yn1 = ADPC_hsamples_1[c][b], yn2 = ADPC_hsamples_2[c][b];
            
            //Magic adapted from brawllib's ADPCMState.cs
            signed int 
            cps = ps,
            cyn1 = yn1,
            cyn2 = yn2;
            unsigned long dataIndex = 0;
            
            for (unsigned int sampleIndex=0;sampleIndex<currentBlockSamples;) {
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
                
                outSample = (0x400 + ((scale * outSample) << 11) + HEAD3_int16_adpcm[c][brstm_clamp(cIndex, 0, 15)] * cyn1 + HEAD3_int16_adpcm[c][brstm_clamp(cIndex + 1, 0, 15)] * cyn2) >> 11;
                
                cyn2 = cyn1;
                cyn1 = brstm_clamp(outSample, -32768, 32767);
                
                PCM_blockbuffer[c][outputPos++] = cyn1;
                c_writtensamples++;
            }
            PCM_blockbuffer_currentBlock = b;
            //std::cout << ">>" << c << b << yn1 << yn2 << ps << blockData << sampleResult << '\n';
            posOffset+=HEAD1_blocks_size*HEAD3_num_channels;
        }
    }
    //create the requested buffer
    if(brstm_getbuffer_useBuffer) {
        bool blockEndReached = false;
        unsigned int blockEndReachedAt = 0;
        for(unsigned int c=0;c<HEAD3_num_channels;c++) {
            //Create new array of samples for the current channel
            delete[] PCM_buffer[c];
            PCM_buffer[c] = new int16_t[bufferSamples];
            //offset in current block
            unsigned long dataIndex = sampleOffset-HEAD1_blocks_samples*(unsigned int)(sampleOffset/HEAD1_blocks_samples);
            for(unsigned int p=0;p<bufferSamples;p++) {
                if(dataIndex+p >= HEAD1_blocks_samples) {
                    blockEndReached=true;
                    blockEndReachedAt=p;
                    break;
                }
                PCM_buffer[c][p] = PCM_blockbuffer[c][dataIndex+p];
            }
        }
        if(blockEndReached) {
            brstm_getbuffer_useBuffer = false; //don't make a new buffer in PCM_buffer
            brstm_getbuffer_main(fileData,dataType,sampleOffset+blockEndReachedAt,0);
            brstm_getbuffer_useBuffer = true;
            for(unsigned int c=0;c<HEAD3_num_channels;c++) {
                unsigned int dataIndex=0;
                for(unsigned int p=blockEndReachedAt;p<bufferSamples;p++) {
                    PCM_buffer[c][p] = PCM_blockbuffer[c][dataIndex++];
                }
            }
        }
    }
}

void brstm_getbuffer(const unsigned char* fileData,unsigned long sampleOffset,unsigned int bufferSamples) {
    brstm_getbuffer_main(fileData,0,sampleOffset,bufferSamples);
}

//don't do void* kids
std::ifstream* brstm_ifstream;

void brstm_fstream_getbuffer(std::ifstream& stream,unsigned long sampleOffset,unsigned int bufferSamples) {
    if(!stream.is_open()) {perror("brstm_fstream_getbuffer: No file open in ifstream"); exit(255);}
    brstm_ifstream = &stream;
    brstm_getbuffer_main(nullptr,1,sampleOffset,bufferSamples);
}

//backwards comaptibility
void brstm_getbuffer(const unsigned char* fileData,unsigned long sampleOffset,unsigned int bufferSamples,bool useBuffer) {
    brstm_getbuffer(fileData,sampleOffset,bufferSamples);
}

//This function is used by brstm_getbuffer
unsigned char* brstm_getblock(const unsigned char* fileData,bool dataType,unsigned long start,unsigned long length) {
    if(dataType == 0) {
        return brstm_getSlice((const unsigned char*)fileData,start,length);
    } else {
        //disk streaming
        delete[] brstm_slice;
        brstm_slice = new unsigned char[length];
        
        brstm_ifstream->seekg(start);
        
        if(brstm_ifstream->bad()) {perror("brstm_getblock: ifstream error"); exit(255);}
        
        brstm_ifstream->read((char*)brstm_slice,length);
        return brstm_slice;
    }
}

/*
 * Read BRSTM directly from an ifstream
 * 
 * stream: std::ifstream with an open BRSTM file
 * debugLevel: console debug level, same as brstm_read
 */
unsigned char brstm_fstream_read(std::ifstream& stream,signed int debugLevel) {
    if(!stream.is_open()) {
        if(debugLevel>=0) {std::cout << "brstm_fstream_read: no file open in std::ifstream& stream.\n";}
        return 255;
    }
    unsigned char* brstm_header;
    //check if the file has the RSTM word before allocating memory for the full BRSTM header
    //so you won't try to allocate and read huge amounts of memory if an invalid file
    //has a big number in the place where the offset to the DATA chunk usually is
    stream.seekg(0);
    const char* emagic = "RSTM";
    char magicword[5];
    stream.read(magicword,4);
    magicword[4] = '\0';
    if(strcmp(magicword,emagic) != 0) {
        if(debugLevel>=0) {std::cout << "Invalid BRSTM file.\n";}
        return 255;
    }
    
    //get offset to DATA chunk
    stream.seekg(0x20);
    unsigned char of0x20[4];
    stream.read((char*)of0x20,4);
    unsigned int headerSize = brstm_getSliceAsNumber(of0x20,0,4) + 512;
    
    //read header into memory
    stream.seekg(0);
    brstm_header = new unsigned char[headerSize];
    stream.read((char*)brstm_header,headerSize);
    stream.seekg(0);
    
    //call main brstm read function
    unsigned char res = brstm_read(brstm_header,debugLevel,false);
    delete[] brstm_header;
    return res;
}

/* 
 * Close the BRSTM file (reset variables and free memory)
 */
void brstm_close() {
    for(unsigned char i=0;i<16;i++) {
        for(unsigned char j=0;j<16;j++) {
            HEAD3_int16_adpcm[i][j] = 0;
        }
        delete[] ADPC_hsamples_1[i];
        delete[] ADPC_hsamples_2[i];
        delete[] PCM_samples[i];
        delete[] ADPCM_data[i];
    }
    
    HEAD1_codec = 0;
    HEAD1_loop = 0;
    HEAD1_num_channels = 0;
    HEAD1_sample_rate = 0;
    HEAD1_loop_start = 0;
    HEAD1_total_samples = 0;
    HEAD1_ADPCM_offset = 0;
    HEAD1_total_blocks = 0;
    HEAD1_blocks_size = 0;
    HEAD1_blocks_samples = 0;
    HEAD1_final_block_size = 0;
    HEAD1_final_block_samples = 0;
    HEAD1_final_block_size_p = 0;
    HEAD1_samples_per_ADPC = 0;
    HEAD1_bytes_per_ADPC = 0;
    
    HEAD2_num_tracks = 0;
    HEAD2_track_type = 0;
    
    for(unsigned char i=0; i<8; i++) {
        HEAD2_track_num_channels[i] = 0;
        HEAD2_track_lchannel_id [i] = 0;
        HEAD2_track_rchannel_id [i] = 0;
        HEAD2_track_volume      [i] = 0;
        HEAD2_track_panning     [i] = 0;
    }
    HEAD3_num_channels = 0;
    PCM_blockbuffer_currentBlock = -1;
}
