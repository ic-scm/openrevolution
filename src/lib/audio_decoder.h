//OpenRevolution audio decoder
//Copyright (C) 2020 IC

#pragma once

//dataType will be 1 for disk streaming mode (fileData will be null) so brstm_getblock
//will know to do disk streaming stuff instead of just getting a slice of fileData
void brstm_decode_block(Brstm* brstmi,unsigned long b,unsigned int c,const unsigned char* fileData,bool dataType,int16_t** decodeDest,unsigned long decodeDestOff) {
    unsigned long posOffset;
    if(brstmi->audio_stream_format == 0) {
        posOffset = (brstmi->blocks_size*c);
        posOffset+=b*(brstmi->blocks_size*brstmi->num_channels);
    } else {
        posOffset = 0;
        posOffset+=b*(brstmi->blocks_size);
    }
    unsigned long outputPos = 0; //position in decoded PCM samples output array
    
    unsigned long c_writtensamples = 0;
    
    //Read block
    unsigned int currentBlockSize    = brstmi->blocks_size;
    unsigned int currentBlockSamples = brstmi->blocks_samples;
    //Final block
    if(b==brstmi->total_blocks-1) {
        currentBlockSize    = brstmi->final_block_size;
        currentBlockSamples = brstmi->final_block_samples;
    }
    if(brstmi->audio_stream_format != 1) {
        if(b>=brstmi->total_blocks-1 && c>0) {
            //Go back to the previous position
            posOffset-=brstmi->blocks_size*brstmi->num_channels;
            //Go to the next block in position of first channel
            posOffset+=brstmi->blocks_size*(brstmi->num_channels-c);
            //Jump to the correct channel in the final block
            posOffset+=brstmi->final_block_size_p*c;
        }
    }
    //Get data from just the current block
    //This function exists in the including file (brstm.h)
    unsigned char* blockData = brstm_getblock(fileData,dataType,brstmi->audio_offset+posOffset,currentBlockSize);
    
    //Decode the audio
    if(brstmi->codec == 0) {
        //8 bit PCM
        if(brstmi->audio_stream_format == 0) {
            for(unsigned long sampleIndex=0;sampleIndex<currentBlockSamples;sampleIndex++) {
                decodeDest[c][decodeDestOff+(outputPos++)] = ((int16_t)blockData[sampleIndex])*256;
                c_writtensamples++;
            }
        } else if(brstmi->audio_stream_format == 1) {
            for(unsigned long sampleIndex=0;sampleIndex<currentBlockSamples;sampleIndex++) {
                decodeDest[c][decodeDestOff+(outputPos++)] = ((int16_t)blockData[sampleIndex*brstmi->num_channels+c])*256;
                c_writtensamples++;
            }
        }
    } else if(brstmi->codec == 1) {
        //16 bit PCM
        if(brstmi->audio_stream_format == 0) {
            for(unsigned long sampleIndex=0;sampleIndex<currentBlockSamples;sampleIndex++) {
                decodeDest[c][decodeDestOff+(outputPos++)] = brstm_getSliceAsInt16Sample(blockData,sampleIndex*2,brstmi->BOM);
                c_writtensamples++;
            }
        } else if(brstmi->audio_stream_format == 1) {
            for(unsigned long sampleIndex=0;sampleIndex<currentBlockSamples;sampleIndex++) {
                decodeDest[c][decodeDestOff+(outputPos++)] = brstm_getSliceAsInt16Sample(blockData,sampleIndex*2*brstmi->num_channels+(c*2),brstmi->BOM);
                c_writtensamples++;
            }
        }
    } else if(brstmi->codec == 2 && brstmi->audio_stream_format != 1) {
        //4 bit DSPADPCM (does not support stream format 1)
        const unsigned char ps = blockData[0];
        const   signed int  yn1 = brstmi->ADPCM_hsamples_1[c][b], yn2 = brstmi->ADPCM_hsamples_2[c][b];
        
        //Magic adapted from brawllib's ADPCMState.cs
        signed int 
        cps = ps,
        cyn1 = yn1,
        cyn2 = yn2;
        unsigned long dataIndex = 0;
        
        int16_t* coefs = brstmi->ADPCM_coefs[c];
        
        for (unsigned long sampleIndex=0;sampleIndex<currentBlockSamples;) {
            long outSample = 0;
            if (sampleIndex % 14 == 0) {
                cps = blockData[dataIndex++];
            }
            if ((sampleIndex++ & 1) == 0) {
                outSample = blockData[dataIndex] >> 4;
            } else {
                outSample = blockData[dataIndex++] & 0x0f;
            }
            if (outSample >= 8) {
                outSample -= 16;
            }
            const long scale = 1 << (cps & 0x0f);
            const long cIndex = (cps >> 4) << 1;
            
            outSample = (0x400 + ((scale * outSample) << 11) + coefs[brstm_clamp(cIndex, 0, 15)] * cyn1 + coefs[brstm_clamp(cIndex + 1, 0, 15)] * cyn2) >> 11;
            
            cyn2 = cyn1;
            cyn1 = brstm_clamp(outSample, -32768, 32767);
            
            decodeDest[c][decodeDestOff+(outputPos++)] = cyn1;
            c_writtensamples++;
        }
        
        //Overwrite loaded history samples with decoded samples
        if(b<brstmi->total_blocks-1) {
            brstmi->ADPCM_hsamples_1[c][b+1] = decodeDest[c][decodeDestOff+c_writtensamples-1];
            brstmi->ADPCM_hsamples_2[c][b+1] = decodeDest[c][decodeDestOff+c_writtensamples-2];
        }
    }
    
    posOffset+=brstmi->blocks_size*brstmi->num_channels;
}

//Default decoding into PCM_blockbuffer
void brstm_decode_block(Brstm* brstmi,unsigned long b,const unsigned char* fileData,bool dataType) {
    for(unsigned int c=0;c<brstmi->num_channels;c++) {
        //Create new array of samples for the current channel
        delete[] brstmi->PCM_blockbuffer[c];
        brstmi->PCM_blockbuffer[c] = new int16_t[brstmi->blocks_samples];
        
        brstm_decode_block(brstmi,b,c,fileData,dataType,brstmi->PCM_blockbuffer,0);
    }
    brstmi->PCM_blockbuffer_currentBlock = b;
}
