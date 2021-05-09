//OpenRevolution BCWAV writer
//Copyright (C) 2021 I.C.

//BCWAV and BFWAV are almost the same, the main encoder function will take a special argument so it can also be used as the BFWAV encoder.

unsigned char brstm_formats_multi_encode_bcwav_bfwav(Brstm* brstmi, signed int debugLevel, uint8_t encodeADPCM, bool eformat);

unsigned char brstm_formats_encode_bcwav(Brstm* brstmi, signed int debugLevel, uint8_t encodeADPCM) {
    return brstm_formats_multi_encode_bcwav_bfwav(brstmi,debugLevel,encodeADPCM,0);
}


//bool eformat: 0 = BCWAV, 1 = BFWAV.
unsigned char brstm_formats_multi_encode_bcwav_bfwav(Brstm* brstmi, signed int debugLevel, uint8_t encodeADPCM, bool eformat) {
    if(debugLevel>=0) printf("Warning: BCWAV and BFWAV encoding is untested\n");
    
    
    //Strings and other things that change from BCWAV to BFWAV.
    const char* eformat_s[2] = {"BCWAV","BFWAV"};
    const char  eformat_l[2] = {'C','F'};
    
    //Chunk header strings, starting from 0x7000.
    //const char* chunks_s[2] = {"INFO","DATA"};
    
    //Check for unsupported codec
    if(brstmi->codec > 3) {
        if(debugLevel>=0) std::cout << "Unsupported codec.\n";
        return 220;
    } else if(brstmi->codec == 3) {
        if(debugLevel>=0) std::cout << "IMAADPCM is not supported.\n";
        return 220;
    }
    
    if(brstmi->num_tracks > 1) {
        if(debugLevel>=0) printf("Warning: %s cannot store accurate track information\n", eformat_s[eformat]);
    }
    
    bool &BOM = brstmi->BOM;
    char spinner = '/';
    
    if(debugLevel>0) std::cout << "\r" << brstm_encoder_nextspinner(spinner) << " Building " << eformat_s[eformat] << " headers...                " << std::flush;
    
    //Allocate output file buffer
    delete[] brstmi->encoded_file;
    unsigned char* buffer = new unsigned char[(brstmi->total_samples*brstmi->num_channels*
    (brstmi->codec == 1 ? 2 : 1)) //For 16 bit PCM
    +((brstmi->total_samples*brstmi->num_channels/14336)*4)+brstmi->num_channels*256+8192];
    unsigned long  bufpos = 0;
    unsigned long  off; //for argument 4 of brstm_encoder_writebytes when we don't write to the end of the buffer
    
    
    //Header
    brstm_encoder_writebyte(buffer,eformat_l[eformat],bufpos);
    brstm_encoder_writebytes(buffer,(unsigned char*)"WAV",3,bufpos);
    //Byte order mark
    switch(BOM) {
        //LE
        case 0: brstm_encoder_writebytes_i(buffer,new unsigned char[2]{0xFF,0xFE},2,bufpos); break;
        //BE
        case 1: brstm_encoder_writebytes_i(buffer,new unsigned char[2]{0xFE,0xFF},2,bufpos); break;
    }
    //Header size, will be written later
    brstm_encoder_writebytes_i(buffer,new unsigned char[2]{0x00,0x00},2,bufpos);
    //Version number (LE)
    uint8_t version[4] = {0x00,0x00,0x00,0x00};
    if(eformat == 0) {
        //BCWAV version
        version[3] = 0x02;
        version[2] = 0x01;
    } else {
        // ?? TODO
    }
    //Write version in correct byte order
    if(BOM == 1) {
        for(signed char v=3; v>=0; v--) {
            brstm_encoder_writebyte(buffer,version[v],bufpos);
        }
    } else {
        brstm_encoder_writebytes(buffer,version,4,bufpos);
    }
    //File size, will be written later
    brstm_encoder_writebytes_i(buffer,new unsigned char[4]{0x00,0x00,0x00,0x00},4,bufpos);
    //Number of chunks (2)
    brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(2,2,BOM),2,bufpos);
    //Padding
    brstm_encoder_writebytes_i(buffer,new unsigned char[2]{0x00,0x00},2,bufpos);
    
    
    //Chunk information
    bool chunks_to_write[2] = {1,1};
    //Writing offsets to chunk offsets and sizes to be finalized later.
    uint32_t chunks_header_offsets[2] = {0,0};
    uint32_t chunks_header_sizes  [2] = {0,0};
    for(unsigned char c=0; c<2; c++) {
        //Section code
        brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(0x7000+c,2,BOM),2,bufpos);
        //Padding
        brstm_encoder_writebytes_i(buffer,new unsigned char[2]{0x00,0x00},2,bufpos);
        //Offset
        chunks_header_offsets[c] = bufpos;
        if(chunks_to_write[c] == 0) {
            //Write false offset
            brstm_encoder_writebytes_i(buffer,new unsigned char[4]{0xFF,0xFF,0xFF,0xFF},4,bufpos);
        } else {
            //Write 0 offset, actual offset will be written later.
            brstm_encoder_writebytes_i(buffer,new unsigned char[4]{0x00,0x00,0x00,0x00},4,bufpos);
        }
        //Size, will be written later.
        chunks_header_sizes[c] = bufpos;
        brstm_encoder_writebytes_i(buffer,new unsigned char[4]{0x00,0x00,0x00,0x00},4,bufpos);
    }
    //Add padding to 0x20.
    while(bufpos % 0x20 != 0) brstm_encoder_writebyte(buffer,0x00,bufpos);
    //Write finalized header size.
    brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(bufpos,2,BOM),2,off=0x06);
    
    
    if(debugLevel>0) std::cout << "\r" << brstm_encoder_nextspinner(spinner) << " Calculating blocks and history samples...       " << std::flush;
    //Calculate blocks
    brstmi->total_blocks = 1;
    brstmi->blocks_size =
    //In order: 8-bit PCM, 16-bit PCM, 4-bit DSPADPCM
    (brstmi->codec == 0 ? brstmi->total_samples : brstmi->codec == 1 ? brstmi->total_samples*2 : brstm_getBytesForAdpcmSamples(brstmi->total_samples));
    brstmi->blocks_samples = brstmi->total_samples;
    brstmi->final_block_size = brstmi->blocks_size;
    brstmi->final_block_samples = brstmi->blocks_samples;
    brstmi->final_block_size_p = brstmi->final_block_size;
    //TODO: Is 0x08 padding correct instead of 0x20?
    while(brstmi->final_block_size_p % 0x08 != 0) brstmi->final_block_size_p++;
    
    //Calculate history samples
    brstm_HSData_t HSData;
    //Allocate HS storage
    for(unsigned int ch=0; ch<brstmi->num_channels; ch++) {
        HSData.HS1[ch] = new int16_t[brstmi->total_blocks];
        HSData.HS2[ch] = new int16_t[brstmi->total_blocks];
    }
    if(brstmi->codec == 2) {
        brstm_encoder_adpcm_calculateAdpcmData(brstmi, encodeADPCM, &HSData);
    }
    
    
    //INFO chunk
    uint32_t INFOchunkoffset;
    uint32_t INFOchunksize;
    INFOchunkoffset = bufpos;
    
    brstm_encoder_writebytes(buffer,(unsigned char*)"INFO",4,bufpos);
    //Chunk size, will be written later
    brstm_encoder_writebytes_i(buffer,new unsigned char[4]{0x00,0x00,0x00,0x00},4,bufpos);

    //Write stream info data
    brstm_encoder_writebyte(buffer,brstmi->codec,bufpos);
    brstm_encoder_writebyte(buffer,brstmi->loop_flag,bufpos);
    brstm_encoder_writebytes_i(buffer,new unsigned char[2]{0x00,0x00},2,bufpos); //Padding
    brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(brstmi->sample_rate,4,BOM),4,bufpos);
    brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(brstmi->loop_start,4,BOM),4,bufpos);
    brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(brstmi->total_samples,4,BOM),4,bufpos);
    brstm_encoder_writebytes_i(buffer,new unsigned char[4]{0x00,0x00,0x00,0x00},4,bufpos);
    
    
    //Channel info reference table
    uint32_t chinfo_struct_beg;
    uint32_t chinfo_ref_table_offsets[brstmi->num_channels];
    
    chinfo_struct_beg = bufpos;
    //Number of references (channels)
    brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(brstmi->num_channels,4,BOM),4,bufpos);
    //References
    for(unsigned int c=0; c<brstmi->num_channels; c++) {
        //Section code
        brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(0x7100,2,BOM),2,bufpos);
        //Padding
        brstm_encoder_writebytes_i(buffer,new unsigned char[2]{0x00,0x00},2,bufpos);
        //Offset to channel info reference, will be written later
        chinfo_ref_table_offsets[c] = bufpos;
        brstm_encoder_writebytes_i(buffer,new unsigned char[4]{0x00,0x00,0x00,0x00},4,bufpos);
    }
    
    //Offsets to audio data for every channel (from beginning of DATA chunk data)
    uint32_t ch_audio_offsets[brstmi->num_channels];
    //Channel information itself
    uint32_t chinfo_ips_offsets[brstmi->num_channels];
    uint32_t chinfo_lps_offsets[brstmi->num_channels];
    //Second references...
    uint32_t chinfo_ref2_offsets[brstmi->num_channels];
    uint32_t chinfo_ref2_begs[brstmi->num_channels];
    
    for(unsigned int c=0; c<brstmi->num_channels; c++) {
        //Write the offset to first reference table
        brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(bufpos-chinfo_struct_beg,4,BOM),4,off=chinfo_ref_table_offsets[c]);
        
        chinfo_ref2_begs[c] = bufpos;
        
        //Sample offset reference
        brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(0x1F00,2,BOM),2,bufpos);
        brstm_encoder_writebytes_i(buffer,new unsigned char[2]{0x00,0x00},2,bufpos); //Padding
        //Sample data offset from beginning of DATA chunk content. Will be written later.
        brstm_encoder_writebytes_i(buffer,new unsigned char[4]{0x00,0x00,0x00,0x00},4,bufpos);
        
        //ADPCM info reference
        //Section code
        brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(0x0300,2,BOM),2,bufpos);
        //Padding
        brstm_encoder_writebytes_i(buffer,new unsigned char[2]{0x00,0x00},2,bufpos);
        //Offset to actual channel info, will be written later
        chinfo_ref2_offsets[c] = bufpos;
        brstm_encoder_writebytes_i(buffer,new unsigned char[4]{0x00,0x00,0x00,0x00},4,bufpos);
        
        //Padding
        brstm_encoder_writebytes_i(buffer,new unsigned char[4]{0x00,0x00,0x00,0x00},4,bufpos);
    }
    
    //Actual DSPADPCM channel information
    for(unsigned int c=0; c<brstmi->num_channels; c++) {
        //Write the offset to second references
        brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(bufpos-chinfo_ref2_begs[c],4,BOM),4,off=chinfo_ref2_offsets[c]);
        //ADPCM coefs
        if(brstmi->codec == 2) {
            for(unsigned int a=0; a<16; a++) {
                brstm_encoder_writebytes(buffer,brstm_encoder_getByteInt16(brstmi->ADPCM_coefs[c][a],BOM),2,bufpos);
            }
        } else {
            //Empty ADPCM coefs for other codecs
            for(unsigned int a=0; a<16; a++) {
                brstm_encoder_writebytes(buffer,brstm_encoder_getByteInt16(0,BOM),2,bufpos);
            }
        }
        chinfo_ips_offsets[c] = bufpos;
        brstm_encoder_writebytes_i(buffer,new unsigned char[2]{0x00,0x00},2,bufpos); //Initial predictor scale, will be written later
        brstm_encoder_writebytes_i(buffer,new unsigned char[2]{0x00,0x00},2,bufpos); //HS1, always zero
        brstm_encoder_writebytes_i(buffer,new unsigned char[2]{0x00,0x00},2,bufpos); //HS2, always zero
        chinfo_lps_offsets[c] = bufpos;
        brstm_encoder_writebytes_i(buffer,new unsigned char[2]{0x00,0x00},2,bufpos); //Loop predictor scale, will be written later
        if(brstmi->codec == 2) {
            brstm_encoder_writebytes  (buffer,brstm_encoder_getByteInt16(HSData.LoopHS1[c],BOM),2,bufpos); //Loop HS1
            brstm_encoder_writebytes  (buffer,brstm_encoder_getByteInt16(HSData.LoopHS2[c],BOM),2,bufpos); //Loop HS2
        } else {
            brstm_encoder_writebytes  (buffer,brstm_encoder_getByteInt16(0,BOM),2,bufpos); //Loop HS1 (empty)
            brstm_encoder_writebytes  (buffer,brstm_encoder_getByteInt16(0,BOM),2,bufpos); //Loop HS2 (empty)
        }
        brstm_encoder_writebytes_i(buffer,new unsigned char[2]{0x00,0x00},2,bufpos); //Padding
    }
    
    
    //Finalize INFO chunk
    //Padding
    while(bufpos % 0x20 != 0) {
        brstm_encoder_writebyte(buffer,0,bufpos);
    }
    INFOchunksize = bufpos - INFOchunkoffset;
    //Write chunk length
    brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(INFOchunksize,4,BOM),4,off=INFOchunkoffset+4);
    
    
    
    
    //DATA chunk
    if(debugLevel>0) std::cout << "\r" << brstm_encoder_nextspinner(spinner) << " Writing audio data...                   " << std::flush;
    uint32_t DATAchunkoffset;
    uint32_t DATAchunksize;
    
    DATAchunkoffset = bufpos;
    brstm_encoder_writebytes(buffer,(unsigned char*)"DATA",4,bufpos);
    //Chunk size, will be written later
    brstm_encoder_writebytes_i(buffer,new unsigned char[4]{0x00,0x00,0x00,0x00},4,bufpos);
    //Padding before beginning of audio data
    while(bufpos % 0x20 != 0) {
        brstm_encoder_writebyte(buffer,0,bufpos);
    }
    
    //Calculate offsets to every channel and write them back to channel information
    for(unsigned int c=0; c<brstmi->num_channels; c++) {
        ch_audio_offsets[c] = (bufpos - (DATAchunkoffset + 8)) + (c * brstmi->final_block_size_p);
        brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(ch_audio_offsets[c],4,BOM),4,off=chinfo_ref2_begs[c]+4);
    }
    
    //DSPADPCM encoding
    if(brstmi->codec == 2) {
        if(encodeADPCM == 1) {
            uint32_t adpcmbytes = brstm_getBytesForAdpcmSamples(brstmi->total_samples);
            for(unsigned int c=0;c<brstmi->num_channels;c++) {
                brstmi->ADPCM_data[c] = new unsigned char[adpcmbytes];
            }
            brstm_encode_adpcm(brstmi, brstmi->ADPCM_data, debugLevel);
        }
    }
    
    //Write audio
    brstm_encoder_doStandardAudioWrite(brstmi, buffer, bufpos, debugLevel);
    
    //Finalize and clean up DSPADPCM encoding
    if(brstmi->codec == 2) {
        for(unsigned int c=0;c<brstmi->num_channels;c++) {
            //Write ADPCM information to channel information in INFO chunk
            brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(brstmi->ADPCM_data[c][0],2,BOM),2,off=chinfo_ips_offsets[c]); //Initial scale
            brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(brstmi->ADPCM_data[c][(unsigned long)(brstmi->loop_start / 1.75)],2,BOM),2,off=chinfo_lps_offsets[c]); //Loop initial scale
            
            if(encodeADPCM == 1) {
                //delete the ADPCM data only if we made it locally
                delete[] brstmi->ADPCM_data[c];
                brstmi->ADPCM_data[c] = nullptr;
            }
        }
    }
    
    //Finalize DATA chunk
    DATAchunksize = bufpos - DATAchunkoffset;
    //Write chunk length
    brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(DATAchunksize,4,BOM),4,off=DATAchunkoffset+4);
    
    
    //Finalize file
    //Filesize
    brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(bufpos,4,BOM),4,off=0x0C);
    //INFO offset and size
    brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(INFOchunkoffset,4,BOM),4,off=chunks_header_offsets[0]);
    brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(INFOchunksize,4,BOM),4,off=chunks_header_sizes[0]);
    //DATA offset and size
    brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(DATAchunkoffset,4,BOM),4,off=chunks_header_offsets[1]);
    brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(DATAchunksize,4,BOM),4,off=chunks_header_sizes[1]);
    
    
    //copy finished file to brstm_encoded_data
    brstmi->encoded_file = new unsigned char[bufpos];
    memcpy(brstmi->encoded_file,buffer,bufpos);
    delete[] buffer;
    brstmi->encoded_file_size = bufpos;
    
    //Free HS storage
    for(unsigned int ch=0; ch<brstmi->num_channels; ch++) {
        delete[] HSData.HS1[ch];
        delete[] HSData.HS2[ch];
    }
    
    if(debugLevel>0) std::cout << "\r" << eformat_s[eformat] << " encoding done                                    \n" << std::flush;
    
    return 0;
}
