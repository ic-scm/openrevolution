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
#include "idsp.h"

unsigned char brstm_formats_read_wav   (Brstm* brstmi, const unsigned char* fileData, signed int debugLevel, uint8_t decodeAudio);
unsigned char brstm_formats_read_brstm (Brstm* brstmi, const unsigned char* fileData, signed int debugLevel, uint8_t decodeAudio);
unsigned char brstm_formats_read_bcstm (Brstm* brstmi, const unsigned char* fileData, signed int debugLevel, uint8_t decodeAudio);
unsigned char brstm_formats_read_bfstm (Brstm* brstmi, const unsigned char* fileData, signed int debugLevel, uint8_t decodeAudio);
unsigned char brstm_formats_read_bwav  (Brstm* brstmi, const unsigned char* fileData, signed int debugLevel, uint8_t decodeAudio);
unsigned char brstm_formats_read_orstm (Brstm* brstmi, const unsigned char* fileData, signed int debugLevel, uint8_t decodeAudio);
unsigned char brstm_formats_read_brwav (Brstm* brstmi, const unsigned char* fileData, signed int debugLevel, uint8_t decodeAudio);
unsigned char brstm_formats_read_bcwav (Brstm* brstmi, const unsigned char* fileData, signed int debugLevel, uint8_t decodeAudio);
unsigned char brstm_formats_read_bfwav (Brstm* brstmi, const unsigned char* fileData, signed int debugLevel, uint8_t decodeAudio);
unsigned char brstm_formats_read_idsp  (Brstm* brstmi, const unsigned char* fileData, signed int debugLevel, uint8_t decodeAudio);
