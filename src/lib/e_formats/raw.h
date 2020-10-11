//OpenRevolution raw file encoder
//Copyright (C) 2020 IC

#include <math.h>

unsigned char brstm_formats_encode_raw(Brstm* brstmi,signed int debugLevel,uint8_t encodeADPCM) {
    //Check for invalid requests
    //Unsupported codec
    if(brstmi->codec > 7 || brstmi->codec == 3) {
        if(debugLevel>=0) std::cout << "Unsupported codec.\n";
        return 220;
    }
    //Too many channels
    if(brstmi->num_channels > 1) {
        if(debugLevel>=0) std::cout << "Raw files with more than one channel cannot be made.\n";
        return 249;
    }
    
    bool &BOM = brstmi->BOM;
    char spinner = '/';
    
    if(debugLevel>0) std::cout << "\r" << brstm_encoder_nextspinner(spinner) << " Starting RAW encode...                " << std::flush;
    
    delete[] brstmi->encoded_file;
    unsigned char* buffer = new unsigned char[(brstmi->total_samples*brstmi->num_channels*
    (brstmi->codec == 1 ? 2 : 1)) //For 16 bit PCM
    +((brstmi->total_samples*brstmi->num_channels/14336)*4)+brstmi->num_channels*256+8192];
    unsigned long  bufpos = 0;
    unsigned long  off; //for argument 4 of brstm_encoder_writebytes when we don't write to the end of the buffer
    
    //Calculate blocks
    brstmi->blocks_samples;
    switch(brstmi->codec) {
        case 0: brstmi->blocks_samples = 8192; break;
        case 1: brstmi->blocks_samples = 4096; break;
        case 2: brstmi->blocks_samples = 14336; break;
        case 4: brstmi->blocks_samples = 24576; break;
        case 5: brstmi->blocks_samples = 16384; break;
        case 6: brstmi->blocks_samples = 32768; break;
        case 7: brstmi->blocks_samples = 65536; break;
    }
    brstmi->blocks_size = 8192;
    brstmi->total_blocks = brstmi->total_samples / brstmi->blocks_samples;
    if(brstmi->total_samples % brstmi->blocks_samples != 0) brstmi->total_blocks++;
    //Final block
    brstmi->final_block_samples = brstmi->total_samples % brstmi->blocks_samples;
    if(brstmi->final_block_samples == 0) brstmi->final_block_samples = brstmi->blocks_samples;
    brstmi->final_block_size = (
        brstmi->codec == 2 ? brstm_getBytesForAdpcmSamples(brstmi->final_block_samples)
        : brstmi->codec == 1 ? brstmi->final_block_samples * 2
        : brstmi->codec == 0 ? brstmi->final_block_samples
        : brstmi->codec == 4 ? brstm_getBytesFor2bitAdpcmSamples(brstmi->final_block_samples)
        : brstmi->codec == 5 ? ceil((double)brstmi->final_block_samples / 2)
        : brstmi->codec == 6 ? ceil((double)brstmi->final_block_samples / 4)
        : brstmi->codec == 7 ? ceil((double)brstmi->final_block_samples / 8)
        : 8192
    );
    brstmi->final_block_size_p = brstmi->final_block_size;
    while(brstmi->final_block_size_p % 0x20 != 0) {brstmi->final_block_size_p++;}
    
    //Calculate history samples
    //TODO this is copied code, split this into a separate utils function?
    int16_t LoopHS1[brstmi->num_channels];
    int16_t LoopHS2[brstmi->num_channels];
    int16_t HS1[brstmi->num_channels][brstmi->total_blocks];
    int16_t HS2[brstmi->num_channels][brstmi->total_blocks];
    if(brstmi->codec == 2 || brstmi->codec == 4) {
        if(debugLevel>0) std::cout << "\r" << brstm_encoder_nextspinner(spinner) << " Calculating history samples...       " << std::flush;
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
                //Calculate ADPCM coefs for channel
                if(brstmi->codec == 2) DSPCorrelateCoefs(brstmi->PCM_samples[c],brstmi->total_samples,brstmi->ADPCM_coefs[c]);
            }
        } else {
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
        }
    }
    
    unsigned char** ADPCMdata;
    //Encode 4-bit DSPADPCM
    if(brstmi->codec == 2) {
        if(encodeADPCM == 1) {
            ADPCMdata = new unsigned char* [brstmi->num_channels];
            for(unsigned int c=0;c<brstmi->num_channels;c++) {
                ADPCMdata[c] = new unsigned char[brstm_getBytesForAdpcmSamples(brstmi->total_samples)];
            }
            brstm_encode_adpcm(brstmi,ADPCMdata,debugLevel);
        } else {
            ADPCMdata = brstmi->ADPCM_data;
        }
    }
    //Encode 2-bit ADPCM
    else if(brstmi->codec == 4 && encodeADPCM == 1) {
        ADPCMdata = new unsigned char* [brstmi->num_channels];
        for(unsigned int c=0;c<brstmi->num_channels;c++) {
            ADPCMdata[c] = new unsigned char[brstm_getBytesFor2bitAdpcmSamples(brstmi->total_samples)];
        }
        brstm_encode_adpcm_2bit(brstmi,ADPCMdata,debugLevel);
    }
    
    if(debugLevel>0) std::cout << "\r" << brstm_encoder_nextspinner(spinner) << " Writing audio data...                                                                        " << std::flush;
    
    //Write audio data to output file buffer
    for(unsigned long b=0;b<brstmi->total_blocks-1;b++) {
        for(unsigned int c=0;c<brstmi->num_channels;c++) {
            switch(brstmi->codec) {
                //4-bit DSPADPCM and 2-bit ADPCM
                case 2: case 4: {
                    brstm_encoder_writebytes(buffer,&ADPCMdata[c][b*brstmi->blocks_size],brstmi->blocks_size,bufpos);
                    break;
                }
                //8-bit PCM
                case 0: {
                    for(unsigned int i=0; i<brstmi->blocks_samples; i++) {
                        brstm_encoder_writebyte(buffer,(brstmi->PCM_samples[c][b*brstmi->blocks_samples+i])/256,bufpos);
                    }
                    break;
                }
                //16-bit PCM
                case 1: {
                    for(unsigned int i=0; i<brstmi->blocks_samples; i++) {
                        brstm_encoder_writebytes(
                            buffer,brstm_encoder_getByteInt16(brstmi->PCM_samples[c][b*brstmi->blocks_samples+i],BOM),2,bufpos
                        );
                        if(!(b%4) && debugLevel>0) std::cout << "\r" << brstm_encoder_nextspinner(spinner) << " Writing PCM data... ("
                        << floor(((float)(b*brstmi->blocks_samples+i)/brstmi->total_samples) * 100) << "%)            ";
                    }
                    break;
                }
                //4-bit unsigned PCM
                case 5: {
                    uint8_t byte_tmp;
                    for(unsigned int i=0; i<brstmi->blocks_samples; i+=2) {
                        byte_tmp = (uint8_t)((((int32_t)brstmi->PCM_samples[c][b*brstmi->blocks_samples+i] + 32768) / 4096) << 4);
                        byte_tmp = byte_tmp | (uint8_t)(((int32_t)brstmi->PCM_samples[c][b*brstmi->blocks_samples+i+1] + 32768) / 4096);
                        brstm_encoder_writebyte(buffer,byte_tmp,bufpos);
                    }
                    break;
                }
                //2-bit unsigned PCM
                case 6: {
                    uint8_t byte_tmp;
                    for(unsigned int i=0; i<brstmi->blocks_samples; i+=4) {
                        byte_tmp = (uint8_t)((((int32_t)brstmi->PCM_samples[c][b*brstmi->blocks_samples+i] + 32768) / 16384) << 6);
                        byte_tmp = byte_tmp | (uint8_t)((((int32_t)brstmi->PCM_samples[c][b*brstmi->blocks_samples+i+1] + 32768) / 16384) << 4);
                        byte_tmp = byte_tmp | (uint8_t)((((int32_t)brstmi->PCM_samples[c][b*brstmi->blocks_samples+i+2] + 32768) / 16384) << 2);
                        byte_tmp = byte_tmp | (uint8_t)(((int32_t)brstmi->PCM_samples[c][b*brstmi->blocks_samples+i+3] + 32768) / 16384);
                        brstm_encoder_writebyte(buffer,byte_tmp,bufpos);
                    }
                    break;
                }
                //1-bit unsigned PCM
                case 7: {
                    uint8_t byte_tmp;
                    for(unsigned int i=0; i<brstmi->blocks_samples; i+=8) {
                        byte_tmp = (uint8_t)((((int32_t)brstmi->PCM_samples[c][b*brstmi->blocks_samples+i] + 32768) / 32768) << 7);
                        byte_tmp = byte_tmp | (uint8_t)((((int32_t)brstmi->PCM_samples[c][b*brstmi->blocks_samples+i+1] + 32768) / 32768) << 6);
                        byte_tmp = byte_tmp | (uint8_t)((((int32_t)brstmi->PCM_samples[c][b*brstmi->blocks_samples+i+2] + 32768) / 32768) << 5);
                        byte_tmp = byte_tmp | (uint8_t)((((int32_t)brstmi->PCM_samples[c][b*brstmi->blocks_samples+i+3] + 32768) / 32768) << 4);
                        byte_tmp = byte_tmp | (uint8_t)((((int32_t)brstmi->PCM_samples[c][b*brstmi->blocks_samples+i+4] + 32768) / 32768) << 3);
                        byte_tmp = byte_tmp | (uint8_t)((((int32_t)brstmi->PCM_samples[c][b*brstmi->blocks_samples+i+5] + 32768) / 32768) << 2);
                        byte_tmp = byte_tmp | (uint8_t)((((int32_t)brstmi->PCM_samples[c][b*brstmi->blocks_samples+i+6] + 32768) / 32768) << 1);
                        byte_tmp = byte_tmp | (uint8_t)(((int32_t)brstmi->PCM_samples[c][b*brstmi->blocks_samples+i+7] + 32768) / 32768);
                        brstm_encoder_writebyte(buffer,byte_tmp,bufpos);
                    }
                    break;
                }
            }
        }
    }
    //Final block
    for(unsigned int c=0;c<brstmi->num_channels;c++) {
        unsigned int i=brstmi->final_block_size;
        switch(brstmi->codec) {
            //4-bit DSPADPCM and 2-bit ADPCM
            case 2: case 4: {
                brstm_encoder_writebytes(buffer,&ADPCMdata[c][(brstmi->total_blocks-1)*brstmi->blocks_size],brstmi->final_block_size,bufpos);
                break;
            }
            //8-bit PCM
            case 0: {
                for(unsigned int i=0; i<brstmi->final_block_samples; i++) {
                    brstm_encoder_writebyte(buffer,(brstmi->PCM_samples[c][(brstmi->total_blocks-1)*brstmi->blocks_samples+i])/256,bufpos);
                }
                break;
            }
            //16-bit PCM
            case 1: {
                for(unsigned int i=0; i<brstmi->final_block_samples; i++) {
                    brstm_encoder_writebytes(
                        buffer,brstm_encoder_getByteInt16(brstmi->PCM_samples[c][(brstmi->total_blocks-1)*brstmi->blocks_samples+i],BOM),2,bufpos
                    );
                }
                break;
            }
            //4-bit unsigned PCM
            case 5: {
                uint8_t byte_tmp;
                for(unsigned int i=0; i<brstmi->final_block_samples; i+=2) {
                    byte_tmp = (uint8_t)((((int32_t)brstmi->PCM_samples[c][(brstmi->total_blocks-1)*brstmi->blocks_samples+i] + 32768) / 4096) << 4);
                    if(i+1 < brstmi->final_block_samples) {
                        byte_tmp = byte_tmp | (uint8_t)(((int32_t)brstmi->PCM_samples[c][(brstmi->total_blocks-1)*brstmi->blocks_samples+i+1] + 32768) / 4096);
                    }
                    brstm_encoder_writebyte(buffer,byte_tmp,bufpos);
                }
                break;
            }
            //2-bit unsigned PCM
            case 6: {
                uint8_t byte_tmp;
                for(unsigned int i=0; i<brstmi->final_block_samples; i+=4) {
                    byte_tmp = (uint8_t)((((int32_t)brstmi->PCM_samples[c][(brstmi->total_blocks-1)*brstmi->blocks_samples+i] + 32768) / 16384) << 6);
                    if(i+1 < brstmi->final_block_samples) {byte_tmp = byte_tmp | (uint8_t)((((int32_t)brstmi->PCM_samples[c][(brstmi->total_blocks-1)*brstmi->blocks_samples+i+1] + 32768) / 16384) << 4);}
                    if(i+2 < brstmi->final_block_samples) {byte_tmp = byte_tmp | (uint8_t)((((int32_t)brstmi->PCM_samples[c][(brstmi->total_blocks-1)*brstmi->blocks_samples+i+2] + 32768) / 16384) << 2);}
                    if(i+3 < brstmi->final_block_samples) {byte_tmp = byte_tmp | (uint8_t)(((int32_t)brstmi->PCM_samples[c][(brstmi->total_blocks-1)*brstmi->blocks_samples+i+3] + 32768) / 16384);}
                    brstm_encoder_writebyte(buffer,byte_tmp,bufpos);
                }
                break;
            }
            //1-bit unsigned PCM
            case 7: {
                uint8_t byte_tmp;
                for(unsigned int i=0; i<brstmi->final_block_samples; i+=8) {
                    byte_tmp = (uint8_t)((((int32_t)brstmi->PCM_samples[c][(brstmi->total_blocks-1)*brstmi->blocks_samples+i] + 32768) / 32768) << 7);
                    if(i+1 < brstmi->final_block_samples) {byte_tmp = byte_tmp | (uint8_t)((((int32_t)brstmi->PCM_samples[c][(brstmi->total_blocks-1)*brstmi->blocks_samples+i+1] + 32768) / 32768) << 6);}
                    if(i+2 < brstmi->final_block_samples) {byte_tmp = byte_tmp | (uint8_t)((((int32_t)brstmi->PCM_samples[c][(brstmi->total_blocks-1)*brstmi->blocks_samples+i+2] + 32768) / 32768) << 5);}
                    if(i+3 < brstmi->final_block_samples) {byte_tmp = byte_tmp | (uint8_t)((((int32_t)brstmi->PCM_samples[c][(brstmi->total_blocks-1)*brstmi->blocks_samples+i+3] + 32768) / 32768) << 4);}
                    if(i+4 < brstmi->final_block_samples) {byte_tmp = byte_tmp | (uint8_t)((((int32_t)brstmi->PCM_samples[c][(brstmi->total_blocks-1)*brstmi->blocks_samples+i+4] + 32768) / 32768) << 3);}
                    if(i+5 < brstmi->final_block_samples) {byte_tmp = byte_tmp | (uint8_t)((((int32_t)brstmi->PCM_samples[c][(brstmi->total_blocks-1)*brstmi->blocks_samples+i+5] + 32768) / 32768) << 2);}
                    if(i+6 < brstmi->final_block_samples) {byte_tmp = byte_tmp | (uint8_t)((((int32_t)brstmi->PCM_samples[c][(brstmi->total_blocks-1)*brstmi->blocks_samples+i+6] + 32768) / 32768) << 1);}
                    if(i+7 < brstmi->final_block_samples) {byte_tmp = byte_tmp | (uint8_t)(((int32_t)brstmi->PCM_samples[c][(brstmi->total_blocks-1)*brstmi->blocks_samples+i+7] + 32768) / 32768);}
                    brstm_encoder_writebyte(buffer,byte_tmp,bufpos);
                }
                break;
            }
        }
        if((brstmi->codec == 2 || brstmi->codec == 4) && encodeADPCM == 1) delete[] ADPCMdata[c]; //delete the ADPCM data only if we made it locally
        //padding
        while(i<brstmi->final_block_size_p) {
            brstm_encoder_writebyte(buffer,0x00,bufpos);
            i++;
        }
    }
    if((brstmi->codec == 2 || brstmi->codec == 4) && encodeADPCM == 1) delete[] ADPCMdata; //delete the ADPCM data only if we made it locally
    
    //copy finished file to brstm_encoded_data
    brstmi->encoded_file = new unsigned char[bufpos];
    memcpy(brstmi->encoded_file,buffer,bufpos);
    delete[] buffer;
    brstmi->encoded_file_size = bufpos;
    
    if(debugLevel>0) std::cout << "\r" << "RAW encoding done                                                     \n" << std::flush;
    
    return 0;
}
