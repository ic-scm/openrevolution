//OpenRevolution BRWAV reader
//Copyright (C) 2021 IC

unsigned char brstm_formats_read_brwav(Brstm* brstmi, const unsigned char* fileData, signed int debugLevel, uint8_t decodeAudio) {
    bool &BOM = brstmi->BOM;
    
    //Byte Order Mark
    if(brstm_getSliceAsInt16Sample(fileData,0x04,1) == -257) {
        BOM = 1; //Big endian
    } else {
        BOM = 0; //Little endian
    }
    
    brstmi->num_channels = brstm_getSliceAsNumber(fileData,0x2A,1,BOM);
    if(brstmi->num_channels > 16) {
        if(debugLevel>=0) {std::cout << "Too many channels, max supported is 16.\n";}
        return 248;
    }
    
    brstmi->codec = brstm_getSliceAsNumber(fileData,0x28,1,BOM);
    if(brstmi->codec < 1 || brstmi->codec > 2) {
        if(debugLevel>=0) {std::cout << "Unsupported codec.\n";}
        return 220;
    }
    
    brstmi->sample_rate = brstm_getSliceAsNumber(fileData,0x2C,2,BOM);
    brstmi->total_samples = brstm_getSamplesForAdpcmNibbles(brstm_getSliceAsNumber(fileData,0x34,4,BOM));
    brstmi->loop_flag = brstm_getSliceAsNumber(fileData,0x29,1,BOM);
    brstmi->loop_start = brstm_getSamplesForAdpcmNibbles(brstm_getSliceAsNumber(fileData,0x30,4,BOM));
    
    //Audio offset to first channel audio data.
    brstmi->audio_offset = brstm_getSliceAsNumber(fileData,0x18,4,BOM) + 8;
    
    //Write compatible block information
    brstmi->total_blocks = 1;
    
    //Calculate block size based on sample count and codec
    brstmi->blocks_size = (brstmi->codec == 1 ? brstmi->total_samples*2 : brstm_getBytesForAdpcmSamples(brstmi->total_samples));
    
    //Block size with padding
    brstmi->final_block_size_p = brstmi->num_channels > 1 ? brstm_getSliceAsNumber(fileData,0x68,4,BOM) :
    // ----------------------------------------------------- ^ Offset to second channel audio data - Offset to first channel = Block size that has the first byte of the next channel data after the end
    // Normal block size in single channel files
    brstmi->blocks_size;
    
    brstmi->blocks_samples = brstmi->total_samples;
    brstmi->final_block_size = brstmi->blocks_size;
    brstmi->final_block_samples = brstmi->blocks_samples;
    
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
        
        //Read coefs
        if(brstmi->codec == 2) {
            for(unsigned int i=0;i<16;i++) {
                brstmi->ADPCM_coefs[c][i] = brstm_getSliceAsInt16Sample(fileData,0x84+i*2+c*0x30,BOM);
            }
        }
    }
    if(brstmi->num_tracks>1 && debugLevel>=0) {std::cout << "Warning: BRWAV track information is guessed\n";}
    brstmi->warn_guessed_track_info = 1;
    
    //Audio
    //Allocate and read history samples
    for(unsigned char c=0;c<brstmi->num_channels;c++) {
        brstmi->ADPCM_hsamples_1[c] = new int16_t[1];
        brstmi->ADPCM_hsamples_2[c] = new int16_t[1];
        brstmi->ADPCM_hsamples_1[c][0] = 0;
        brstmi->ADPCM_hsamples_2[c][0] = 0;
    }
    
    if(decodeAudio) {
        unsigned char awrite_res = brstm_doStandardAudioWrite(brstmi, fileData, debugLevel, decodeAudio);
        if(awrite_res > 128) return awrite_res;
    } else {
        if(debugLevel>=0) {std::cout << "Warning: Realtime decoding works like full decoding for this format\n";}
        brstmi->warn_realtime_decoding = 1;
    }
    
    return 0;
}
