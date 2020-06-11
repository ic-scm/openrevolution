//Revolution BFSTM reader
//Copyright (C) 2020 Extrasklep

unsigned char brstm_formats_read_bfstm(Brstm* brstmi,const unsigned char* fileData,signed int debugLevel,uint8_t decodeAudio) {
    if(debugLevel>=0) {std::cout << "Warning: BFSTM reader work is in progress\n";}
    
    bool &BOM = brstmi->BOM;
    
    //BFSTM file magic words
    const char* emagic1 = "FSTM";
    const char* emagic2 = "INFO";
    const char* emagic3 = "SEEK";
    const char* emagic4 = "DATA";
    char* magicstr = brstm_getSliceAsString(fileData,0,4);
    
    if(strcmp(magicstr,emagic1) != 0) {
        if(debugLevel>=0) {std::cout << "Invalid BFSTM file.\n";}
        return 255;
    }
    
    //Header
    //Byte order mark
    if(brstm_getSliceAsInt16Sample(fileData,0x04,1) == -257) {
        BOM = 1; //Big endian
    } else {
        BOM = 0; //Little endian
    }
    //uint16_t header_size = brstm_getSliceAsNumber(fileData,0x06,2,BOM);
    uint32_t file_size       = brstm_getSliceAsNumber(fileData,0x0C,4,BOM);
    uint16_t num_file_chunks = brstm_getSliceAsNumber(fileData,0x10,2,BOM);
    
    uint32_t INFO_offset = brstm_getSliceAsNumber(fileData,0x18,4,BOM);
    uint32_t INFO_size   = brstm_getSliceAsNumber(fileData,0x1C,4,BOM);
    uint32_t SEEK_offset = brstm_getSliceAsNumber(fileData,0x24,4,BOM);
    uint32_t SEEK_size   = brstm_getSliceAsNumber(fileData,0x28,4,BOM);
    uint32_t DATA_offset = brstm_getSliceAsNumber(fileData,0x30,4,BOM);
    uint32_t DATA_size   = brstm_getSliceAsNumber(fileData,0x34,4,BOM);
    
    //INFO chunk
    magicstr = brstm_getSliceAsString(fileData,INFO_offset,4);
    if(strcmp(magicstr,emagic2) != 0) {
        if(debugLevel>=0) {std::cout << "Invalid INFO chunk.";}
        return 250;
    }
    //INFO header (offsets to other chunks in INFO)
    int32_t INFO_stream_offset  = brstm_getSliceAsNumber(fileData,0x0C+INFO_offset,4,BOM);
    int32_t INFO_track_offset   = brstm_getSliceAsNumber(fileData,0x14+INFO_offset,4,BOM);
    int32_t INFO_channel_offset = brstm_getSliceAsNumber(fileData,0x1C+INFO_offset,4,BOM);
    
    //Stream info
    if(INFO_stream_offset != -1) {
        INFO_stream_offset += INFO_offset + 8;
        
        brstmi->codec = brstm_getSliceAsNumber(fileData,0x00+INFO_stream_offset,1,BOM);
    }
    
    return 0;
}
