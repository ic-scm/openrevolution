# brstm
C++ BRSTM tools

Should work with up to 8 tracks and 16 channels.
Only ADPCM codec is supported (for now).

## Converter/decoder program
Usage:
./brstm input.brstm -o output.wav

-o [output file name] - If this is not used the output will not be saved.

-v - Verbose output

-r - Output a raw audio file instead of WAV

The raw output file can be imported into a program like Audacity as signed 16-bit PCM.

## RtAudio player
Usage:
./brstm_rt file.brstm

-v - Verbose output

**Memory modes**

-m - Load the entire file into memory (default for <15MB files)

-s - Stream the file from disk (default for >15MB files)

-d - Decode all audio data into memory before playing

**Controls**

Left and right arrow keys: Seek 1 second

Up and down arrow keys: Switch track (for multi track BRSTMs)

Space: Pause

Q: Quit

## Encoder program
Usage:
./brstm_encoder input.wav -o output.brstm

-o [output file name] - If this is not used the output will not be saved.

-v - Verbose output

-l [loop point] - Set the loop point. If this is not used the BRSTM file will have no loop.

-c [1 or 2] - Number of channels for each track

## Reencoder
Re-encode a BRSTM (or other supported input format) into a new BRSTM.

Usage:
./brstm_reencoder input.bwav -o output.brstm --ffmpeg "-af volume=10dB"

-o [output file name] - If this is not used the output will not be saved.

-v - Verbose output

--ffmpeg [ffmpeg arguments] - Use ffmpeg in the middle of re-encoding to edit the audio data with the passed ffmpeg arguments (as a single argument!)
Requires FFMPEG to be installed and it may not work on non-unix systems.

## Rebuilder
Losslessly rebuild a BRSTM (or other supported input format) into a new BRSTM.

Usage:
./brstm_rebuilder input.bwav -o output.brstm

-o [output file name] - If this is not used the output will not be saved.

-v - Verbose output

## Thanks to

- [WiiBrew](https://wiibrew.org/wiki/BRSTM_file): BRSTM file structure reference
- [kenrick95/nikku](https://github.com/kenrick95/nikku) and [BrawlLib](https://github.com/libertyernie/brawltools): DSPADPCM decoder reference
- [jackoalan/gc-dspadpcm-encode](https://github.com/jackoalan/gc-dspadpcm-encode): DSPADPCM encoder
- [RtAudio](https://github.com/thestk/rtaudio): RtAudio library

## Planned features

- Merge decoder, encoder, reencoder and rebuilder programs into one program
- Editing BRSTM headers
- Qt GUI
- Support for more file formats (BCSTM, BFSTM etc.)

## brstm.h and brstm_encode.h usage
**WARNING: This is currently under development and there may be major incompatible changes at any time, please use the latest stable release or wait for v2.0**



The BRSTM struct
```cpp
struct Brstm {
    //Byte order mark
    bool BOM;
    //File type, 1 = BRSTM, see brstm.h file for full list
    unsigned int  file_format;
    //Audio codec, 0 = PCM8, 1 = PCM16, 2 = DSPADPCM
    unsigned int  codec;
    bool          loop_flag;
    unsigned int  num_channels;
    unsigned long sample_rate;
    unsigned long loop_start;
    unsigned long total_samples;
    unsigned long audio_offset;
    unsigned long total_blocks;
    unsigned long blocks_size;
    unsigned long blocks_samples;
    unsigned long final_block_size;
    unsigned long final_block_samples;
    unsigned long final_block_size_p;
    
    //track information
    unsigned int  num_tracks;
    unsigned int  track_desc_type;
    unsigned int  track_num_channels[8];
    unsigned int  track_lchannel_id [8];
    unsigned int  track_rchannel_id [8];
    unsigned int  track_volume      [8];
    unsigned int  track_panning     [8];
    
    int16_t* PCM_samples[16];
    int16_t* PCM_buffer[16];
    
    unsigned char* ADPCM_data  [16];
    unsigned char* ADPCM_buffer[16]; //Not used yet
    int16_t  ADPCM_coefs [16][16];
    int16_t* ADPCM_hsamples_1   [16];
    int16_t* ADPCM_hsamples_2   [16];
    
    //Encoder
    unsigned char* encoded_file;
    unsigned long  encoded_file_size;
    
    //Things you probably shouldn't touch
    //block cache
    int16_t* PCM_blockbuffer[16];
    int PCM_blockbuffer_currentBlock = -1;
    bool getbuffer_useBuffer = true;
};
```

Include the files:
```cpp
#include "brstm.h"
```
If you want to use the encoder then include this too:
```cpp
#include "brstm_encode.h"
```
Create your BRSTM struct:
```cpp
Brstm* brstm = new Brstm;
```
Read a file:
```
brstm_read (

Your BRSTM struct pointer (Brstm*),

Raw file data (const unsigned char*),

Console debug level:
-1 = Never output anything
 0 = Only output errors/warnings
 1 = Output information about the BRSTM
 2 = Output all information about the BRSTM (offsets, sizes, ADPCM information etc.)
(signed int),

Decode audio data flag:
0 = Don't decode the audio data (Don't read the DATA chunk)
1 = Decode the audio data into PCM_samples[channel][sample offset]
2 = Write the raw ADPCM data into ADPCM_data
(unsigned char)

)
```
Returns error code (>127) or warning code (<128) (see brstm.h file for full list of error/warning codes). (unsigned char)

You can then read information about the BRSTM from your struct.

Decode a small part of the audio data into a buffer:
```
brstm_getbuffer (

Your BRSTM struct pointer (Brstm*),

Raw file data (const unsigned char*),

Sample offset (unsigned long),

Amount of samples in the buffer
(can't be bigger than the amount of samples per block (HEAD1_blocks_samples)!)
(unsigned int)

)
```
You can then read the requested buffer from PCM_buffer[channel][sample offset (from the sample offset passed to brstm_getbuffer)].

### Streaming
Read a file:
```
brstm_fstream_read (

Your BRSTM struct pointer (Brstm*),

std::ifstream with an open BRSTM file (std::ifstream&),

Console debug level (same as brstm_read) (signed int)

)
```
Returns error/warning codes like brstm_read. (unsigned char)

You can then read information about the BRSTM just like with brstm_read.

Get buffer:
```
brstm_fstream_getbuffer (

Your BRSTM struct pointer (Brstm*),

std::ifstream with an open BRSTM file (std::ifstream&),

Sample offset (unsigned long),

Amount of samples in the buffer
(can't be bigger than the amount of samples per block (HEAD1_blocks_samples)!)
(unsigned int)

)
```
You can then read the requested buffer from PCM_buffer[channel][sample offset (from the sample offset passed to brstm_fstream_getbuffer)].

Remember to close the file (free any buffers and allocated memory) with brstm_close(Brstm*) before deleting your struct!
```cpp
brstm_close(brstm);
delete brstm;
```

### Encoder

Create a BRSTM struct like with reading (or even read into that struct to reencode/rebuild a file)

Write your Int16 PCM audio data to PCM_samples[c] for each channel

You can also write raw DSPADPCM data to ADPCM_data[c] and the coefs to ADPCM_coefs[c]

Set audio and BRSTM details:
```cpp
codec         = BRSTM codec (only 2 is supported)
loop_flag     = 0 or 1, loop flag
num_channels  = Number of audio channels
sample_rate   = Audio sample rate
loop_start    = Loop start point (sample number in a single channel)
total_samples = Total samples (in a single channel)

/*/--- TRACKS ---/*/

num_tracks      = Number of tracks
track_desc_type = Track description type
//You can set this to 0 or 1 (1 can store volume and panning information, 0 doesn't)
//See the WiiBrew page about BRSTM files for more information.

track_num_channels[track] = Number of channels in the track. (1 or 2)
track_lchannel_id [track] = BRSTM Channel ID for the left channel of this track.
track_rchannel_id [track] = BRSTM Channel ID for the right channel of this track.
//Set to 0 if this is a mono track. It's recommended to always set the channel IDs in order
//(from 0) because some decoders probably don't care about this information and all
//BRSTM files are like that.

track_volume [track] = Volume of the track. (0x00 to 0x7F)
track_panning[track] = Left to right panning of the track. (0x00 to 0x7F)
//Description type 1 only.
//Most decoders/players (including this one) probably don't care about this information.

// Example - standard stereo BRSTM

Brstm* brstm = new Brstm;

brstm->codec         = 2;
brstm->loop_flag     = 1;
brstm->num_channels  = 2;
brstm->sample_rate   = 44100;
brstm->loop_start    = 41234;
brstm->total_samples = 500000;

brstm->PCM_samples[0] = new int16_t[500000];
brstm->PCM_samples[1] = new int16_t[500000];

brstm->num_tracks      = 1;
brstm->track_desc_type = 1;

brstm->track_num_channels[0] = 2;
brstm->track_lchannel_id [0] = 0;
brstm->track_rchannel_id [0] = 1;

brstm->track_volume      [0] = 0x7F;
brstm->track_panning     [0] = 0x40;

brstm_encode(/*

Console debug level:
-1 = Never output anything
 0 = Only output errors/warnings
 1 = Log encoding progress
(signed int),

Encode ADPCM flag:
 0 = Use the ADPCM data from ADPCM_data (requires coefs in ADPCM_coefs[ch][coef])
 1 = Encode PCM_samples into ADPCM

Returns error code (>127) or warning code (<128)
(see brstm_encode.h file for full list of error/warning codes). (unsigned char)

*/);

// Remember to free PCM_samples!

file.write(brstm->encoded_file,brstm->encoded_file_size);

delete brstm;

```

### Editing headers
Coming soon.
