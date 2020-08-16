//OpenRevolution BFSTM reader
//Copyright (C) 2020 IC

//This is mostly shared code with the BCSTM reader, the two formats are very similar.

unsigned char brstm_formats_read_bfstm(Brstm* brstmi,const unsigned char* fileData,signed int debugLevel,uint8_t decodeAudio) {
    bool &BOM = brstmi->BOM;
    
    //BFSTM file magic words
    const char* emagic1 = "FSTM";
    const char* emagic2 = "INFO";
    const char* emagic3 = "SEEK";
    const char* emagic4 = "DATA";
    char* magicstr = brstm_getSliceAsString(fileData,0,4);
    
    if(strcmp(magicstr,emagic1) != 0) {
        if(debugLevel>=0) {std::cout << "Invalid BFSTM file.\n";}
        return 255;
    }
    
    //Header
    //Byte order mark
    if(brstm_getSliceAsInt16Sample(fileData,0x04,1) == -257) {
        BOM = 1; //Big endian
    } else {
        BOM = 0; //Little endian
    }
    //uint16_t header_size = brstm_getSliceAsNumber(fileData,0x06,2,BOM);
    //uint32_t file_size       = brstm_getSliceAsNumber(fileData,0x0C,4,BOM);
    //uint16_t num_file_chunks = brstm_getSliceAsNumber(fileData,0x10,2,BOM);
    
    uint32_t INFO_offset = brstm_getSliceAsNumber(fileData,0x18,4,BOM);
    //uint32_t INFO_size   = brstm_getSliceAsNumber(fileData,0x1C,4,BOM);
    uint32_t SEEK_offset = brstm_getSliceAsNumber(fileData,0x24,4,BOM);
    //uint32_t SEEK_size   = brstm_getSliceAsNumber(fileData,0x28,4,BOM);
    uint32_t DATA_offset = brstm_getSliceAsNumber(fileData,0x30,4,BOM);
    //uint32_t DATA_size   = brstm_getSliceAsNumber(fileData,0x34,4,BOM);
    
    //INFO chunk
    if(INFO_offset == (uint32_t)-1) {
        if(debugLevel>=0) {std::cout << "INFO chunk does not exist.\n";}
        return 250;
    }
    magicstr = brstm_getSliceAsString(fileData,INFO_offset,4);
    if(strcmp(magicstr,emagic2) != 0) {
        if(debugLevel>=0) {std::cout << "Invalid INFO chunk.";}
        return 250;
    }
    //INFO header (offsets to other chunks in INFO)
    int32_t INFO_stream_offset  = brstm_getSliceAsNumber(fileData,0x0C+INFO_offset,4,BOM);
    //int32_t INFO_track_offset   = brstm_getSliceAsNumber(fileData,0x14+INFO_offset,4,BOM);
    int32_t INFO_channel_offset = brstm_getSliceAsNumber(fileData,0x1C+INFO_offset,4,BOM);
    
    //Stream info
    if(INFO_stream_offset != -1) {
        INFO_stream_offset += INFO_offset + 8;
        
        brstmi->codec        = brstm_getSliceAsNumber(fileData,0x00+INFO_stream_offset,1,BOM);
        brstmi->loop_flag    = brstm_getSliceAsNumber(fileData,0x01+INFO_stream_offset,1,BOM);
        brstmi->num_channels = brstm_getSliceAsNumber(fileData,0x02+INFO_stream_offset,1,BOM);
        //0x03: Num regions??
        brstmi->sample_rate    = brstm_getSliceAsNumber(fileData,0x04+INFO_stream_offset,4,BOM);
        brstmi->loop_start     = brstm_getSliceAsNumber(fileData,0x08+INFO_stream_offset,4,BOM);
        brstmi->total_samples  = brstm_getSliceAsNumber(fileData,0x0C+INFO_stream_offset,4,BOM);
        brstmi->total_blocks   = brstm_getSliceAsNumber(fileData,0x10+INFO_stream_offset,4,BOM);
        brstmi->blocks_size    = brstm_getSliceAsNumber(fileData,0x14+INFO_stream_offset,4,BOM);
        brstmi->blocks_samples = brstm_getSliceAsNumber(fileData,0x18+INFO_stream_offset,4,BOM);
        brstmi->final_block_size    = brstm_getSliceAsNumber(fileData,0x1C+INFO_stream_offset,4,BOM);
        brstmi->final_block_samples = brstm_getSliceAsNumber(fileData,0x20+INFO_stream_offset,4,BOM);
        brstmi->final_block_size_p  = brstm_getSliceAsNumber(fileData,0x24+INFO_stream_offset,4,BOM);
        //0x28: Bytes per SEEK chunk, not important and should always be the same
        //0x2C: SEEK sample interval?
        //0x30: Sample data flag?
        brstmi->audio_offset = brstm_getSliceAsNumber(fileData,0x34+INFO_stream_offset,4,BOM) + 0x08 + DATA_offset;
        //0x38: Size of region info?
        //0x3C -> 0x44: other unkown region info stuff
        if(!(brstmi->codec >= 0 && brstmi->codec < 3)) {
            if(debugLevel>=0) {std::cout << "Unsupported codec.\n";}
            return 220;
        }
        if(brstmi->num_channels > 16) {
            if(debugLevel>=0) {std::cout << "Too many channels, max supported is 16.\n";}
            return 249;
        }
    }
    
    //TODO the weird track info chunk
    
    //Channel info
    if(INFO_channel_offset != -1) {
        //Reference table
        INFO_channel_offset += INFO_offset + 8;
        uint32_t num_references = brstm_getSliceAsNumber(fileData,0x00+INFO_channel_offset,4,BOM);
        if(num_references > 16) {
            if(debugLevel>=0) {std::cout << "Too many channels, max supported is 16.\n";}
            return 249;
        }
        uint32_t offptr = 4;
        int32_t  offsets[num_references];
        for(unsigned int i=0;i<num_references;i++) {
            offptr += 4;
            offsets[i] = brstm_getSliceAsNumber(fileData,offptr+INFO_channel_offset,4,BOM);
            offptr += 4;
            if(offsets[i] != -1) {
                //Read channel data at offset
                //yet another offset, I know a person who knows a person who knows a person
                int32_t offset2 = brstm_getSliceAsNumber(fileData,offsets[i]+INFO_channel_offset+0x04,4,BOM);
                if(offset2 != -1) {
                    //DSPADPCM coefs
                    for(unsigned int c=0;c<16;c++) {
                        brstmi->ADPCM_coefs[i][c] = brstm_getSliceAsInt16Sample(fileData,c*2+offset2+offsets[i]+INFO_channel_offset,BOM);
                    }
                    //other predictor scales and history samples not important
                }
            }
        }
    }
    
    //SEEK chunk (ADPC)
    //Allocate hsample memory
    for(unsigned int c=0;c<brstmi->num_channels;c++) {
        brstmi->ADPCM_hsamples_1[c] = new int16_t[brstmi->total_blocks];
        brstmi->ADPCM_hsamples_2[c] = new int16_t[brstmi->total_blocks];
        brstmi->ADPCM_hsamples_1[c][0] = 0;
        brstmi->ADPCM_hsamples_2[c][0] = 0;
    }
    if(brstmi->codec == 2 && SEEK_offset != (uint32_t)-1) {
        magicstr = brstm_getSliceAsString(fileData,SEEK_offset,4);
        if(strcmp(magicstr,emagic3) != 0) {
            if(debugLevel>=0) {std::cout << "Invalid SEEK chunk.";}
            return 240;
        }
        
        //History samples broken in BFSTM for some dumb reason....
        /*
        uint32_t offptr = SEEK_offset + 8;
        for(unsigned long b=0;b<brstmi->total_blocks;b++) {
            for(unsigned int c=0;c<brstmi->num_channels;c++) {
                brstmi->ADPCM_hsamples_1[c][b] = brstm_getSliceAsInt16Sample(fileData,offptr,BOM);
                offptr += 2;
                brstmi->ADPCM_hsamples_2[c][b] = brstm_getSliceAsInt16Sample(fileData,offptr,BOM);
                offptr += 2;
            }
        }
        */
    }
    
    //Write standard track information
    brstmi->num_tracks = (brstmi->num_channels > 1 && brstmi->num_channels%2 == 0) ? brstmi->num_channels/2 : brstmi->num_channels;
    if(brstmi->num_tracks > 8) {
        if(debugLevel>=0) {std::cout << "Too many tracks, max supported is 8.\n";}
        return 248;
    }
    unsigned char track_num_channels = brstmi->num_tracks*2 == brstmi->num_channels ? 2 : 1;
    brstmi->track_desc_type = 0;
    for(unsigned char c=0; c<brstmi->num_channels; c++) {
        brstmi->track_num_channels[c/track_num_channels] = track_num_channels;
        if(track_num_channels == 1 || c%2 == 0) brstmi->track_lchannel_id[c/track_num_channels] = c;
        if(track_num_channels == 2 && c%2 == 1) brstmi->track_rchannel_id[c/track_num_channels] = c;
    }
    if(brstmi->num_tracks>1 && debugLevel>=0) {std::cout << "Warning: BFSTM track information is guessed\n";}
    
    //DATA chunk
    if(DATA_offset == (uint32_t)-1) {
        if(debugLevel>=0) {std::cout << "DATA chunk does not exist.\n";}
        return 230;
    }
    magicstr = brstm_getSliceAsString(fileData,DATA_offset,4);
    if(strcmp(magicstr,emagic4) != 0) {
        if(debugLevel>=0) {std::cout << "Invalid DATA chunk.";}
        return 230;
    }
    
    if(decodeAudio) {
        unsigned long posOffset=0;
        
        for(unsigned int c=0;c<brstmi->num_channels;c++) {
            //Create new array of samples for the current channel
            switch(decodeAudio) {
                case 1: brstmi->PCM_samples[c] = new int16_t[brstmi->total_samples]; break;
                case 2: brstmi->ADPCM_data [c] = new unsigned char[brstm_getBytesForAdpcmSamples(brstmi->total_samples)]; break;
            }
            
            posOffset=0+(brstmi->blocks_size*c);
            unsigned long outputPos = 0; //position in PCM samples or ADPCM data output array
            for(unsigned long b=0;b<brstmi->total_blocks;b++) {
                //Read every block
                unsigned int currentBlockSize    = brstmi->blocks_size;
                //Final block
                if(b==brstmi->total_blocks-1) {
                    currentBlockSize    = brstmi->final_block_size;
                }
                if(b>=brstmi->total_blocks-1 && c>0) {
                    //Go back to the previous position
                    posOffset-=brstmi->blocks_size*brstmi->num_channels;
                    //Go to the next block in position of first channel
                    posOffset+=brstmi->blocks_size*(brstmi->num_channels-c);
                    //Jump to the correct channel in the final block
                    posOffset+=brstmi->final_block_size_p*c;
                }
                
                
                if(decodeAudio == 1) {
                    //Decode audio normally
                    brstm_decode_block(brstmi,b,c,fileData,0,brstmi->PCM_samples,b*brstmi->blocks_samples);
                } else {
                    //Write raw data to ADPCM_data
                    if(brstmi->codec!=2) {
                        if(debugLevel>=0) {
                            std::cout << "Cannot write raw ADPCM data because the codec is not ADPCM.\n";
                        }
                        return 222;
                    }
                    //Get data from just the current block
                    unsigned char* blockData = brstm_getSlice(fileData,brstmi->audio_offset+posOffset,currentBlockSize);
                    for(unsigned int i=0; i<currentBlockSize; i++) {
                        brstmi->ADPCM_data[c][outputPos++] = blockData[i];
                    }
                }
                
                posOffset+=brstmi->blocks_size*brstmi->num_channels;
            }
        }
    }
    
    return 0;
}
