# Revolution
C++ BRSTM tools

Decode, encode, play and convert BRSTM files and other Nintendo audio formats.

Supports lossless conversion between supported formats and up to 8 tracks/16 channels.

## Audio formats

Reading: BRSTM, BWAV
Writing: BRSTM
(put a proper table here later)

## Usage
Compile everything by running build.sh or another compiler with the correct options

Usage guides:
- [src/](https://github.com/Extrasklep/revolution/tree/master/src): Command line tools
- [src/rt_player](https://github.com/Extrasklep/revolution/tree/master/src/rt_player): RtAudio command line player
- [src/lib](https://github.com/Extrasklep/revolution/tree/master/src/lib) Library documentation

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

