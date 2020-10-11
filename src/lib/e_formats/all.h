//Include all formats
#include "wav.h"
#include "brstm.h"
#include "bcstm.h"
#include "bfstm.h"
#include "bwav.h"
#include "orstm.h"
#include "raw.h"

unsigned char brstm_formats_encode_wav  (Brstm* brstmi,signed int debugLevel,uint8_t encodeADPCM);
unsigned char brstm_formats_encode_brstm(Brstm* brstmi,signed int debugLevel,uint8_t encodeADPCM);
unsigned char brstm_formats_encode_bcstm(Brstm* brstmi,signed int debugLevel,uint8_t encodeADPCM);
unsigned char brstm_formats_encode_bfstm(Brstm* brstmi,signed int debugLevel,uint8_t encodeADPCM);
unsigned char brstm_formats_encode_bwav (Brstm* brstmi,signed int debugLevel,uint8_t encodeADPCM);
unsigned char brstm_formats_encode_orstm(Brstm* brstmi,signed int debugLevel,uint8_t encodeADPCM);
unsigned char brstm_formats_encode_raw  (Brstm* brstmi,signed int debugLevel,uint8_t encodeADPCM);
