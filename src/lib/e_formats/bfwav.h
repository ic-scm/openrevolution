//OpenRevolution BFWAV writer
//Copyright (C) 2021 IC

//BCWAV and BFWAV are almost the same, this function uses the multi-format function from the BCWAV encoder file.

unsigned char brstm_formats_multi_encode_bcwav_bfwav(Brstm* brstmi, signed int debugLevel, uint8_t encodeADPCM, bool eformat);

unsigned char brstm_formats_encode_bfwav(Brstm* brstmi,signed int debugLevel,uint8_t encodeADPCM) {
    return brstm_formats_multi_encode_bcwav_bfwav(brstmi, debugLevel, encodeADPCM, 1);
}
