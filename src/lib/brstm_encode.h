//C++ BRSTM encoder
//Copyright (C) 2020 Extrasklep
//https://github.com/Extrasklep/revolution

//This file requires brstm.h to be included too

#include <math.h>
#include "utils.h"
#include "audio_encoder.h"
#include "e_formats/all.h"

/* 
 * Build a file and encode audio data
 * 
 * brstmi: Your BRSTM struct pointer
 * debugLevel:
 *    -1 = Never output anything
 *     0 = Only output errors/warnings
 *     1 = Log encoding progress
 * encodeADPCM:
 *     0 = Use ADPCM data from ADPCM_data
 *     1 = Encode PCM_samples to ADPCM
 * 
 * Returns error code (>127) or warning code (<128):
 *        0 = No error
 *      249 = Too many channels
 *      248 = Too many tracks
 *      244 = Unknown track info type
 *      220 = Unsupported or unknown audio codec
 *      210 = Invalid or unsupported file format
 *      200 = Unknown error (this should never happen)
 * 
 * Write your audio data in PCM_samples and other BRSTM header information in the brstm.h variables, more info in README
 * The created file will be in brstm_encoded_data with size brstm_encoded_data_size.
 */
unsigned char brstm_encode(Brstm* brstmi,signed int debugLevel,uint8_t encodeADPCM) {
    //Check for invalid requests
    //Too many tracks
    if(brstmi->num_tracks > 8) {
        if(debugLevel>=0) std::cout << "Too many tracks. Max supported is 8.\n";
        return 248;
    }
    //Too many channels
    if(brstmi->num_channels > 16) {
        if(debugLevel>=0) std::cout << "Too many channels. Max supported is 16.\n";
        return 249;
    }
    //Unsupported track description type
    if(!(brstmi->track_desc_type >= 0 && brstmi->track_desc_type <= 1)) {
        if(debugLevel>=0) std::cout << "Invalid track description type.\n";
        return 244;
    }
    
    unsigned char encres = 0;
    
    if(brstmi->file_format==1) {
        //BRSTM
        encres = brstm_formats_encode_brstm(brstmi,debugLevel,encodeADPCM);
    } else if(brstmi->file_format==2) {
        //BCSTM
        encres = brstm_formats_encode_bcstm(brstmi,debugLevel,encodeADPCM);
    } else if(brstmi->file_format==3) {
        //BFSTM
        encres = brstm_formats_encode_bfstm(brstmi,debugLevel,encodeADPCM);
    } else if(brstmi->file_format==4) {
        //BWAV
        encres = brstm_formats_encode_bwav (brstmi,debugLevel,encodeADPCM);
    } else {
        //Invalid
        if(debugLevel>=0) std::cout << "Invalid file format.\n";
        return 210;
    }
    
    return encres;
}
