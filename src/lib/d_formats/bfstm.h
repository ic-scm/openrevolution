//OpenRevolution BFSTM reader
//Copyright (C) 2020 IC

//BCSTM and BFSTM are almost the same, this function uses the multi-format function from the BCSTM reader file.

unsigned char brstm_formats_multi_read_bcstm_bfstm(Brstm* brstmi, const unsigned char* fileData, signed int debugLevel, uint8_t decodeAudio, bool eformat);

unsigned char brstm_formats_read_bfstm(Brstm* brstmi,const unsigned char* fileData,signed int debugLevel,uint8_t decodeAudio) {
    return brstm_formats_multi_read_bcstm_bfstm(brstmi,fileData,debugLevel,decodeAudio,1);
}
