//OpenRevolution BCSTM/BFSTM encoder
//Copyright (C) 2020 IC

//BCSTM and BFSTM are almost the same, the main encoder function will take a special argument so it can also be used as the BFSTM encoder.

unsigned char brstm_formats_multi_encode_bcstm_bfstm(Brstm* brstmi,signed int debugLevel,uint8_t encodeADPCM, bool eformat);

unsigned char brstm_formats_encode_bcstm(Brstm* brstmi,signed int debugLevel,uint8_t encodeADPCM) {
    //BCSTM files are usually little endian.
    brstmi->BOM = 0;
    return brstm_formats_multi_encode_bcstm_bfstm(brstmi,debugLevel,encodeADPCM,0);
}


//bool eformat: 0 = BCSTM, 1 = BFSTM.
unsigned char brstm_formats_multi_encode_bcstm_bfstm(Brstm* brstmi,signed int debugLevel,uint8_t encodeADPCM, bool eformat) {
    if(eformat == 1) {
        std::cout << "BFSTM encoder is not implemented yet.\n";
        return 210;
    }
    if(debugLevel>=0) {std::cout << "BCSTM encoder is currently work in progress.\n";}
    
    //Strings and other things that change from BCSTM to BFSTM.
    const char* eformat_s[2] = {"BCSTM","BFSTM"};
    const char  eformat_l[2] = {'C','F'};
    //Chunk header strings, starting from 0x4000.
    //REGN is not supported.
    const char* chunks_s[4] = {"INFO","SEEK","DATA","REGN"};
    
    //Check for invalid requests
    if(brstmi->codec > 3) {
        if(debugLevel>=0) std::cout << "Unsupported codec.\n";
        return 220;
    } else if(brstmi->codec == 3) {
        if(debugLevel>=0) std::cout << "IMAADPCM is not supported.\n";
        return 220;
    }
    
    if(brstmi->num_tracks > 1) {
        if(debugLevel>=0) std::cout << "Warning: " << eformat_s[eformat] << " cannot store accurate track information\n";
    }
    
    bool &BOM = brstmi->BOM;
    char spinner = '/';
    
    if(debugLevel>0) std::cout << "\r" << brstm_encoder_nextspinner(spinner)
        << " Building headers... (" << eformat_l[eformat] << "STM)             " << std::flush;
    
    //Allocate output file buffer
    delete[] brstmi->encoded_file;
    unsigned char* buffer = new unsigned char[(brstmi->total_samples*brstmi->num_channels*
    (brstmi->codec == 1 ? 2 : 1)) //For 16 bit PCM
    +((brstmi->total_samples*brstmi->num_channels/14336)*4)+brstmi->num_channels*256+8192];
    unsigned long  bufpos = 0;
    unsigned long  off; //for argument 4 of brstm_encoder_writebytes when we don't write to the end of the buffer
    
    //Header
    brstm_encoder_writebyte(buffer,eformat_l[eformat],bufpos);
    brstm_encoder_writebytes(buffer,(unsigned char*)"STM",3,bufpos);
    //Byte order mark
    switch(BOM) {
        //LE
        case 0: brstm_encoder_writebytes_i(buffer,new unsigned char[2]{0xFF,0xFE},2,bufpos); break;
        //BE
        case 1: brstm_encoder_writebytes_i(buffer,new unsigned char[2]{0xFE,0xFF},2,bufpos); break;
    }
    //Header size, will be written later
    brstm_encoder_writebytes_i(buffer,new unsigned char[2]{0x00,0x00},2,bufpos);
    //Version number? TODO: Research more about it and check if it's different in BFSTM
    brstm_encoder_writebytes_i(buffer,new unsigned char[4]{0x00,0x00,0x01,0x02},4,bufpos);
    //File size, will be written later
    brstm_encoder_writebytes_i(buffer,new unsigned char[4]{0x00,0x00,0x00,0x00},4,bufpos);
    //Number of chunks
    brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(3,2,BOM),2,bufpos);
    //Padding
    brstm_encoder_writebytes_i(buffer,new unsigned char[2]{0x00,0x00},2,bufpos);
    
    //Chunk information
    //TODO: find a PCM file to see if it has the SEEK chunk here.
    bool chunks_to_write[4] = {1,1,1,0};
    //Writing offsets to chunk offsets and sizes to be finalized later.
    uint32_t chunks_header_offsets[4] = {0,0,0,0};
    uint32_t chunks_header_sizes  [4] = {0,0,0,0};
    for(unsigned char c=0; c<4; c++) {
        if(c==3 && chunks_to_write[c]==0) continue;
        //Section code
        brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(0x4000+c,2,BOM),2,bufpos);
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
    brstmi->blocks_samples = (brstmi->codec == 2 ? 14336 : brstmi->codec == 1 ? 4096 : 8192);
    brstmi->blocks_size = 8192;
    brstmi->total_blocks = brstmi->total_samples / brstmi->blocks_samples;
    if(brstmi->total_samples % brstmi->blocks_samples != 0) brstmi->total_blocks++;
    //Final block
    brstmi->final_block_samples = brstmi->total_samples % brstmi->blocks_samples;
    if(brstmi->final_block_samples == 0) brstmi->final_block_samples = brstmi->blocks_samples;
    brstmi->final_block_size = (
        brstmi->codec == 2 ? brstm_getBytesForAdpcmSamples(brstmi->final_block_samples)
        : brstmi->codec == 1 ? brstmi->final_block_samples * 2
        : brstmi->final_block_samples
    );
    brstmi->final_block_size_p = brstmi->final_block_size;
    while(brstmi->final_block_size_p % 0x20 != 0) {brstmi->final_block_size_p++;}
    
    //Calculate history samples
    //TODO this is copied code, split this into a separate utils function?
    int16_t LoopHS1[brstmi->num_channels];
    int16_t LoopHS2[brstmi->num_channels];
    int16_t HS1[brstmi->num_channels][brstmi->total_blocks];
    int16_t HS2[brstmi->num_channels][brstmi->total_blocks];
    if(brstmi->codec == 2) {
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
                DSPCorrelateCoefs(brstmi->PCM_samples[c],brstmi->total_samples,brstmi->ADPCM_coefs[c]);
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
    
    
    //INFO chunk
    uint32_t INFOchunkoffset;
    uint32_t INFOchunksize;
    if(debugLevel>0) std::cout << "\r" << brstm_encoder_nextspinner(spinner) << " Building headers... (INFO)             " << std::flush;
    INFOchunkoffset = bufpos;
    brstm_encoder_writebytes(buffer,(unsigned char*)"INFO",4,bufpos);
    //Chunk size, will be written later
    brstm_encoder_writebytes_i(buffer,new unsigned char[4]{0x00,0x00,0x00,0x00},4,bufpos);
    //Subchunk references: stream info, track info, channel info
    //Track info is unsupported.
    bool info_subchunks_to_write[3] = {1,0,1};
    const uint16_t info_subchunk_codes[3] = {0x4100,0x0101,0x0101};
    uint32_t info_subchunk_offset_offsets[3] = {0,0,0}; //Reference for writing offsets later.
    uint32_t info_subchunk_offsets[3] = {0,0,0}; //From beginning of INFO chunk content.
    for(unsigned char c=0; c<3; c++) {
        //Section code
        if(info_subchunks_to_write[c] == 0) brstm_encoder_writebytes_i(buffer,new unsigned char[2]{0x00,0x00},2,bufpos);
        else brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(info_subchunk_codes[c],2,BOM),2,bufpos);
        //Padding
        brstm_encoder_writebytes_i(buffer,new unsigned char[2]{0x00,0x00},2,bufpos);
        //Offset (from beginning of INFO chunk content)
        info_subchunk_offset_offsets[c] = bufpos;
        if(info_subchunks_to_write[c] == 0) {
            //Write false offset
            brstm_encoder_writebytes_i(buffer,new unsigned char[4]{0xFF,0xFF,0xFF,0xFF},4,bufpos);
        } else {
            //Write 0 offset, actual offset will be written later.
            brstm_encoder_writebytes_i(buffer,new unsigned char[4]{0x00,0x00,0x00,0x00},4,bufpos);
        }
    }
    
    //Stream info
    uint32_t data_audio_offset_offset = 0;
    if(info_subchunks_to_write[0]) {
        info_subchunk_offsets[0] = bufpos - (INFOchunkoffset + 8);
        //Write stream info data
        brstm_encoder_writebyte(buffer,brstmi->codec,bufpos);
        brstm_encoder_writebyte(buffer,brstmi->loop_flag,bufpos);
        brstm_encoder_writebyte(buffer,brstmi->num_channels,bufpos);
        brstm_encoder_writebyte(buffer,0,bufpos); //Region count, unsupported.
        brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(brstmi->sample_rate,4,BOM),4,bufpos);
        brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(brstmi->loop_start,4,BOM),4,bufpos);
        brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(brstmi->total_samples,4,BOM),4,bufpos);
        brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(brstmi->total_blocks,4,BOM),4,bufpos);
        brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(brstmi->blocks_size,4,BOM),4,bufpos);
        brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(brstmi->blocks_samples,4,BOM),4,bufpos);
        brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(brstmi->final_block_size,4,BOM),4,bufpos);
        brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(brstmi->final_block_samples,4,BOM),4,bufpos);
        brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(brstmi->final_block_size_p,4,BOM),4,bufpos);
        //TODO: The 2 values below might be 0 in PCM files.
        brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(4,4,BOM),4,bufpos); //Bytes per ADPC chunk
        brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(brstmi->blocks_samples,4,BOM),4,bufpos); //ADPC sample interval.
        brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(0x1F00,2,BOM),2,bufpos); //Sample data flag?
        brstm_encoder_writebytes_i(buffer,new unsigned char[2]{0x00,0x00},2,bufpos); //Padding
        //Sample data offset from beginning of DATA chunk content. Will be written later.
        data_audio_offset_offset = bufpos;
        brstm_encoder_writebytes_i(buffer,new unsigned char[4]{0x00,0x00,0x00,0x00},4,bufpos);
        //Region info size? Seems to be 0x100 in all files.
        brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(0x100,2,BOM),2,bufpos);
        brstm_encoder_writebytes_i(buffer,new unsigned char[2]{0x00,0x00},2,bufpos); //Padding
        //Region info flag, not used here.
        brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(0,2,BOM),2,bufpos);
        brstm_encoder_writebytes_i(buffer,new unsigned char[2]{0x00,0x00},2,bufpos); //Padding
        //Region info offset from beginning of REGN chunk content. Unsupported and not used here.
        brstm_encoder_writebytes_i(buffer,new unsigned char[4]{0xFF,0xFF,0xFF,0xFF},4,bufpos);
        //"Original" loop start and end in newer files.
        if(0) {
            brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(brstmi->loop_start,4,BOM),4,bufpos);
            brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(brstmi->total_samples,4,BOM),4,bufpos);
        }
    }
    
    //Track info, unsupported
    if(info_subchunks_to_write[1]) {
        //info_subchunk_offsets[1] = bufpos - (INFOchunkoffset + 8);
        if(debugLevel>=0) std::cout << "Warning: Writing track information is not currently supported.\n";
    }
    
    //Channel info
    uint32_t chinfo_ips_offsets[brstmi->num_channels];
    uint32_t chinfo_lps_offsets[brstmi->num_channels];
    if(info_subchunks_to_write[2]) {
        info_subchunk_offsets[2] = bufpos - (INFOchunkoffset + 8);
        //Channel info reference table.
        uint32_t struct_beg = bufpos;
        //Number of references (channels)
        brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(brstmi->num_channels,4,BOM),4,bufpos);
        //References
        uint32_t chinfo_ref_table_offsets[brstmi->num_channels];
        for(unsigned int c=0; c<brstmi->num_channels; c++) {
            //Section code
            brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(0x4102,2,BOM),2,bufpos);
            //Padding
            brstm_encoder_writebytes_i(buffer,new unsigned char[2]{0x00,0x00},2,bufpos);
            //Offset to channel info reference, will be written later
            chinfo_ref_table_offsets[c] = bufpos;
            brstm_encoder_writebytes_i(buffer,new unsigned char[4]{0x00,0x00,0x00,0x00},4,bufpos);
        }
        
        //Second references...
        uint32_t chinfo_ref2_offsets[brstmi->num_channels];
        uint32_t chinfo_ref2_begs[brstmi->num_channels];
        for(unsigned int c=0; c<brstmi->num_channels; c++) {
            chinfo_ref2_begs[c] = bufpos;
            //Write the offset to first reference table
            brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(bufpos-struct_beg,4,BOM),4,off=chinfo_ref_table_offsets[c]);
            //Section code
            brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(0x0300,2,BOM),2,bufpos);
            //Padding
            brstm_encoder_writebytes_i(buffer,new unsigned char[2]{0x00,0x00},2,bufpos);
            //Offset to actual channel info, will be written later
            chinfo_ref2_offsets[c] = bufpos;
            brstm_encoder_writebytes_i(buffer,new unsigned char[4]{0x00,0x00,0x00,0x00},4,bufpos);
        }
        
        //Actual channel information
        for(unsigned int c=0; c<brstmi->num_channels; c++) {
            //Write the offset to second references
            brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(bufpos-chinfo_ref2_begs[c],4,BOM),4,off=chinfo_ref2_offsets[c]);
            //ADPCM coefs
            for(unsigned int a=0; a<16; a++) {
                brstm_encoder_writebytes(buffer,brstm_encoder_getByteInt16(brstmi->ADPCM_coefs[c][a],BOM),2,bufpos);
            }
            chinfo_ips_offsets[c] = bufpos;
            brstm_encoder_writebytes_i(buffer,new unsigned char[2]{0x00,0x00},2,bufpos); //Initial predictor scale, will be written later
            brstm_encoder_writebytes_i(buffer,new unsigned char[2]{0x00,0x00},2,bufpos); //HS1, always zero
            brstm_encoder_writebytes_i(buffer,new unsigned char[2]{0x00,0x00},2,bufpos); //HS2, always zero
            chinfo_lps_offsets[c] = bufpos;
            brstm_encoder_writebytes_i(buffer,new unsigned char[2]{0x00,0x00},2,bufpos); //Loop predictor scale, will be written later
            brstm_encoder_writebytes  (buffer,brstm_encoder_getByteInt16(LoopHS1[c],BOM),2,bufpos); //Loop HS1
            brstm_encoder_writebytes  (buffer,brstm_encoder_getByteInt16(LoopHS2[c],BOM),2,bufpos); //Loop HS2
            brstm_encoder_writebytes_i(buffer,new unsigned char[2]{0x00,0x00},2,bufpos); //Padding
        }
    }
    
    //Finalize INFO chunk
    //Write subchunk offsets
    for(unsigned char c=0; c<3; c++) {
        if(info_subchunks_to_write[c] == 0) continue;
        brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(info_subchunk_offsets[c],4,BOM),4,off=info_subchunk_offset_offsets[c]);
    }
    //Padding
    while(bufpos % 0x20 != 0) {
        brstm_encoder_writebyte(buffer,0,bufpos);
    }
    INFOchunksize = bufpos - INFOchunkoffset;
    //Write chunk length
    brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(INFOchunksize,4,BOM),4,off=INFOchunkoffset+4);
    
    
    //SEEK chunk
    uint32_t SEEKchunkoffset;
    uint32_t SEEKchunksize;
    if(chunks_to_write[1]) {
        if(debugLevel>0) std::cout << "\r" << brstm_encoder_nextspinner(spinner) << " Building headers... (SEEK)             " << std::flush;
        SEEKchunkoffset = bufpos;
        brstm_encoder_writebytes(buffer,(unsigned char*)"SEEK",4,bufpos);
        //Chunk size, will be written later
        brstm_encoder_writebytes_i(buffer,new unsigned char[4]{0x00,0x00,0x00,0x00},4,bufpos);
        
        //Write history sample data
        for(unsigned long b=0;b<brstmi->total_blocks;b++) {
            for(unsigned char c=0;c<brstmi->num_channels;c++) {
                brstm_encoder_writebytes(buffer,brstm_encoder_getByteInt16(HS1[c][b],BOM),2,bufpos); //HS1
                brstm_encoder_writebytes(buffer,brstm_encoder_getByteInt16(HS2[c][b],BOM),2,bufpos); //HS2
            }
        }
        
        //Finalize SEEK chunk
        //Padding
        while(bufpos % 0x20 != 0) {
            brstm_encoder_writebyte(buffer,0,bufpos);
        }
        SEEKchunksize = bufpos - SEEKchunkoffset;
        //Write chunk length
        brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(SEEKchunksize,4,BOM),4,off=SEEKchunkoffset+4);
    }
    
    
    //DATA chunk
    uint32_t DATAchunkoffset;
    uint32_t DATAchunksize;
    uint32_t data_audio_beg;
    DATAchunkoffset = bufpos;
    brstm_encoder_writebytes(buffer,(unsigned char*)"DATA",4,bufpos);
    //Chunk size, will be written later
    brstm_encoder_writebytes_i(buffer,new unsigned char[4]{0x00,0x00,0x00,0x00},4,bufpos);
    //Padding before beginning of audio data
    while(bufpos % 0x20 != 0) {
        brstm_encoder_writebyte(buffer,0,bufpos);
    }
    data_audio_beg = bufpos - (DATAchunkoffset + 8);
    
    
    //Finalize DATA chunk
    //Padding
    while(bufpos % 0x20 != 0) {
        brstm_encoder_writebyte(buffer,0,bufpos);
    }
    DATAchunksize = bufpos - DATAchunkoffset;
    //Write chunk length
    brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(DATAchunksize,4,BOM),4,off=DATAchunkoffset+4);
    
    
    
    //Finalize file
    //Filesize
    brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(bufpos,4,BOM),4,off=0x0C);
    //INFO offset and size
    brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(INFOchunkoffset,4,BOM),4,off=chunks_header_offsets[0]);
    brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(INFOchunksize,4,BOM),4,off=chunks_header_sizes[0]);
    //SEEK offset and size
    if(chunks_to_write[1]) {
        brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(SEEKchunkoffset,4,BOM),4,off=chunks_header_offsets[1]);
        brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(SEEKchunksize,4,BOM),4,off=chunks_header_sizes[1]);
    }
    //DATA offset and size
    brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(DATAchunkoffset,4,BOM),4,off=chunks_header_offsets[2]);
    brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(DATAchunksize,4,BOM),4,off=chunks_header_sizes[2]);
    //Audio offset in INFO
    brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(data_audio_beg,4,BOM),4,off=data_audio_offset_offset);
    
    //copy finished file to brstm_encoded_data
    brstmi->encoded_file = new unsigned char[bufpos];
    memcpy(brstmi->encoded_file,buffer,bufpos);
    delete[] buffer;
    brstmi->encoded_file_size = bufpos;
    
    if(debugLevel>0) std::cout << "\r" << eformat_s[eformat] << " encoding done                                                     \n" << std::flush;
    
    return 0;
}
