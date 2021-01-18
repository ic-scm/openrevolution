//OpenRevolution BRWAV writer
//Copyright (C) 2021 IC

unsigned char brstm_formats_encode_brwav(Brstm* brstmi, signed int debugLevel, uint8_t encodeADPCM) {
    if(debugLevel>=0) std::cout << "BRWAV writing is not supported.\n";
    return 210;
}
