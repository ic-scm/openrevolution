//Openrevolution IDSP reader
//Copyright (C) 2021 I.C.

unsigned char brstm_formats_read_idsp(Brstm* brstmi, const unsigned char* fileData, signed int debugLevel, uint8_t decodeAudio) {
    if(debugLevel>=0) {std::cout << "IDSP reading is not currently supported.\n";}
    return 210;
}
