# brstm
C++ BRSTM reader and player

Should work with up to 8 tracks and 16 channels.
Only ADPCM codec is supported (for now).

## Converter program
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

## brstm.h file usage
Declare these variables in your code before including the brstm.h file:
```cpp
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
unsigned long written_samples=0;
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
false = Don't decode the audio data (Don't read the DATA chunk)
true  = Decode the audio data into PCM_samples[channel][sample offset]
(bool)

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

Amount of samples in the buffer (can't be bigger than the amount of samples per block (HEAD1_blocks_samples)!)
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

Amount of samples in the buffer (can't be bigger than the amount of samples per block (HEAD1_blocks_samples)!)
(unsigned int)

)
```
You can then read the requested buffer from PCM_buffer[channel][sample offset (from the sample offset passed to brstm_fstream_getbuffer)].

Close the file with brstm_close() like with the normal memory mode.
