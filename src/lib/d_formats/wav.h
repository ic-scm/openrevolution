//OpenRevolution WAV reader
//Copyright (C) 2020 Extrasklep

unsigned char brstm_formats_read_wav(Brstm* brstmi,const unsigned char* fileData,signed int debugLevel,uint8_t decodeAudio) {
    brstmi->BOM = 0;
    //Read the WAV file data
    if(strcmp("RIFF",brstm_getSliceAsString(fileData,0,4)) != 0) {
        if(debugLevel>=0) std::cout << "Invalid RIFF header.\n";
        return 255;
    }
    unsigned long wavFileSize = brstm_getSliceAsNumber(fileData,4,4,0) + 8;
    if(strcmp("WAVEfmt ",brstm_getSliceAsString(fileData,8,8)) != 0) {
        if(debugLevel>=0) std::cout << "Invalid WAVE header.\n";
        return 255;
    }
    unsigned long wavFmtSize = brstm_getSliceAsNumber(fileData,16,4,0);
    if(wavFmtSize != 16 || brstm_getSliceAsNumber(fileData,20,2,0) != 1) {
        if(debugLevel>=0) std::cout << "Only PCM WAVs are supported.\n";
        return 220;
    }
    brstmi->num_channels = brstm_getSliceAsNumber(fileData,22,2,0);
    if(brstmi->num_channels > 16) {
        if(debugLevel>=0) std::cout << "Too many channels. Max supported is 16.\n";
        return 249;
    }
    brstmi->sample_rate = brstm_getSliceAsNumber(fileData,24,4,0);
    if(brstm_getSliceAsNumber(fileData,34,2,0) != 16) {
        if(debugLevel>=0) std::cout << "Only 16-bit PCM WAVs are supported.\n";
        return 220;
    }
    unsigned long wavAudioOffset = 36;
    for(;strcmp("data",brstm_getSliceAsString(fileData,wavAudioOffset,4)) != 0 ;wavAudioOffset++) {}
    brstmi->total_samples = brstm_getSliceAsNumber(fileData,wavAudioOffset+4,4,0) / brstmi->num_channels / 2;
    wavAudioOffset += 8;
    
    //Write some standard information to BRSTM struct
    brstmi->codec = 1;
    brstmi->loop_flag = 0;
    brstmi->loop_start = 0;
    brstmi->audio_offset = wavAudioOffset;
    //Extend block size and set audio stream type to 1
    brstmi->total_blocks = brstmi->total_samples / 2048;
    if(brstmi->total_samples % 2048 != 0) brstmi->total_blocks++;
    brstmi->blocks_size = 2*2048 * brstmi->num_channels;
    brstmi->blocks_samples = 1*2048;
    brstmi->final_block_size = 2*(brstmi->total_samples % 2048) * brstmi->num_channels;
    brstmi->final_block_samples = 1*(brstmi->total_samples % 2048);
    brstmi->final_block_size_p = brstmi->final_block_size;
    brstmi->audio_stream_format = 1;
    
    //Write standard track information
    brstmi->num_tracks = (brstmi->num_channels > 1 && brstmi->num_channels%2 == 0) ? brstmi->num_channels/2 : brstmi->num_channels;
    if(brstmi->num_tracks > 8) {
        if(debugLevel>=0) {std::cout << "Too many tracks, Max supported is 8.\n";}
        return 248;
    }
    unsigned char track_num_channels = brstmi->num_tracks*2 == brstmi->num_channels ? 2 : 1;
    brstmi->track_desc_type = 0;
    for(unsigned char c=0; c<brstmi->num_channels; c++) {
        brstmi->track_num_channels[c/track_num_channels] = track_num_channels;
        if(track_num_channels == 1 || c%2 == 0) brstmi->track_lchannel_id[c/track_num_channels] = c;
        if(track_num_channels == 2 && c%2 == 1) brstmi->track_rchannel_id[c/track_num_channels] = c;
    }
    if(brstmi->num_tracks>1 && debugLevel>=0) {std::cout << "Warning: Track information is guessed\n";}
    
    if(decodeAudio == 1) {
        //Read audio from wav file
        unsigned int c;
        for(c=0;c<brstmi->num_channels;c++) {
            brstmi->PCM_samples[c] = new int16_t[brstmi->total_samples];
        }
        for(unsigned int i=0;i<brstmi->total_samples;i++) {
            for(c=0;c<brstmi->num_channels;c++) {
                brstmi->PCM_samples[c][i] = brstm_getSliceAsInt16Sample(fileData,wavAudioOffset+i*(2*brstmi->num_channels)+c*2,0);
            }
        }
    } else if(decodeAudio == 2) {
        if(debugLevel>=0) std::cout << "Cannot write raw ADPCM data because the codec is not ADPCM.\n";
        return 220;
    }
    return 0;
}
