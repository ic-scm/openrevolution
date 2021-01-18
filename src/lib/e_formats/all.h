//Include all formats
#include "wav.h"
#include "brstm.h"
#include "bcstm.h"
#include "bfstm.h"
#include "bwav.h"
#include "orstm.h"
#include "brwav.h"
#include "bcwav.h"
#include "bfwav.h"

unsigned char brstm_formats_encode_wav   (Brstm* brstmi, signed int debugLevel, uint8_t encodeADPCM);
unsigned char brstm_formats_encode_brstm (Brstm* brstmi, signed int debugLevel, uint8_t encodeADPCM);
unsigned char brstm_formats_encode_bcstm (Brstm* brstmi, signed int debugLevel, uint8_t encodeADPCM);
unsigned char brstm_formats_encode_bfstm (Brstm* brstmi, signed int debugLevel, uint8_t encodeADPCM);
unsigned char brstm_formats_encode_bwav  (Brstm* brstmi, signed int debugLevel, uint8_t encodeADPCM);
unsigned char brstm_formats_encode_orstm (Brstm* brstmi, signed int debugLevel, uint8_t encodeADPCM);
unsigned char brstm_formats_encode_brwav (Brstm* brstmi, signed int debugLevel, uint8_t encodeADPCM);
unsigned char brstm_formats_encode_bcwav (Brstm* brstmi, signed int debugLevel, uint8_t encodeADPCM);
unsigned char brstm_formats_encode_bfwav (Brstm* brstmi, signed int debugLevel, uint8_t encodeADPCM);
