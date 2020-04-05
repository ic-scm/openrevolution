//Revolution BWAV reader
//Copyright (C) 2020 Extrasklep

unsigned char brstm_formats_read_bwav(Brstm* brstmi,const unsigned char* fileData,signed int debugLevel,uint8_t decodeAudio) {
    bool &BOM = brstmi->BOM;
    //Byte Order Mark
    if(brstm_getSliceAsInt16Sample(fileData,0x04,1) == -257) {
        BOM = 1; //Big endian
    } else {
        BOM = 0; //Little endian
    }
    //BWAV is weird and stupid
    brstmi->num_channels = brstm_getSliceAsNumber(fileData,0x0E,2,BOM);
    brstmi->codec = brstm_getSliceAsNumber(fileData,0x10,2,BOM) + 1; //add 1 so the codec number is like BRSTM's codec number
    brstmi->sample_rate = brstm_getSliceAsNumber(fileData,0x14,4,BOM);
    brstmi->total_samples = brstm_getSliceAsNumber(fileData,0x18,4,BOM);
    brstmi->loop_flag = ( (int32_t)brstm_getSliceAsNumber(fileData,0x4C,4,BOM) != -1 );
    brstmi->loop_start = brstm_getSliceAsNumber(fileData,0x50,4,BOM);
    brstmi->audio_offset = brstm_getSliceAsNumber(fileData,0x40,4,BOM);
    brstmi->total_blocks = 1;
    brstmi->blocks_size = brstmi->num_channels > 1 ? (brstm_getSliceAsNumber(fileData,0x8C,4,BOM) - brstmi->audio_offset) : (brstmi->codec == 1 ? brstmi->total_samples*2 : brstmi->total_samples/1.75);
    brstmi->blocks_samples = brstmi->total_samples;
    brstmi->final_block_size = brstmi->blocks_size;
    brstmi->final_block_samples = brstmi->blocks_samples;
    brstmi->final_block_size_p = brstmi->final_block_size;
    
    //Write BRSTM standard track data
    brstmi->num_tracks = (brstmi->num_channels > 1 && brstmi->num_channels%2 == 0) ? brstmi->num_channels/2 : brstmi->num_channels;
    unsigned char track_num_channels = brstmi->num_tracks*2 == brstmi->num_channels ? 2 : 1;
    brstmi->track_desc_type = 0;
    for(unsigned char c=0; c<brstmi->num_channels; c++) {
        brstmi->track_num_channels[c/track_num_channels] = track_num_channels;
        if(track_num_channels == 1 || c%2 == 0) brstmi->track_lchannel_id[c/track_num_channels] = c;
        if(track_num_channels == 2 && c%2 == 1) brstmi->track_rchannel_id[c/track_num_channels] = c;
        
        //Read coefs
        if(brstmi->codec == 2) {
            for(unsigned int i=0;i<16;i++) {
                brstmi->ADPCM_coefs[c][i] = brstm_getSliceAsInt16Sample(fileData,0x20+i*2+c*0x4C,BOM);
            }
        }
    }
    if(brstmi->num_tracks>1 && debugLevel>=0) {std::cout << "Warning: Tracks are assumed\n";}
    
    //Audio
    //This is stupid copied code but works for now and eventually I plan to move the decoder into a function
    if(decodeAudio) {
        //Read the ADPCM data
        unsigned long decoded_samples=0;
        
        unsigned long posOffset=0;
        
        for(unsigned int c=0;c<brstmi->num_channels;c++) {
            //Create new array of samples for the current channel
            switch(decodeAudio) {
                case 1: brstmi->PCM_samples[c] = new int16_t[brstmi->total_samples]; break;
                case 2: brstmi->ADPCM_data [c] = new unsigned char[brstmi->blocks_size]; break;
            }
            
            posOffset=0+(brstmi->blocks_size*c);
            unsigned long outputPos = 0; //position in PCM samples or ADPCM data output array
            for(unsigned long b=0;b<brstmi->total_blocks;b++) {
                //Read every block
                unsigned int currentBlockSize    = brstmi->blocks_size;
                unsigned int currentBlockSamples = brstmi->blocks_samples;
                //Final block
                if(b==brstmi->total_blocks-1) {
                    currentBlockSize    = brstmi->final_block_size;
                    currentBlockSamples = brstmi->final_block_samples;
                }
                if(b>=brstmi->total_blocks-1 && c>0) {
                    //Go back to the previous position
                    posOffset-=brstmi->blocks_size*brstmi->num_channels;
                    //Go to the next block in position of first channel
                    posOffset+=brstmi->blocks_size*(brstmi->num_channels-c);
                    //Jump to the correct channel in the final block
                    posOffset+=brstmi->final_block_size_p*c;
                }
                //Get data from just the current block
                unsigned char* blockData = brstm_getSlice(fileData,brstmi->audio_offset+posOffset,currentBlockSize);
                
                if(decodeAudio == 1) {
                    if(brstmi->codec == 2) {
                        //Decode 4 bit ADPCM
                        const unsigned char ps = blockData[0];
                        const   signed int  yn1 = 0, yn2 = 0;
                        
                        //Magic adapted from brawllib's ADPCMState.cs
                        signed int 
                        cps = ps,
                        cyn1 = yn1,
                        cyn2 = yn2;
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
                            
                            cyn2 = cyn1;
                            cyn1 = brstm_clamp(outSample, -32768, 32767);
                            
                            brstmi->PCM_samples[c][outputPos++] = cyn1;
                            decoded_samples++;
                        }
                    } else if(brstmi->codec == 1) {
                        for(unsigned long s=0;s<currentBlockSamples;s++) {
                            brstmi->PCM_samples[c][outputPos++] = brstm_getSliceAsInt16Sample(fileData,brstmi->audio_offset+posOffset+s*2,BOM);
                            decoded_samples++;
                        }
                    } else {
                        if(debugLevel>=0) {std::cout << "Unsupported codec.\n";}
                        return 220;
                    }
                } else {
                    if(brstmi->codec == 2) {
                        //Write raw data to ADPCM_data
                        for(unsigned int i=0; i<currentBlockSize; i++) {
                            brstmi->ADPCM_data[c][outputPos++] = blockData[i];
                        }
                    } else {
                        if(debugLevel>=0) {std::cout << "Codec is not ADPCM.\n";}
                        return 220;
                    }
                }
                
                posOffset+=brstmi->blocks_size*brstmi->num_channels;
            }
        }
        if(debugLevel>0) {std::cout << "Decoded PCM samples: " << decoded_samples << '\n';}
    } else {
        if(debugLevel>=0) {std::cout << "Warning: Realtime decoding works like full decoding for this format\n";}
        for(unsigned char c=0;c<brstmi->num_channels;c++) {
            brstmi->ADPCM_hsamples_1[c] = new int16_t[1];
            brstmi->ADPCM_hsamples_2[c] = new int16_t[1];
            brstmi->ADPCM_hsamples_1[c][0] = 0;
            brstmi->ADPCM_hsamples_2[c][0] = 0;
        }
    }
    
    return 0;
}
