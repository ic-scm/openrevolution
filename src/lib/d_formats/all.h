//Include all formats
#include "brstm.h"
#include "bcstm.h"
#include "bfstm.h"
#include "bwav.h"
#include "orstm.h"

unsigned char brstm_formats_read_brstm(Brstm* brstmi,const unsigned char* fileData,signed int debugLevel,uint8_t decodeAudio);
unsigned char brstm_formats_read_bcstm(Brstm* brstmi,const unsigned char* fileData,signed int debugLevel,uint8_t decodeAudio);
unsigned char brstm_formats_read_bfstm(Brstm* brstmi,const unsigned char* fileData,signed int debugLevel,uint8_t decodeAudio);
unsigned char brstm_formats_read_bwav (Brstm* brstmi,const unsigned char* fileData,signed int debugLevel,uint8_t decodeAudio);
unsigned char brstm_formats_read_orstm(Brstm* brstmi,const unsigned char* fileData,signed int debugLevel,uint8_t decodeAudio);
