//OpenRevolution BFSTM encoder
//Copyright (C) 2020 IC

//BCSTM and BFSTM are almost the same, this function uses the multi-format function from the BCSTM encoder file.

unsigned char brstm_formats_multi_encode_bcstm_bfstm(Brstm* brstmi, signed int debugLevel, uint8_t encodeADPCM, bool eformat);

unsigned char brstm_formats_encode_bfstm(Brstm* brstmi,signed int debugLevel,uint8_t encodeADPCM) {
    //BFSTM files are usually big endian.
    brstmi->BOM = 1;
    return brstm_formats_multi_encode_bcstm_bfstm(brstmi, debugLevel, encodeADPCM, 1);
}
