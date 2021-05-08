//C++ BRSTM and other format encoder tools
//Copyright (C) 2020 IC
//https://github.com/ic-scm/openrevolution

//This file requires brstm.h to be included too

//History sample data struct used internally in encoding.
struct brstm_HSData_t {
    int16_t  LoopHS1[16];
    int16_t  LoopHS2[16];
    int16_t* HS1[16];
    int16_t* HS2[16];
};

#include <math.h>
#include "utils.h"
#include "audio_encoder.h"
#include "e_formats/all.h"

/* 
 * Build a file and encode/write audio data
 * 
 * brstmi: Your BRSTM struct pointer
 * debugLevel:
 *    -1 = Never output anything
 *     0 = Only output errors/warnings
 *     1 = Log encoding progress
 * encodeADPCM:
 *     0 = Write ADPCM data from ADPCM_data
 *     1 = Encode PCM_samples to ADPCM or another codec
 * endian (optional):
 *     Byte order to use for the file, if not used the default for the format will be used.
 *     0 = LE
 *     1 = BE
 * 
 * Returns error code (>127) or warning code (<128):
 *        0 = No error
 *      249 = Too many channels
 *      248 = Too many tracks
 *      244 = Invalid track information
 *      222 = Cannot write raw ADPCM data because the codec is not ADPCM
 *      220 = Unsupported or unknown audio codec
 *      210 = Invalid or unsupported file format
 *      206 = Unsupported sample rate
 *      205 = Other invalid input data
 *      Errors other than listed here could be returned.
 *      See the brstm_getErrorString function in brstm.h for the full list of error codes.
 * 
 * Write your audio data in PCM_samples and other BRSTM header information in the brstm.h variables, more info in README
 * The created file will be in brstm_encoded_data with size brstm_encoded_data_size.
 */
__attribute__((warn_unused_result))
unsigned char brstm_encode(Brstm* brstmi, signed int debugLevel, uint8_t encodeADPCM, bool endian) {
    brstmi->BOM = endian;
    
    //Check for invalid input data
    //Too many tracks
    if(brstmi->num_tracks > 8) {
        if(debugLevel>=0) std::cout << "Too many tracks, max supported is 8.\n";
        return 248;
    }
    
    //Too many channels
    if(brstmi->num_channels > 16) {
        if(debugLevel>=0) std::cout << "Too many channels, max supported is 16.\n";
        return 249;
    }
    
    //Unsupported track description type
    if(!(brstmi->track_desc_type >= 0 && brstmi->track_desc_type <= 1)) {
        if(debugLevel>=0) std::cout << "Invalid track description type.\n";
        return 244;
    }
    
    //Trying to write ADPCM data when codec is not ADPCM
    if(encodeADPCM == 0 && brstmi->codec != 2) {
        if(debugLevel>=0) std::cout << "Cannot write raw ADPCM data because the codec is not ADPCM.\n";
        return 222;
    }
    
    //Check loop point
    if((brstmi->loop_start >= brstmi->total_samples) || (brstmi->loop_start > 0 && brstmi->loop_flag == 0)) {
        if(debugLevel>=0) std::cout << "Invalid loop point.\n";
        return 205;
    }
    
    if(brstmi->sample_rate == 0) {
        if(debugLevel >= 0) std::cout << "Invalid sample rate.\n";
        return 206;
    }
    
    if(brstmi->total_samples == 0) {
        if(debugLevel >= 0) std::cout << "Invalid sample count.\n";
        return 205;
    }
    
    if(brstmi->num_channels == 0) {
        if(debugLevel >= 0) std::cout << "Invalid channel count.\n";
        return 205;
    }
    
    if(brstmi->num_tracks == 0) {
        if(debugLevel >= 0) std::cout << "Invalid track count.\n";
        return 244;
    }
    
    //Validate track information
    for(unsigned int t=0; t<brstmi->num_tracks; t++) {
        //Invalid channel count
        if(brstmi->track_num_channels[t] > 2 || brstmi->track_num_channels[t] == 0) {
            if(debugLevel>=0) {
                std::cout << "Invalid track channel count in track " << t << ".\n";
                if(brstmi->track_num_channels[t] > 2) {
                    std::cout << "Creating tracks with more than 2 channels is not currently supported.\n";
                }
            }
            return 244;
        }
        
        //Invalid channel
        if(brstmi->track_lchannel_id[t] >= brstmi->num_channels || brstmi->track_rchannel_id[t] >= brstmi->num_channels) {
            if(debugLevel>=0) std::cout << "Invalid channel ID in track " << t << ".\n";
            return 244;
        }
        
        //Invalid volume and pan values
        bool trackinfo_fail = 0;
        switch(brstmi->track_desc_type) {
            case 0: {
                if(brstmi->track_volume[t] != 0 || brstmi->track_panning[t] != 0) trackinfo_fail = 1;
                break;
            }
            case 1: {
                if(brstmi->track_volume[t] > 0x7F || brstmi->track_panning[t] > 0x7F) trackinfo_fail = 1;
                break;
            }
        }
        
        if(trackinfo_fail) {
            if(debugLevel >= 0) std::cout << "Invalid volume/pan values in track " << t << ".\n";
            return 244;
        }
    }
    
    unsigned char encres = 0;
    
    if(brstmi->file_format == 1) {
        //BRSTM
        encres = brstm_formats_encode_brstm(brstmi,debugLevel,encodeADPCM);
    } else if(brstmi->file_format == 2) {
        //BCSTM
        encres = brstm_formats_encode_bcstm(brstmi,debugLevel,encodeADPCM);
    } else if(brstmi->file_format == 3) {
        //BFSTM
        encres = brstm_formats_encode_bfstm(brstmi,debugLevel,encodeADPCM);
    } else if(brstmi->file_format == 4) {
        //BWAV
        encres = brstm_formats_encode_bwav (brstmi,debugLevel,encodeADPCM);
    } else if(brstmi->file_format == 5) {
        //ORSTM
        encres = brstm_formats_encode_orstm(brstmi,debugLevel,encodeADPCM);
    } else if(brstmi->file_format == 6) {
        //BRWAV
        encres = brstm_formats_encode_brwav(brstmi,debugLevel,encodeADPCM);
    } else if(brstmi->file_format == 7) {
        //BCWAV
        encres = brstm_formats_encode_bcwav(brstmi,debugLevel,encodeADPCM);
    } else if(brstmi->file_format == 8) {
        //BFWAV
        encres = brstm_formats_encode_bfwav(brstmi,debugLevel,encodeADPCM);
    } else if(brstmi->file_format == 9) {
        //IDSP
        encres = brstm_formats_encode_idsp (brstmi,debugLevel,encodeADPCM);
    } else {
        //Invalid
        if(debugLevel>=0) std::cout << "Invalid file format.\n";
        return 210;
    }
    
    return encres;
}

//No optional byte order argument, and backwards compatibility
__attribute__((warn_unused_result))
unsigned char brstm_encode(Brstm* brstmi, signed int debugLevel, uint8_t encodeADPCM) {
    //Check for valid file format variable
    if(brstmi->file_format >= BRSTM_formats_count) {
        if(debugLevel>=0) std::cout << "Invalid file format.\n";
        return 210;
    }
    return brstm_encode(brstmi, debugLevel, encodeADPCM, BRSTM_formats_default_endian[brstmi->file_format]);
}
