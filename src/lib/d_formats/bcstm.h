//OpenRevolution BCSTM/BFSTM reader
//Copyright (C) 2020 IC

//BCSTM and BFSTM are almost the same, the main reader function will take a special argument so it can also be used as the BFSTM reader.

unsigned char brstm_formats_multi_read_bcstm_bfstm(Brstm* brstmi, const unsigned char* fileData, signed int debugLevel, uint8_t decodeAudio, bool eformat);

unsigned char brstm_formats_read_bcstm(Brstm* brstmi, const unsigned char* fileData, signed int debugLevel, uint8_t decodeAudio) {
    return brstm_formats_multi_read_bcstm_bfstm(brstmi,fileData,debugLevel,decodeAudio,0);
}

//bool eformat: 0 = BCSTM, 1 = BFSTM.
unsigned char brstm_formats_multi_read_bcstm_bfstm(Brstm* brstmi, const unsigned char* fileData, signed int debugLevel, uint8_t decodeAudio, bool eformat) {
    bool &BOM = brstmi->BOM;
    
    //Strings and other things that change from BCSTM to BFSTM.
    const char* eformat_s[2] = {"BCSTM","BFSTM"};
    // const char  eformat_l[2] = {'C','F'};
    
    //BCSTM/BFSTM file magic words
    const char* emagic1 = (eformat == 1 ? "FSTM" : "CSTM");
    const char* emagic2 = "INFO";
    const char* emagic3 = "SEEK";
    const char* emagic4 = "DATA";
    char* magicstr = brstm_getSliceAsString(fileData,0,4);
    
    if(strcmp(magicstr,emagic1) != 0) {
        if(debugLevel>=0) {std::cout << "Invalid " << eformat_s[eformat] << " file.\n";}
        return 255;
    }
    
    //Byte order mark
    if(brstm_getSliceAsInt16Sample(fileData,0x04,1) == -257) {
        BOM = 1; //Big endian
    } else {
        BOM = 0; //Little endian
    }
    //uint16_t header_size = brstm_getSliceAsNumber(fileData,0x06,2,BOM);
    //uint32_t file_size       = brstm_getSliceAsNumber(fileData,0x0C,4,BOM);
    
    //Get all chunk offsets correctly.
    uint16_t num_file_chunks = brstm_getSliceAsNumber(fileData,0x10,2,BOM);
    uint32_t INFO_offset = -1; //Marker 0x4000
    uint32_t SEEK_offset = -1; //Marker 0x4001
    uint32_t DATA_offset = -1; //Marker 0x4002
    uint32_t REGN_offset = -1; //Marker 0x4003 - This chunk is unsupported and will be ignored.
    for(unsigned int c=0; c<num_file_chunks; c++) {
        uint16_t chunk_marker = brstm_getSliceAsNumber(fileData,(12*c)+0x14,2,BOM);
        uint32_t chunk_offset = brstm_getSliceAsNumber(fileData,(12*c)+0x14+4,4,BOM);
        switch(chunk_marker) {
            case 0x4000: {
                INFO_offset = chunk_offset;
                break;
            }
            case 0x4001: {
                SEEK_offset = chunk_offset;
                break;
            }
            case 0x4002: {
                DATA_offset = chunk_offset;
                break;
            }
            case 0x4003: {
                if(debugLevel>=0) {
                    std::cout << "Warning: REGN chunk is not supported and will be ignored.\n";
                }
                REGN_offset = chunk_offset;
                brstmi->warn_unsupported_chunk = 1;
                break;
            }
            default: {
                if(debugLevel>=0) {
                    std::cout << "Warning: Unrecognized chunk with code 0x" << std::hex << chunk_marker << std::dec << ".\n";
                }
                brstmi->warn_unsupported_chunk = 1;
                break;
            }
        }
    }
    
    //INFO chunk
    if(INFO_offset == (uint32_t)-1) {
        if(debugLevel>=0) {std::cout << "INFO chunk does not exist.\n";}
        return 250;
    }
    magicstr = brstm_getSliceAsString(fileData,INFO_offset,4);
    if(strcmp(magicstr,emagic2) != 0) {
        if(debugLevel>=0) {std::cout << "Invalid INFO chunk.\n";}
        return 250;
    }
    //INFO header (offsets to other chunks in INFO)
    int32_t INFO_stream_offset  = brstm_getSliceAsNumber(fileData,0x0C+INFO_offset,4,BOM);
    int32_t INFO_track_offset   = brstm_getSliceAsNumber(fileData,0x14+INFO_offset,4,BOM);
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
        if(!(brstmi->codec < 3)) {
            if(debugLevel>=0) {std::cout << "Unsupported codec.\n";}
            return 220;
        }
        if(brstmi->num_channels > 16) {
            if(debugLevel>=0) {std::cout << "Too many channels, max supported is 16.\n";}
            return 249;
        }
    }
    
    //Track info
    if(INFO_track_offset != -1) {
        INFO_track_offset += INFO_offset + 8;
        //Reference table
        uint32_t num_references = brstm_getSliceAsNumber(fileData,0x00+INFO_track_offset,4,BOM);
        if(num_references > 8) {
            if(debugLevel>=0) {std::cout << "Too many tracks, max supported is 8.\n";}
            return 248;
        }
        
        brstmi->num_tracks = num_references;
        brstmi->track_desc_type = 1;
        
        //Information
        int32_t offsets[num_references];
        //True if a track with more than 2 channels was found.
        bool unsupported_track_flag = 0;
        for(uint32_t i=0; i<num_references; i++) {
            //Get offset from reference table
            offsets[i] = brstm_getSliceAsNumber(fileData,0x08*i+0x08+INFO_track_offset,4,BOM);
            if(offsets[i] == -1) {
                if(debugLevel>=0) {std::cout << "Invalid track information.\n";}
                return 244;
            }
            //Read data
            brstmi->track_volume [i] = brstm_getSliceAsNumber(fileData,INFO_track_offset+offsets[i]+0x00,1,BOM);
            brstmi->track_panning[i] = brstm_getSliceAsNumber(fileData,INFO_track_offset+offsets[i]+0x01,1,BOM);
            //Channel index
            int32_t chindex_offset = brstm_getSliceAsNumber(fileData,INFO_track_offset+offsets[i]+0x08,4,BOM);
            if(chindex_offset == -1) {
                if(debugLevel>=0) {std::cout << "Invalid track information.\n";}
                return 244;
            }
            uint32_t chindex_count = brstm_getSliceAsNumber(fileData,INFO_track_offset+offsets[i]+chindex_offset+0x00,4,BOM);
            if(chindex_count == 0) {
                if(debugLevel>=0) {std::cout << "Invalid track information.\n";}
                return 244;
            }
            if(chindex_count > 2) {
                unsupported_track_flag = 1;
                chindex_count = 2;
            }
            brstmi->track_num_channels[i] = chindex_count;
            brstmi->track_lchannel_id [i] = brstm_getSliceAsNumber(fileData,INFO_track_offset+offsets[i]+chindex_offset+0x04,1,BOM);
            if(chindex_count >= 2) brstmi->track_rchannel_id[i] = brstm_getSliceAsNumber(fileData,INFO_track_offset+offsets[i]+chindex_offset+0x05,1,BOM);
            //Check if channel index is valid
            if(brstmi->track_lchannel_id[i] >= brstmi->num_channels || brstmi->track_rchannel_id[i] >= brstmi->num_channels) {
                if(debugLevel>=0) {std::cout << "Invalid track information.\n";}
                return 244;
            }
        }
        
        if(unsupported_track_flag) {
            if(debugLevel>=0) {std::cout << "Warning: This " << eformat_s[eformat] << " file has a track with more than 2 channels which is not currently supported.\n";}
            brstmi->warn_unsupported_channels = 1;
        }
    }
    //Guess and write standard track information if the track information subchunk does not exist.
    else {
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
        
        if(brstmi->num_tracks>1 && debugLevel>=0) {std::cout << "Warning: " << eformat_s[eformat] << " track information is guessed\n";}
        brstmi->warn_guessed_track_info = 1;
    }
    
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
            if(debugLevel>=0) {std::cout << "Invalid SEEK chunk.\n";}
            return 240;
        }
        
        uint32_t offptr = SEEK_offset + 8;
        for(unsigned long b=0;b<brstmi->total_blocks;b++) {
            for(unsigned int c=0;c<brstmi->num_channels;c++) {
                brstmi->ADPCM_hsamples_1[c][b] = brstm_getSliceAsInt16Sample(fileData,offptr,BOM);
                offptr += 2;
                brstmi->ADPCM_hsamples_2[c][b] = brstm_getSliceAsInt16Sample(fileData,offptr,BOM);
                offptr += 2;
            }
        }
        
    }
    
    //TODO: REGN chunk?
    
    //DATA chunk
    //Do not read the DATA chunk header when not decoding audio, when using brstm_fstream this would cause an error
    //when reading files with a REGN chunk because the fstream reader is not smart enough to read the offsets correctly,
    //and will only give the data of the file until the REGN chunk.
    if(decodeAudio) {
        if(DATA_offset == (uint32_t)-1) {
            if(debugLevel>=0) {std::cout << "DATA chunk does not exist.\n";}
            return 230;
        }
        magicstr = brstm_getSliceAsString(fileData,DATA_offset,4);
        if(strcmp(magicstr,emagic4) != 0) {
            if(debugLevel>=0) {std::cout << "Invalid DATA chunk.\n";}
            return 230;
        }
    }
    
    if(decodeAudio) {
        unsigned char awrite_res = brstm_doStandardAudioWrite(brstmi, fileData, debugLevel, decodeAudio);
        if(awrite_res > 128) return awrite_res;
    }
    
    return 0;
}
