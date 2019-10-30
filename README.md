# brstm
C++ BRSTM reader and player

Should work with up to 8 tracks and 16 channels.
Only ADPCM codec is supported.

## How to use the converter
Compile the main file in the main directory

Usage:
./brstm input.brstm -o output.raw

The raw output file can be imported into a program like Audacity as little endian signed 16-bit PCM.

### RtAudio player
Compile the player with the build script in rt_player (or with another compiler.)


**Player controls**

Left and right arrow keys: Seek 1 second

Up and down arrow keys: Switch track (for multi track BRSTMs)

Space: Pause
