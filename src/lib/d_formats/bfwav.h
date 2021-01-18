//OpenRevolution BFWAV reader
//Copyright (C) 2021 IC

//BCWAV and BFWAV are almost the same, this function uses the multi-format function from the BCWAV reader file.

unsigned char brstm_formats_multi_read_bcwav_bfwav(Brstm* brstmi, const unsigned char* fileData, signed int debugLevel, uint8_t decodeAudio, bool eformat);

unsigned char brstm_formats_read_bfwav(Brstm* brstmi, const unsigned char* fileData, signed int debugLevel, uint8_t decodeAudio) {
    return brstm_formats_multi_read_bcwav_bfwav(brstmi, fileData, debugLevel, decodeAudio, 1);
}
