//OpenRevolution BCSTM/BFSTM encoder
//Copyright (C) 2020 IC

//BCSTM and BFSTM are almost the same, the main encoder function will take a special argument so it can also be used as the BFSTM encoder.

unsigned char brstm_formats_multi_encode_bcstm_bfstm(Brstm* brstmi, signed int debugLevel, uint8_t encodeADPCM, bool eformat);

unsigned char brstm_formats_encode_bcstm(Brstm* brstmi, signed int debugLevel, uint8_t encodeADPCM) {
    return brstm_formats_multi_encode_bcstm_bfstm(brstmi,debugLevel,encodeADPCM,0);
}


//bool eformat: 0 = BCSTM, 1 = BFSTM.
unsigned char brstm_formats_multi_encode_bcstm_bfstm(Brstm* brstmi, signed int debugLevel, uint8_t encodeADPCM, bool eformat) {
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
    //Version number
    uint8_t version[4] = {0x00,0x00,0x00,0x00};
    if(eformat == 0) {
        version[3] = 0x02;
    } else {
        version[1] = 0x03;
    }
    brstm_encoder_writebytes(buffer,version,4,bufpos);
    //File size, will be written later
    brstm_encoder_writebytes_i(buffer,new unsigned char[4]{0x00,0x00,0x00,0x00},4,bufpos);
    //Number of chunks
    brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(3,2,BOM),2,bufpos);
    //Padding
    brstm_encoder_writebytes_i(buffer,new unsigned char[2]{0x00,0x00},2,bufpos);
    
    //Chunk information
    bool chunks_to_write[4] = {1,1,1,0};
    if(brstmi->codec != 2) chunks_to_write[1] = 0;
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
    bool info_subchunks_to_write[3] = {1,1,1};
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
        if(version[2] >= 1 || version[1] >= 1 || version[0] >= 1) {
            //Region info size? Seems to be 0x100 in all files.
            brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(0x100,2,BOM),2,bufpos);
            brstm_encoder_writebytes_i(buffer,new unsigned char[2]{0x00,0x00},2,bufpos); //Padding
            //Region info flag, not used here.
            brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(0,2,BOM),2,bufpos);
            brstm_encoder_writebytes_i(buffer,new unsigned char[2]{0x00,0x00},2,bufpos); //Padding
            //Region info offset from beginning of REGN chunk content. Unsupported and not used here.
            brstm_encoder_writebytes_i(buffer,new unsigned char[4]{0xFF,0xFF,0xFF,0xFF},4,bufpos);
        }
        //"Original" loop start and end in newer files.
        if((version[1] >= 1 && eformat == 0) || (version[1] >= 4 && eformat == 1) || version[0] >= 1) {
            brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(brstmi->loop_start,4,BOM),4,bufpos);
            brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(brstmi->total_samples,4,BOM),4,bufpos);
        }
    }
    
    //Track info reference table
    uint32_t trinfo_struct_beg;
    uint32_t trinfo_ref_table_offsets[brstmi->num_tracks];
    if(info_subchunks_to_write[1]) {
        info_subchunk_offsets[1] = bufpos - (INFOchunkoffset + 8);
        trinfo_struct_beg = bufpos;
        //Number of references (tracks)
        brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(brstmi->num_tracks,4,BOM),4,bufpos);
        //References
        for(unsigned int t=0; t<brstmi->num_tracks; t++) {
            //Section code
            brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(0x4101,2,BOM),2,bufpos);
            //Padding
            brstm_encoder_writebytes_i(buffer,new unsigned char[2]{0x00,0x00},2,bufpos);
            //Offset to track info, will be written later
            trinfo_ref_table_offsets[t] = bufpos;
            brstm_encoder_writebytes_i(buffer,new unsigned char[4]{0x00,0x00,0x00,0x00},4,bufpos);
        }
    }
    
    //Channel info reference table
    uint32_t chinfo_struct_beg;
    uint32_t chinfo_ref_table_offsets[brstmi->num_channels];
    if(info_subchunks_to_write[2]) {
        info_subchunk_offsets[2] = bufpos - (INFOchunkoffset + 8);
        chinfo_struct_beg = bufpos;
        //Number of references (channels)
        brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(brstmi->num_channels,4,BOM),4,bufpos);
        //References
        for(unsigned int c=0; c<brstmi->num_channels; c++) {
            //Section code
            brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(0x4102,2,BOM),2,bufpos);
            //Padding
            brstm_encoder_writebytes_i(buffer,new unsigned char[2]{0x00,0x00},2,bufpos);
            //Offset to channel info reference, will be written later
            chinfo_ref_table_offsets[c] = bufpos;
            brstm_encoder_writebytes_i(buffer,new unsigned char[4]{0x00,0x00,0x00,0x00},4,bufpos);
        }
    }
    
    //Track information itself
    if(info_subchunks_to_write[1]) {
        //Track information
        for(unsigned int t=0; t<brstmi->num_tracks; t++) {
            uint32_t trinfo_thistrack_beg = bufpos;
            //Write the offset to reference table
            brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(bufpos-trinfo_struct_beg,4,BOM),4,off=trinfo_ref_table_offsets[t]);
            if(brstmi->track_desc_type == 1) {
                //Volume
                brstm_encoder_writebyte(buffer,brstmi->track_volume[t],bufpos);
                //Panning
                brstm_encoder_writebyte(buffer,brstmi->track_panning[t],bufpos);
            }
            //Write defaults if the input data's track description type is 0.
            else {
                brstm_encoder_writebyte(buffer,0x7F,bufpos);
                brstm_encoder_writebyte(buffer,0x40,bufpos);
            }
            //Padding / unused data
            brstm_encoder_writebytes_i(buffer,new unsigned char[2]{0x00,0x00},2,bufpos);
            
            //Channel index reference
            //Section code
            brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(0x0100,2,BOM),2,bufpos);
            //Padding
            brstm_encoder_writebytes_i(buffer,new unsigned char[2]{0x00,0x00},2,bufpos);
            //Offset to channel index table
            brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(bufpos+4-trinfo_thistrack_beg,4,BOM),4,bufpos);
            
            //Channel index table
            //Channel count for track
            brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(brstmi->track_num_channels[t],4,BOM),4,bufpos);
            //Channel IDs
            brstm_encoder_writebyte(buffer,brstmi->track_lchannel_id[t],bufpos);
            if(brstmi->track_num_channels[t] == 2) brstm_encoder_writebyte(buffer,brstmi->track_rchannel_id[t],bufpos);
            //Padding to 0x04
            while(bufpos % 0x04 != 0) brstm_encoder_writebyte(buffer,0,bufpos);
        }
    }
    
    //Channel information itself
    uint32_t chinfo_ips_offsets[brstmi->num_channels];
    uint32_t chinfo_lps_offsets[brstmi->num_channels];
    if(info_subchunks_to_write[2]) {
        //Second references...
        uint32_t chinfo_ref2_offsets[brstmi->num_channels];
        uint32_t chinfo_ref2_begs[brstmi->num_channels];
        for(unsigned int c=0; c<brstmi->num_channels; c++) {
            chinfo_ref2_begs[c] = bufpos;
            //Write the offset to first reference table
            brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(bufpos-chinfo_struct_beg,4,BOM),4,off=chinfo_ref_table_offsets[c]);
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
    if(debugLevel>0) std::cout << "\r" << brstm_encoder_nextspinner(spinner) << " Writing audio data...                   " << std::flush;
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
    
    //Encode / write audio
    //DSPADPCM
    unsigned char** ADPCMdata;
    if(brstmi->codec == 2) {
        if(encodeADPCM == 1) {
            ADPCMdata = new unsigned char* [brstmi->num_channels];
            for(unsigned int c=0;c<brstmi->num_channels;c++) {
                ADPCMdata[c] = new unsigned char[brstm_getBytesForAdpcmSamples(brstmi->total_samples)];
            }
            brstm_encode_adpcm(brstmi,ADPCMdata,debugLevel);
        } else {
            ADPCMdata = brstmi->ADPCM_data;
        }
        //Write ADPCM information to channel information in INFO chunk
        for(unsigned int c=0;c<brstmi->num_channels;c++) {
            brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(ADPCMdata[c][0],2,BOM),2,off=chinfo_ips_offsets[c]); //Initial scale
            brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(ADPCMdata[c][(unsigned long)(brstmi->loop_start / 1.75)],2,BOM),2,off=chinfo_lps_offsets[c]); //Loop initial scale
        }
    }
    //Write audio data to output file buffer
    for(unsigned long b=0;b<brstmi->total_blocks-1;b++) {
        for(unsigned int c=0;c<brstmi->num_channels;c++) {
            switch(brstmi->codec) {
                //4-bit DSPADPCM
                case 2: {
                    brstm_encoder_writebytes(buffer,&ADPCMdata[c][b*brstmi->blocks_size],brstmi->blocks_size,bufpos);
                    break;
                }
                //8-bit PCM
                case 0: {
                    for(unsigned int i=0; i<brstmi->blocks_samples; i++) {
                        brstm_encoder_writebyte(buffer,(brstmi->PCM_samples[c][b*brstmi->blocks_samples+i])/256,bufpos);
                    }
                    break;
                }
                //16-bit PCM
                case 1: {
                    for(unsigned int i=0; i<brstmi->blocks_samples; i++) {
                        brstm_encoder_writebytes(
                            buffer,brstm_encoder_getByteInt16(brstmi->PCM_samples[c][b*brstmi->blocks_samples+i],BOM),2,bufpos
                        );
                        if(!(b%4) && debugLevel>0) std::cout << "\r" << brstm_encoder_nextspinner(spinner) << " Writing PCM data... ("
                        << floor(((float)(b*brstmi->blocks_samples+i)/brstmi->total_samples) * 100) << "%)          ";
                    }
                    break;
                }
            }
        }
    }
    //Final block
    for(unsigned int c=0;c<brstmi->num_channels;c++) {
        unsigned int i=brstmi->final_block_size;
        switch(brstmi->codec) {
            //4-bit DSPADPCM
            case 2: {
                brstm_encoder_writebytes(buffer,&ADPCMdata[c][(brstmi->total_blocks-1)*brstmi->blocks_size],brstmi->final_block_size,bufpos);
                break;
            }
            //8-bit PCM
            case 0: {
                for(unsigned int i=0; i<brstmi->final_block_samples; i++) {
                    brstm_encoder_writebyte(buffer,(brstmi->PCM_samples[c][(brstmi->total_blocks-1)*brstmi->blocks_samples+i])/256,bufpos);
                }
                break;
            }
            //16-bit PCM
            case 1: {
                for(unsigned int i=0; i<brstmi->final_block_samples; i++) {
                    brstm_encoder_writebytes(
                        buffer,brstm_encoder_getByteInt16(brstmi->PCM_samples[c][(brstmi->total_blocks-1)*brstmi->blocks_samples+i],BOM),2,bufpos
                    );
                }
                break;
            }
        }
        if(brstmi->codec == 2 && encodeADPCM == 1) delete[] ADPCMdata[c]; //delete the ADPCM data only if we made it locally
        //padding
        while(i<brstmi->final_block_size_p) {
            brstm_encoder_writebyte(buffer,0x00,bufpos);
            i++;
        }
    }
    if(brstmi->codec == 2 && encodeADPCM == 1) delete[] ADPCMdata; //delete the ADPCM data only if we made it locally
    
    
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
