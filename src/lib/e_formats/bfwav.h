//OpenRevolution BFWAV writer
//Copyright (C) 2021 IC

unsigned char brstm_formats_encode_bfwav(Brstm* brstmi, signed int debugLevel, uint8_t encodeADPCM) {
    if(debugLevel>=0) std::cout << "BFWAV writing is not supported.\n";
    return 210;
}
