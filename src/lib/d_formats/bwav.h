//OpenRevolution BWAV reader
//Copyright (C) 2020 IC

unsigned char brstm_formats_read_bwav(Brstm* brstmi,const unsigned char* fileData,signed int debugLevel,uint8_t decodeAudio) {
    bool &BOM = brstmi->BOM;
    
    //Byte Order Mark
    if(brstm_getSliceAsInt16Sample(fileData,0x04,1) == -257) {
        BOM = 1; //Big endian
    } else {
        BOM = 0; //Little endian
    }
    
    brstmi->num_channels = brstm_getSliceAsNumber(fileData,0x0E,2,BOM);
    if(brstmi->num_channels > 16) {
        if(debugLevel>=0) {std::cout << "Too many channels, max supported is 16.\n";}
        return 248;
    }
    
    //Adding 1 to make the codec number compatible with standard codec numbers in the library.
    brstmi->codec = brstm_getSliceAsNumber(fileData,0x10,2,BOM) + 1;
    if(brstmi->codec < 1 || brstmi->codec > 2) {
        if(debugLevel>=0) {std::cout << "Unsupported codec.\n";}
        return 220;
    }
    
    brstmi->sample_rate = brstm_getSliceAsNumber(fileData,0x14,4,BOM);
    brstmi->total_samples = brstm_getSliceAsNumber(fileData,0x18,4,BOM);
    brstmi->loop_flag = ( (int32_t)brstm_getSliceAsNumber(fileData,0x4C,4,BOM) != -1 );
    brstmi->loop_start = brstm_getSliceAsNumber(fileData,0x50,4,BOM);
    
    //Audio offset to first channel audio data.
    brstmi->audio_offset = brstm_getSliceAsNumber(fileData,0x40,4,BOM);
    
    //Write compatible block information
    brstmi->total_blocks = 1;
    
    brstmi->blocks_size = brstmi->num_channels > 1 ? (brstm_getSliceAsNumber(fileData,0x8C,4,BOM) - brstmi->audio_offset) :
    // ---------------------------------------------- ^ Offset to second channel audio data - Offset to first channel = Block size that has the first byte of the next channel data after the end
    //In order: 16-bit PCM, 4-bit DSPADPCM
    (brstmi->codec == 1 ? brstmi->total_samples*2 : brstm_getBytesForAdpcmSamples(brstmi->total_samples));
    
    brstmi->blocks_samples = brstmi->total_samples;
    brstmi->final_block_size = brstmi->blocks_size;
    brstmi->final_block_samples = brstmi->blocks_samples;
    brstmi->final_block_size_p = brstmi->final_block_size;
    
    //Read channel pan values for better track information guessing
    unsigned int channelPans[2];
    channelPans[0] = brstm_getSliceAsNumber(fileData,0x12,2,BOM);
    if(brstmi->num_channels > 1) {
        channelPans[1] = brstm_getSliceAsNumber(fileData,0x12+0x4C,2,BOM);
    }
    
    //Guess track data based on channel pans and channel count
    brstmi->num_tracks = (brstmi->num_channels > 1 && brstmi->num_channels%2 == 0 && channelPans[0] == 0 && channelPans[1] == 1) ? brstmi->num_channels/2 : brstmi->num_channels;
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
                brstmi->ADPCM_coefs[c][i] = brstm_getSliceAsInt16Sample(fileData,0x20+i*2+c*0x4C,BOM);
            }
        }
    }
    if(brstmi->num_tracks>1 && debugLevel>=0) {std::cout << "Warning: BWAV track information is guessed\n";}
    
    //Audio
    //Allocate and read history samples
    for(unsigned char c=0;c<brstmi->num_channels;c++) {
        brstmi->ADPCM_hsamples_1[c] = new int16_t[1];
        brstmi->ADPCM_hsamples_2[c] = new int16_t[1];
        brstmi->ADPCM_hsamples_1[c][0] = brstm_getSliceAsInt16Sample(fileData,0x56+c*0x4C,BOM);
        brstmi->ADPCM_hsamples_2[c][0] = brstm_getSliceAsInt16Sample(fileData,0x58+c*0x4C,BOM);
    }
    
    if(decodeAudio) {
        unsigned char awrite_res = brstm_doStandardAudioWrite(brstmi, fileData, debugLevel, decodeAudio);
        if(awrite_res > 128) return awrite_res;
    } else {
        if(debugLevel>=0) {std::cout << "Warning: Realtime decoding works like full decoding for this format\n";}
    }
    
    return 0;
}
