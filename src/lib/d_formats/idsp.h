//Openrevolution IDSP reader
//Copyright (C) 2021 I.C.

unsigned char brstm_formats_read_idsp(Brstm* brstmi, const unsigned char* fileData, signed int debugLevel, uint8_t decodeAudio) {
    bool &BOM = brstmi->BOM;
    //IDSP is always big endian
    BOM = 1;
    
    //TODO: Consider implementing a new audio stream format in the audio decoder to avoid inefficiency caused by the small block sizes.
    
    char* magicstr = brstm_getSliceAsString(fileData,0,4);
    if(strcmp(magicstr, "IDSP") != 0) {
        if(debugLevel>=0) printf("Invalid IDSP file.\n");
        return 255;
    }
    
    uint32_t dsp_interleave_size;
    uint32_t dsp_headers_offset;
    uint32_t dsp_headers_spacing;
    uint32_t dsp_channel_size;
    
    //Stream information
    brstmi->num_channels  = brstm_getSliceAsNumber(fileData,0x08,4,BOM);
    brstmi->sample_rate   = brstm_getSliceAsNumber(fileData,0x0C,4,BOM);
    brstmi->total_samples = brstm_getSliceAsNumber(fileData,0x10,4,BOM);
    brstmi->loop_start    = brstm_getSliceAsNumber(fileData,0x14,4,BOM);
    //0x18: Loop end
    //0x1C: Interleave size (read later)
    //0x20: DSP header offset (read later)
    //0x24: DSP header spacing (read later)
    brstmi->audio_offset  = brstm_getSliceAsNumber(fileData,0x28,4,BOM);
    //0x2C: Channel size
    
    if(brstmi->num_channels > 16) {
        if(debugLevel>=0) printf("Too many channels, max supported is 16.\n");
        return 249;
    }
    
    brstmi->codec = 2;
    
    //DSP headers
    dsp_interleave_size = brstm_getSliceAsNumber(fileData,0x1C,4,BOM);
    dsp_headers_offset  = brstm_getSliceAsNumber(fileData,0x20,4,BOM);
    dsp_headers_spacing = brstm_getSliceAsNumber(fileData,0x24,4,BOM);
    dsp_channel_size    = brstm_getSliceAsNumber(fileData,0x2C,4,BOM);
    
    if(dsp_interleave_size == 0) {
        //Use channel size
        dsp_interleave_size = dsp_channel_size;
    }
    
    //Write standard block information
    brstmi->total_blocks = ceil((float)dsp_channel_size / dsp_interleave_size);
    brstmi->blocks_size = dsp_interleave_size;
    //Use total samples if we're dealing with a single block, otherwise multiply interleave size by 1.75 and hope for the best
    //Usually dsp_interleave_size should be 16 so the resulting samples per block would be 28.
    brstmi->blocks_samples = (dsp_interleave_size == dsp_channel_size ? brstmi->total_samples : dsp_interleave_size * 1.75);
    
    brstmi->final_block_samples = brstmi->total_samples % brstmi->blocks_samples;
    if(brstmi->final_block_samples == 0) brstmi->final_block_samples = brstmi->blocks_samples;
    
    brstmi->final_block_size = brstm_getBytesForAdpcmSamples(brstmi->final_block_samples);
    brstmi->final_block_size_p = brstmi->blocks_size;
    
    //Allocate history samples
    for(unsigned char c=0;c<brstmi->num_channels;c++) {
        brstmi->ADPCM_hsamples_1[c] = new int16_t[brstmi->total_blocks];
        brstmi->ADPCM_hsamples_2[c] = new int16_t[brstmi->total_blocks];
    }
    
    //Read headers for each channel
    for(uint32_t c=0; c<brstmi->num_channels; c++) {
        //Offset to current channel DSP header
        uint32_t cc_offset = dsp_headers_spacing * c + dsp_headers_offset;
        
        //0x00: Sample count
        //0x04: Nibble count
        //0x08: Sample rate
        //0x0C: Loop flag (16-bit)
        brstmi->loop_flag = brstm_getSliceAsNumber(fileData,0x0C+cc_offset,2,BOM);
        //0x0E: Format? (16-bit)
        //0x10: Loop start offset (in nibbles?)
        //0x14: Loop end offset (in nibbles?)
        //0x1C: DSPADPCM coefs (16 * 16-bit)
        for(uint8_t i=0; i<16; i++) {
            brstmi->ADPCM_coefs[c][i] = brstm_getSliceAsNumber(fileData,0x1C+cc_offset + (i<<1),2,BOM);
        }
        //0x3C: Gain (16-bit)
        //0x3E: Initial predictor scale (16-bit)
        //0x40: History sample 1 (16-bit)
        brstmi->ADPCM_hsamples_1[c][0] = brstm_getSliceAsNumber(fileData,0x40+cc_offset,2,BOM);
        //0x42: History sample 2 (16-bit)
        brstmi->ADPCM_hsamples_2[c][0] = brstm_getSliceAsNumber(fileData,0x42+cc_offset,2,BOM);
        //0x44: Loop predictor scale (16-bit)
        //0x46: Loop history sample 1 (16-bit)
        //0x48: Loop history sample 2 (16-bit)
        //0x4A: Channel count?
        //0x4C: Block size?
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
    if(brstmi->num_tracks>1 && debugLevel>=0) {std::cout << "Warning: IDSP track information is guessed\n";}
    brstmi->warn_guessed_track_info = 1;
    
    
    //Run standard audio write
    if(decodeAudio) {
        unsigned char awrite_res = brstm_doStandardAudioWrite(brstmi, fileData, debugLevel, decodeAudio);
        if(awrite_res > 128) return awrite_res;
    }
    
    return 0;
}
