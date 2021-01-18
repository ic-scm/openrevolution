//OpenRevolution BCWAV reader
//Copyright (C) 2021 IC

unsigned char brstm_formats_read_bcwav(Brstm* brstmi, const unsigned char* fileData, signed int debugLevel, uint8_t decodeAudio) {
    if(debugLevel>=0) std::cout << "BCWAV reading is not supported.\n";
    return 210;
}
