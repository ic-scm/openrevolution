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
    
    return 210;
}
