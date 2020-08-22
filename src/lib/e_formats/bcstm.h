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
    
    
    //INFO chunk
    uint32_t INFOchunkoffset = bufpos;
    brstm_encoder_writebytes(buffer,(unsigned char*)"INFO",4,bufpos);
    //Chunk size, will be written later
    brstm_encoder_writebytes_i(buffer,new unsigned char[4]{0x00,0x00,0x00,0x00},4,bufpos);
    //Subchunk references: stream info, track info, channel info
    //Track info is unsupported.
    bool info_subchunks_to_write[3] = {1,0,1};
    const uint16_t info_subchunk_codes[3] = {0x4100,0x0101,0x0101};
    uint32_t info_subchunk_offset_offsets[3] = {0,0,0};
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
    
    //Finalize INFO chunk
    //Padding
    while(bufpos % 0x20 != 0) {
        brstm_encoder_writebyte(buffer,0,bufpos);
    }
    uint32_t INFOchunksize = bufpos - INFOchunkoffset;
    //Write chunk length
    brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(INFOchunksize,4,BOM),4,off=INFOchunkoffset+4);
    
    
    
    
    
    //Finalize file
    //Filesize
    brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(bufpos,4,BOM),4,off=0x0C);
    //INFO offset and size
    brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(INFOchunkoffset,4,BOM),4,off=chunks_header_offsets[0]);
    brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(INFOchunksize,4,BOM),4,off=chunks_header_sizes[0]);
    //ADPCM offset in INFO
    //brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(DATAchunkoffset+0x20,4,BOM),4,off=HEADchunkoffset + 8 + HEAD1offset + 0x10);
    
    //copy finished file to brstm_encoded_data
    brstmi->encoded_file = new unsigned char[bufpos];
    memcpy(brstmi->encoded_file,buffer,bufpos);
    delete[] buffer;
    brstmi->encoded_file_size = bufpos;
    
    if(debugLevel>0) std::cout << "\r" << eformat_s[eformat] << " encoding done                                                     \n" << std::flush;
    
    return 0;
}
