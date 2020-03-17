# brstm
C++ BRSTM tools

Should work with up to 8 tracks and 16 channels.
Only ADPCM codec is supported (for now).

## Converter/decoder program
Usage:
./brstm input.brstm -o output.raw

-o [output file name] - If this is not used the output will not be saved.

-v - Verbose output

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

## Thanks to

- [WiiBrew](https://wiibrew.org/wiki/BRSTM_file): BRSTM file structure reference
- [kenrick95/nikku](https://github.com/kenrick95/nikku) and [BrawlLib](https://github.com/libertyernie/brawltools): DSPADPCM decoder reference
- [jackoalan/gc-dspadpcm-encode](https://github.com/jackoalan/gc-dspadpcm-encode): DSPADPCM encoder
- [RtAudio](https://github.com/thestk/rtaudio): RtAudio library

## Planned features

- Editing BRSTM headers
- Qt GUI
- Support for other file formats (BCSTM, BFSTM etc.)
- Lossless conversion between the other formats
- Use structs instead of global variables declared in main file

## brstm.h and brstm_encode.h usage
Declare these variables in your code before including the brstm.h file:
```cpp
unsigned int  BRSTM_format; //File type, 1 = BRSTM, see brstm.h for full list
unsigned int  HEAD1_codec;
unsigned int  HEAD1_loop;
unsigned int  HEAD1_num_channels;
unsigned int  HEAD1_sample_rate;
unsigned long HEAD1_loop_start;
unsigned long HEAD1_total_samples;
unsigned long HEAD1_ADPCM_offset;
unsigned long HEAD1_total_blocks;
unsigned long HEAD1_blocks_size;
unsigned long HEAD1_blocks_samples;
unsigned long HEAD1_final_block_size;
unsigned long HEAD1_final_block_samples;
unsigned long HEAD1_final_block_size_p;
unsigned long HEAD1_samples_per_ADPC;
unsigned long HEAD1_bytes_per_ADPC;

unsigned int  HEAD2_num_tracks;
unsigned int  HEAD2_track_type;

unsigned int  HEAD2_track_num_channels[8] = {0,0,0,0,0,0,0,0};
unsigned int  HEAD2_track_lchannel_id [8] = {0,0,0,0,0,0,0,0};
unsigned int  HEAD2_track_rchannel_id [8] = {0,0,0,0,0,0,0,0};

unsigned int  HEAD2_track_volume      [8] = {0,0,0,0,0,0,0,0};
unsigned int  HEAD2_track_panning     [8] = {0,0,0,0,0,0,0,0};

unsigned int  HEAD3_num_channels;

int16_t* PCM_samples[16];
int16_t* PCM_buffer[16];

unsigned char* ADPCM_data  [16];
unsigned char* ADPCM_buffer[16]; //Not used yet
int16_t  HEAD3_int16_adpcm [16][16]; //Coefs
int16_t* ADPC_hsamples_1   [16];
int16_t* ADPC_hsamples_2   [16];
//Include the file now
#include "brstm.h"
```
If you want to use the encoder then add this too:
```cpp
unsigned char* brstm_encoded_data;
unsigned long  brstm_encoded_data_size;
#include "brstm_encode.h"
```
Read a file:
```
brstm_read (

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

You can then read information about the BRSTM from the variables you declared earlier.

Close the file with brstm_close().

Decode a small part of the audio data into a buffer:
```
brstm_getbuffer (

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

std::ifstream with an open BRSTM file (std::ifstream&),

Console debug level (same as brstm_read) (signed int)

)
```
Returns error/warning codes like brstm_read. (unsigned char)

You can then read information about the BRSTM just like with brstm_read.

Get buffer:
```
brstm_fstream_getbuffer (

std::ifstream with an open BRSTM file (std::ifstream&),

Sample offset (unsigned long),

Amount of samples in the buffer
(can't be bigger than the amount of samples per block (HEAD1_blocks_samples)!)
(unsigned int)

)
```
You can then read the requested buffer from PCM_buffer[channel][sample offset (from the sample offset passed to brstm_fstream_getbuffer)].

Close the file with brstm_close() like with the normal memory mode.

### Encoder
Write your Int16 PCM audio data to PCM_samples[c] for each channel

Set audio and BRSTM details:
```cpp
HEAD1_codec         = BRSTM codec (only 2 is supported)
HEAD1_loop          = 0 or 1, loop flag
HEAD1_num_channels  = Number of audio channels
HEAD1_sample_rate   = Audio sample rate
HEAD1_loop_start    = Loop start point (sample number in a single channel)
HEAD1_total_samples = Total samples (in a single channel)

/*/--- TRACKS ---/*/

HEAD2_num_tracks  = Number of tracks
HEAD2_track_type  = Track description type
//You can set this to 0 or 1, but 1 is recommended.
//See the WiiBrew page about BRSTM files for more information.

HEAD2_track_num_channels[track] = Number of channels in the track. (1 or 2)
HEAD2_track_lchannel_id [track] = BRSTM Channel ID for the left channel of this track.
HEAD2_track_rchannel_id [track] = BRSTM Channel ID for the right channel of this track.
//Set to 0 if this is a mono track. It's recommended to always set the channel IDs in order
//(from 0) because some decoders probably don't care about this information.

HEAD2_track_volume [track] = Volume of the track. (0x00 to 0x7F)
HEAD2_track_panning[track] = Left to right panning of the track. (0x00 to 0x7F)
//Description type 1 only.
//Most decoders (including this one) probably don't care about this information.

// Example - standard stereo BRSTM

HEAD1_codec         = 2;
HEAD1_loop          = 1;
HEAD1_num_channels  = 2;
HEAD1_sample_rate   = 44100;
HEAD1_loop_start    = 41234;
HEAD1_total_samples = 500000;

PCM_samples[0] = new int16_t[500000];
PCM_samples[1] = new int16_t[500000];

HEAD2_num_tracks    = 1;
HEAD2_track_type    = 1;

HEAD2_track_num_channels[0] = 2;
HEAD2_track_lchannel_id [0] = 0;
HEAD2_track_rchannel_id [0] = 1;

HEAD2_track_volume      [0] = 0x7F;
HEAD2_track_panning     [0] = 0x40;

brstm_encode(/*

Console debug level:
-1 = Never output anything
 0 = Only output errors/warnings
 1 = Log encoding progress
(signed int),

Encode ADPCM flag:
 0 = Use the ADPCM data from ADPCM_data (requires coefs in HEAD3_int16_adpcm[ch][coef])
 1 = Encode PCM_samples into ADPCM

Returns error code (>127) or warning code (<128)
(see brstm_encode.h file for full list of error/warning codes). (unsigned char)

*/);

// Remember to free PCM_samples!

file.write(brstm_encoded_data,brstm_encoded_data_size);

```

### Editing headers
Coming soon.
