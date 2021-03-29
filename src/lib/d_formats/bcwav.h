//OpenRevolution BCWAV/BFWAV reader
//Copyright (C) 2021 IC

//BCWAV and BFWAV are almost the same, the main reader function will take a special argument so it can also be used as the BFWAV reader.

unsigned char brstm_formats_multi_read_bcwav_bfwav(Brstm* brstmi, const unsigned char* fileData, signed int debugLevel, uint8_t decodeAudio, bool eformat);

unsigned char brstm_formats_read_bcwav(Brstm* brstmi, const unsigned char* fileData, signed int debugLevel, uint8_t decodeAudio) {
    return brstm_formats_multi_read_bcwav_bfwav(brstmi, fileData, debugLevel, decodeAudio, 0);
}

//bool eformat: 0 = BCWAV, 1 = BFWAV.
unsigned char brstm_formats_multi_read_bcwav_bfwav(Brstm* brstmi, const unsigned char* fileData, signed int debugLevel, uint8_t decodeAudio, bool eformat) {
    bool &BOM = brstmi->BOM;
    
    //Strings and other things that change from BCWAV to BFWAV.
    const char* eformat_s[2] = {"BCWAV","BFWAV"};
    
    //BCWAV/BFWAV file magic words
    const char* emagic1 = (eformat == 1 ? "FWAV" : "CWAV");
    const char* emagic2 = "INFO";
    const char* emagic3 = "DATA";
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
    //uint32_t file_size   = brstm_getSliceAsNumber(fileData,0x0C,4,BOM);
    
    //Get all chunk offsets correctly.
    uint16_t num_file_chunks = brstm_getSliceAsNumber(fileData,0x10,2,BOM);
    uint32_t INFO_offset = -1; //Marker 0x7000
    uint32_t DATA_offset = -1; //Marker 0x7001
    
    for(unsigned int c=0; c<num_file_chunks; c++) {
        uint16_t chunk_marker = brstm_getSliceAsNumber(fileData,(12*c)+0x14,2,BOM);
        uint32_t chunk_offset = brstm_getSliceAsNumber(fileData,(12*c)+0x14+4,4,BOM);
        
        switch(chunk_marker) {
            case 0x7000: {
                INFO_offset = chunk_offset;
                break;
            }
            case 0x7001: {
                DATA_offset = chunk_offset;
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
    
    int32_t INFO_stream_offset  = INFO_offset + 0x08;
    int32_t INFO_channel_offset = INFO_stream_offset + 0x14;
    
    //Stream info
    brstmi->codec          = brstm_getSliceAsNumber(fileData,0x00+INFO_stream_offset,1,BOM);
    brstmi->loop_flag      = brstm_getSliceAsNumber(fileData,0x01+INFO_stream_offset,1,BOM);
    brstmi->sample_rate    = brstm_getSliceAsNumber(fileData,0x04+INFO_stream_offset,4,BOM);
    brstmi->loop_start     = brstm_getSliceAsNumber(fileData,0x08+INFO_stream_offset,4,BOM);
    brstmi->total_samples  = brstm_getSliceAsNumber(fileData,0x0C+INFO_stream_offset,4,BOM);
    
    if(!(brstmi->codec < 3)) {
        if(debugLevel>=0) {std::cout << "Unsupported codec.\n";}
        return 220;
    }
    
    //Channel info
    
    //Reference table, number of references (and channels)
    brstmi->num_channels = brstm_getSliceAsNumber(fileData,0x00+INFO_channel_offset,4,BOM);
    
    if(brstmi->num_channels > 16) {
        if(debugLevel>=0) {std::cout << "Too many channels, max supported is 16.\n";}
        return 249;
    }
    if(brstmi->num_channels == 0) {
        if(debugLevel>=0) {std::cout << "Invalid channel count.\n";}
        return 255;
    }
    
    //Offsets to audio data for every channel (from beginning of file)
    uint32_t ch_audio_offsets[brstmi->num_channels];
    
    //Allocate ADPCM hsamples
    for(unsigned char c=0;c<brstmi->num_channels;c++) {
        brstmi->ADPCM_hsamples_1[c] = new int16_t[1];
        brstmi->ADPCM_hsamples_2[c] = new int16_t[1];
    }
    
    //Read the reference table and data
    {
        uint32_t offptr = 4;
        int32_t  offsets[brstmi->num_channels];
        for(unsigned int i = 0; i < brstmi->num_channels; i++) {
            offptr += 4;
            offsets[i] = brstm_getSliceAsNumber(fileData,offptr+INFO_channel_offset,4,BOM);
            offptr += 4;
            
            if(offsets[i] != -1) {
                //Read channel data at offset
                //Audio data offset for this channel in DATA chunk
                ch_audio_offsets[i] = brstm_getSliceAsNumber(fileData,offsets[i]+INFO_channel_offset+0x04,4,BOM) + DATA_offset;
                
                //DSPADPCM channel data
                if(brstmi->codec == 2) {
                    int32_t cdata_offset = brstm_getSliceAsNumber(fileData,offsets[i]+INFO_channel_offset+0x0C,4,BOM);
                    
                    if(cdata_offset != -1) {
                        //DSPADPCM coefs
                        for(unsigned int c=0;c<16;c++) {
                            brstmi->ADPCM_coefs[i][c] = brstm_getSliceAsInt16Sample(fileData,c*2+cdata_offset+offsets[i]+INFO_channel_offset,BOM);
                        }
                        //History samples
                        brstmi->ADPCM_hsamples_1[i][0] = brstm_getSliceAsInt16Sample(fileData,0x22+cdata_offset+offsets[i]+INFO_channel_offset,BOM);
                        brstmi->ADPCM_hsamples_2[i][0] = brstm_getSliceAsInt16Sample(fileData,0x24+cdata_offset+offsets[i]+INFO_channel_offset,BOM);
                    }
                }
            }
            
        }
        
    }
    
    brstmi->audio_offset = ch_audio_offsets[0];
    
    
    //Calculate compatible block information
    brstmi->total_blocks = 1;
    brstmi->blocks_size = brstmi->num_channels > 1 ? (ch_audio_offsets[1] - ch_audio_offsets[0]) :
    //In order: 8-bit PCM, 16-bit PCM, 4-bit DSPADPCM
    (brstmi->codec == 0 ? brstmi->total_samples : brstmi->codec == 1 ? brstmi->total_samples*2 : brstm_getBytesForAdpcmSamples(brstmi->total_samples));
    
    brstmi->blocks_samples = brstmi->total_samples;
    brstmi->final_block_size = brstmi->blocks_size;
    brstmi->final_block_samples = brstmi->blocks_samples;
    brstmi->final_block_size_p = brstmi->final_block_size;
    
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
    if(brstmi->num_tracks>1 && debugLevel>=0) {std::cout << "Warning: " << eformat_s[eformat] << " track information is guessed\n";}
    brstmi->warn_guessed_track_info = 1;
    
    
    //DATA chunk
    if(DATA_offset == (uint32_t)-1) {
        if(debugLevel>=0) {std::cout << "DATA chunk does not exist.\n";}
        return 230;
    }
    
    if(decodeAudio) {
        magicstr = brstm_getSliceAsString(fileData,DATA_offset,4);
        if(strcmp(magicstr,emagic3) != 0) {
            if(debugLevel>=0) {std::cout << "Invalid DATA chunk.\n";}
            return 230;
        }
        
        unsigned char awrite_res = brstm_doStandardAudioWrite(brstmi, fileData, debugLevel, decodeAudio);
        if(awrite_res > 128) return awrite_res;
    } else {
        if(debugLevel>=0) {std::cout << "Warning: Realtime decoding works like full decoding for this format\n";}
        brstmi->warn_realtime_decoding = 1;
    }
    
    return 0;
}
