//C++ BRSTM reader
//Copyright (C) 2020 Extrasklep

unsigned char* slice;
char* slicestring;

long clamp(long value, long min, long max) {
  return value <= min ? min : value >= max ? max : value;
}

unsigned char* getSlice(const unsigned char* data,unsigned long start,unsigned long length) {
    delete[] slice;
    slice = new unsigned char[length];
    for(unsigned int i=0;i<length;i++) {
        slice[i]=data[i+start];
    }
    return slice;
}

unsigned long getSliceAsNumber(const unsigned char* data,unsigned long start,unsigned long length) {
    if(length>4) {length=4;}
    unsigned long number=0;
    unsigned char* bytes=getSlice(data,start,length);
    unsigned char pos=length-1; //Read as big endian
    unsigned long pw=1; //Multiply by 1,256,65536...
    //std::cout << length << '\n';
    for(unsigned int i=0;i<length;i++) {
        if(i>0) {pw*=256;}
        number+=bytes[pos]*pw;
        //unsigned int n=bytes[pos]; std::cout << n << ' ' << number << '\n';
        pos--;
    }
    return number;
}

signed int getSliceAsInt16Sample(const unsigned char * data,unsigned long start) {
    unsigned int length=2;
    unsigned long number=0;
    unsigned char* bytes=getSlice(data,start,length);
    unsigned char little=bytes[1];
    signed   char big=bytes[0];
    number=little+big*256;
    return number;
}

char* getSliceAsString(const unsigned char* data,unsigned long start,unsigned long length) {
    unsigned char slicestr[length+1];
    unsigned char* bytes=getSlice(data,start,length);
    for(unsigned int i=0;i<length;i++) {
        slicestr[i]=bytes[i];
        if(slicestr[i]=='\0') {slicestr[i]=' ';}
    }
    slicestr[length]='\0';
    delete[] slice;
    slicestring = new char[length+1];
    for(unsigned int i=0;i<length+1;i++) {
        slicestring[i]=slicestr[i];
    }
    return slicestring;
}

signed int  HEAD3_int16_adpcm  [16][16];
signed   int* ADPC_hsamples_1[16];
signed   int* ADPC_hsamples_2[16];

unsigned char readBrstm(const unsigned char* fileData,unsigned char debugLevel,bool decodeADPCM) {
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
    
    //Check if the header matches RSTM
    char* magicstr=getSliceAsString(fileData,0,4);
    char  emagic1[5]="RSTM";
    char  emagic2[5]="HEAD";
    char  emagic3[5]="ADPC";
    char  emagic4[5]="DATA";
    bool  magic=1;
    for(unsigned int i=0;i<strlen(magicstr);i++) {
        if(magicstr[i]!=emagic1[i]) {magic=0;}
    }
    if(magic) {
        //Start reading header
        file_size   = getSliceAsNumber(fileData,0x08,4);
        header_size = getSliceAsNumber(fileData,0x0C,2);
        num_chunks  = getSliceAsNumber(fileData,0x0E,2);
        HEAD_offset = getSliceAsNumber(fileData,0x10,4);
        HEAD_size   = getSliceAsNumber(fileData,0x14,4);
        ADPC_offset = getSliceAsNumber(fileData,0x18,4);
        ADPC_size   = getSliceAsNumber(fileData,0x1C,4);
        DATA_offset = getSliceAsNumber(fileData,0x20,4);
        DATA_size   = getSliceAsNumber(fileData,0x24,4);
        if(debugLevel>1) {std::cout << "File size: " << file_size << "\nHeader size: " << header_size << "\nChunks: " << num_chunks << "\nHEAD offset: " << HEAD_offset << "\nHEAD size: " << HEAD_size << "\nADPC offset: " << ADPC_offset << "\nADPC size: " << ADPC_size << "\nDATA offset: " << DATA_offset << "\nDATA size: " << DATA_size << "\n\n";}
        
        //HEAD
        magicstr=getSliceAsString(fileData,HEAD_offset,4);
        for(unsigned int i=0;i<strlen(magicstr);i++) {
            if(magicstr[i]!=emagic2[i]) {magic=0;}
        }
        if(magic) {
            //Start reading HEAD
            HEAD_length  = getSliceAsNumber(fileData,HEAD_offset+0x04,4);
            HEAD1_offset = getSliceAsNumber(fileData,HEAD_offset+0x0C,4);
            HEAD2_offset = getSliceAsNumber(fileData,HEAD_offset+0x14,4);
            HEAD3_offset = getSliceAsNumber(fileData,HEAD_offset+0x1C,4);
            if(debugLevel>1) {std::cout << "HEAD length: " << HEAD_length << "\nHEAD1 offset: " << HEAD1_offset << "\nHEAD2 offset: " << HEAD2_offset << "\nHEAD3 offset: " << HEAD3_offset << "\n\n";}
            //HEAD1
            HEAD1_offset+=8;
            HEAD1_codec               = getSliceAsNumber(fileData,HEAD_offset+HEAD1_offset+0x00,1);
            HEAD1_loop                = getSliceAsNumber(fileData,HEAD_offset+HEAD1_offset+0x01,1);
            HEAD1_num_channels        = getSliceAsNumber(fileData,HEAD_offset+HEAD1_offset+0x02,1);
            HEAD1_sample_rate         = getSliceAsNumber(fileData,HEAD_offset+HEAD1_offset+0x04,2);
            HEAD1_loop_start          = getSliceAsNumber(fileData,HEAD_offset+HEAD1_offset+0x08,4);
            HEAD1_total_samples       = getSliceAsNumber(fileData,HEAD_offset+HEAD1_offset+0x0C,4);
            HEAD1_ADPCM_offset        = getSliceAsNumber(fileData,HEAD_offset+HEAD1_offset+0x10,4);
            HEAD1_total_blocks        = getSliceAsNumber(fileData,HEAD_offset+HEAD1_offset+0x14,4);
            HEAD1_blocks_size         = getSliceAsNumber(fileData,HEAD_offset+HEAD1_offset+0x18,4);
            HEAD1_blocks_samples      = getSliceAsNumber(fileData,HEAD_offset+HEAD1_offset+0x1C,4);
            HEAD1_final_block_size    = getSliceAsNumber(fileData,HEAD_offset+HEAD1_offset+0x20,4);
            HEAD1_final_block_samples = getSliceAsNumber(fileData,HEAD_offset+HEAD1_offset+0x24,4);
            HEAD1_final_block_size_p  = getSliceAsNumber(fileData,HEAD_offset+HEAD1_offset+0x28,4);
            HEAD1_samples_per_ADPC    = getSliceAsNumber(fileData,HEAD_offset+HEAD1_offset+0x2C,4);
            HEAD1_bytes_per_ADPC      = getSliceAsNumber(fileData,HEAD_offset+HEAD1_offset+0x30,4);
            HEAD1_offset-=8;
            if(debugLevel>0) {std::cout << "Codec: " << HEAD1_codec << "\nLoop: " << HEAD1_loop << "\nChannels: " << HEAD1_num_channels << "\nSample rate: " << HEAD1_sample_rate << "\nLoop start: " << HEAD1_loop_start << "\nTotal samples: " << HEAD1_total_samples << "\nOffset to ADPCM data: " << HEAD1_ADPCM_offset << "\nTotal blocks: " << HEAD1_total_blocks << "\nBlock size: " << HEAD1_blocks_size << "\nSamples per block: " << HEAD1_blocks_samples << "\nFinal block size: " << HEAD1_final_block_size << "\nFinal block samples: " << HEAD1_final_block_samples << "\nFinal block size with padding: " << HEAD1_final_block_size_p << "\nSamples per entry in ADPC: " << HEAD1_samples_per_ADPC << "\nBytes per entry in ADPC: " << HEAD1_bytes_per_ADPC << "\n\n";}
            
            //safety
            if(HEAD1_num_channels>16) {std::cout << "Too many channels. Max supported is 16.\n"; return 249;}
            
            //HEAD2
            HEAD2_offset+=8;
            HEAD2_num_tracks = getSliceAsNumber(fileData,HEAD_offset+HEAD2_offset+0x00,1);
            HEAD2_track_type = getSliceAsNumber(fileData,HEAD_offset+HEAD2_offset+0x01,1);
            
            //safety
            if(HEAD2_num_tracks>8) {std::cout << "Too many tracks. Max supported is 8.\n"; return 248;}
            
            //read info for each track
            for(unsigned char i=0;i<HEAD2_num_tracks;i++) {
                unsigned int readOffset = HEAD_offset+HEAD2_offset+0x04+4+(i*8);
                unsigned int infoOffset = getSliceAsNumber(fileData,readOffset,4);
                HEAD2_track_info_offsets[i]=infoOffset+8;
                if(HEAD2_track_type==0) {
                    HEAD2_track_num_channels[i] = getSliceAsNumber(fileData,HEAD_offset+HEAD2_track_info_offsets[i]+0x00,1);
                    HEAD2_track_lchannel_id [i] = getSliceAsNumber(fileData,HEAD_offset+HEAD2_track_info_offsets[i]+0x01,1);
                    HEAD2_track_rchannel_id [i] = getSliceAsNumber(fileData,HEAD_offset+HEAD2_track_info_offsets[i]+0x02,1);
                } else if(HEAD2_track_type==1) {
                    HEAD2_track_num_channels[i] = getSliceAsNumber(fileData,HEAD_offset+HEAD2_track_info_offsets[i]+0x08,1);
                    HEAD2_track_lchannel_id [i] = getSliceAsNumber(fileData,HEAD_offset+HEAD2_track_info_offsets[i]+0x09,1);
                    HEAD2_track_rchannel_id [i] = getSliceAsNumber(fileData,HEAD_offset+HEAD2_track_info_offsets[i]+0x0A,1);
                    //type 1 stuff
                    HEAD2_track_volume      [i] = getSliceAsNumber(fileData,HEAD_offset+HEAD2_track_info_offsets[i]+0x00,1);
                    HEAD2_track_panning     [i] = getSliceAsNumber(fileData,HEAD_offset+HEAD2_track_info_offsets[i]+0x01,1);
                } else {std::cout << "Unknown track type.\n"; return 254;}
                HEAD2_track_info_offsets[i]-=8;
            }
            
            if(debugLevel>0) {std::cout << "Tracks: " << HEAD2_num_tracks << "\nTrack type: " << HEAD2_track_type << "\n";}
            if(debugLevel>1) {for(unsigned char i=0;i<HEAD2_num_tracks;i++) {
                std::cout << "\nTrack " << i+1 << "\nOffset: " << HEAD2_track_info_offsets[i] << "\nVolume: " << HEAD2_track_volume[i] << "\nPanning: " << HEAD2_track_panning[i] << "\nChannels: " << HEAD2_track_num_channels[i] << "\nLeft channel ID:  " << HEAD2_track_lchannel_id[i] << "\nRight channel ID: " << HEAD2_track_rchannel_id[i] << "\n\n";
            }}
            HEAD2_offset-=8;
            
            //HEAD3
            HEAD3_offset+=8;
            HEAD3_num_channels = getSliceAsNumber(fileData,HEAD_offset+HEAD3_offset+0x00,1);
            
            //safety
            if(HEAD3_num_channels>16) {std::cout << "Too many channels. Max supported is 16.\n"; return 249;}
            
            for(unsigned char i=0;i<HEAD3_num_channels;i++) {
                unsigned int readOffset = HEAD_offset+HEAD3_offset+0x04+4+(i*8);
                unsigned int infoOffset = getSliceAsNumber(fileData,readOffset,4);
                HEAD3_ch_info_offsets[i]=infoOffset+8;
                for(unsigned char x=0;x<32;x+=2) {
                    HEAD3_int16_adpcm[i][x/2] = getSliceAsInt16Sample(fileData,HEAD_offset+HEAD3_ch_info_offsets[i]+0x08+x);
                }
                HEAD3_ch_gain          [i] = getSliceAsNumber(fileData,HEAD_offset+HEAD3_ch_info_offsets[i]+0x28,2);
                HEAD3_ch_initial_scale [i] = getSliceAsNumber(fileData,HEAD_offset+HEAD3_ch_info_offsets[i]+0x2A,2);
                HEAD3_ch_hsample_1     [i] = getSliceAsInt16Sample(fileData,HEAD_offset+HEAD3_ch_info_offsets[i]+0x2C);
                HEAD3_ch_hsample_2     [i] = getSliceAsInt16Sample(fileData,HEAD_offset+HEAD3_ch_info_offsets[i]+0x2E);
                HEAD3_ch_loop_ini_scale[i] = getSliceAsNumber(fileData,HEAD_offset+HEAD3_ch_info_offsets[i]+0x30,2);
                HEAD3_ch_loop_hsample_1[i] = getSliceAsInt16Sample(fileData,HEAD_offset+HEAD3_ch_info_offsets[i]+0x32);
                HEAD3_ch_loop_hsample_2[i] = getSliceAsInt16Sample(fileData,HEAD_offset+HEAD3_ch_info_offsets[i]+0x34);
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
            magicstr=getSliceAsString(fileData,ADPC_offset,4);
            for(unsigned int i=0;i<strlen(magicstr);i++) {
                if(magicstr[i]!=emagic3[i]) {magic=0;}
            }
            if(magic) {
                //Start reading ADPC
                ADPC_total_length  = getSliceAsNumber(fileData,ADPC_offset+0x04,4);
                ADPC_total_entries = (ADPC_total_length-8)/HEAD1_bytes_per_ADPC;
                for(unsigned int n=0;n<HEAD3_num_channels;n++) {
                    ADPC_hsamples_1[n] = new signed int[ADPC_total_entries/HEAD3_num_channels];
                    ADPC_hsamples_2[n] = new signed int[ADPC_total_entries/HEAD3_num_channels];
                    if(debugLevel>1) {std::cout << "Channel " << n << ": ";}
                    unsigned int it;
                    for(unsigned int i=0;i<ADPC_total_entries/HEAD3_num_channels;i++) {
                        unsigned int offset=ADPC_offset+8+(n*HEAD1_bytes_per_ADPC)+((i*HEAD1_bytes_per_ADPC)*HEAD3_num_channels);
                        ADPC_hsamples_1[n][i] = getSliceAsInt16Sample(fileData,offset);
                        ADPC_hsamples_2[n][i] = getSliceAsInt16Sample(fileData,offset+2);
                        //std::cout << "\nRead at offset " << offset << " result " << ADPC_hsamples_1[n][i] << ", " << ADPC_hsamples_2[n][i] << '\n';
                        it=i+1;
                    }
                    if(debugLevel>1) {std::cout << it << " history sample pairs read.\n";}
                }
                if(debugLevel>1) {std::cout << "ADPC length: " << ADPC_total_length << "\nTotal entries: " << ADPC_total_entries << "\n\n";}
                
                //DATA chunk
                magicstr=getSliceAsString(fileData,DATA_offset,4);
                for(unsigned int i=0;i<strlen(magicstr);i++) {
                    if(magicstr[i]!=emagic4[i]) {magic=0;}
                }
                if(magic) {
                    //Start reading DATA
                    DATA_total_length = getSliceAsNumber(fileData,DATA_offset+0x04,4);
                    if(debugLevel>1) {std::cout << "DATA length: " << DATA_total_length << '\n';}
                    
                    if(decodeADPCM) {
                        //Read the ADPCM data
                        //unsigned long written_samples=0; //#########################################################################################################-Should be declared in main file
                        
                        unsigned long posOffset=0;
                        if(HEAD1_codec!=2) {std::cout << "Unsupported codec.\n"; return 220;}
                        for(unsigned int c=0;c<HEAD3_num_channels;c++) {
                            //Create new array of samples for the current channel
                            PCM_samples[c] = new int16_t[((DATA_total_length-32)*2)/HEAD3_num_channels];
                            
                            posOffset=0+(HEAD1_blocks_size*c);
                            unsigned long outputPos = 0; //position in PCM samples output array
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
                                unsigned char* blockData = getSlice(fileData,HEAD1_ADPCM_offset+posOffset,currentBlockSize);
                                
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
                                    
                                    outSample = (0x400 + ((scale * outSample) << 11) + HEAD3_int16_adpcm[c][clamp(cIndex, 0, 15)] * cyn1 + HEAD3_int16_adpcm[c][clamp(cIndex + 1, 0, 15)] * cyn2) >> 11;
                                    
                                    cyn2 = cyn1;
                                    cyn1 = clamp(outSample, -32768, 32767);
                                    
                                    PCM_samples[c][outputPos++] = cyn1;
                                    written_samples++;
                                }
                                //std::cout << ">>" << c << b << yn1 << yn2 << ps << blockData << sampleResult << '\n';
                                posOffset+=HEAD1_blocks_size*HEAD3_num_channels;
                            }
                        }
                        if(debugLevel>0) {std::cout << "Written PCM samples: " << written_samples << '\n';}
                    }
                    //end
                    return 0;
                    
                } else {std::cout << "Invalid DATA chunk.\n"; return 230;}
                
            } else {std::cout << "Invalid ADPC chunk.\n"; return 240;}
            
        } else {std::cout << "Invalid HEAD chunk.\n"; return 250;}
        
    } else {std::cout << "Invalid BRSTM file.\n"; return 255;}
    return 0;
}

//int16_t* PCM_buffer[16]; Should be declared in main file

//block cache
int16_t* PCM_blockbuffer[16];
int PCM_blockbuffer_currentBlock = -1;

void brstm_getbuffer(const unsigned char* fileData,unsigned long sampleOffset,unsigned int bufferSamples,bool useBuffer) {
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
    if(PCM_blockbuffer_currentBlock != sampleOffset/HEAD1_blocks_samples) {
        //Read the ADPCM data
        unsigned long posOffset=0;
        if(HEAD1_codec!=2) {std::cout << "Unsupported codec.\n"; exit(220);}
        for(unsigned int c=0;c<HEAD3_num_channels;c++) {
            //Create new array of samples for the current channel
            delete[] PCM_blockbuffer[c];
            PCM_blockbuffer[c] = new int16_t[HEAD1_blocks_samples];
            
            posOffset=0+(HEAD1_blocks_size*c);
            unsigned long outputPos = 0; //position in PCM block samples output array
            
            unsigned long b=sampleOffset/HEAD1_blocks_samples;
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
            unsigned char* blockData = getSlice(fileData,HEAD1_ADPCM_offset+posOffset,currentBlockSize);
            
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
                
                outSample = (0x400 + ((scale * outSample) << 11) + HEAD3_int16_adpcm[c][clamp(cIndex, 0, 15)] * cyn1 + HEAD3_int16_adpcm[c][clamp(cIndex + 1, 0, 15)] * cyn2) >> 11;
                
                cyn2 = cyn1;
                cyn1 = clamp(outSample, -32768, 32767);
                
                PCM_blockbuffer[c][outputPos++] = cyn1;
                c_writtensamples++;
            }
            PCM_blockbuffer_currentBlock = b;
            //std::cout << ">>" << c << b << yn1 << yn2 << ps << blockData << sampleResult << '\n';
            posOffset+=HEAD1_blocks_size*HEAD3_num_channels;
        }
    }
    if(useBuffer) {
        //Put it in the pcm buffer
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
            brstm_getbuffer(fileData,sampleOffset+blockEndReachedAt,0,false);
            for(unsigned int c=0;c<HEAD3_num_channels;c++) {
                unsigned int dataIndex=0;
                for(unsigned int p=blockEndReachedAt;p<bufferSamples;p++) {
                    PCM_buffer[c][p] = PCM_blockbuffer[c][dataIndex++];
                }
            }
        }
    }
}

void brstm_close() {
    for(unsigned char i=0;i<16;i++) {
        for(unsigned char j=0;j<16;j++) {
            HEAD3_int16_adpcm[i][j] = 0;
        }
        delete[] ADPC_hsamples_1[i];
        delete[] ADPC_hsamples_2[i];
        delete[] PCM_samples[i];
        delete[] PCM_buffer[i];
        delete[] PCM_blockbuffer[i];
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
    written_samples=0;
    PCM_blockbuffer_currentBlock = -1;
}
