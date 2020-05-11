# Open Revolution
C++ BRSTM tools

Decode, encode, play and convert BRSTM files and other Nintendo audio formats.

Supports lossless conversion between supported formats and up to 8 tracks/16 channels.

## Audio formats

| Format       | Read                | Write               |
|:------------ |:-------------------:|:-------------------:|
| BRSTM        | Yes (only ADPCM)    | Yes (only ADPCM)    |
| BCSTM        | No                  | No                  |
| BFSTM        | No                  | No                  |
| BWAV         | Yes                 | Yes                 |

## Usage
Compile everything by running build.sh or another compiler with the correct options

Usage guides:
- [src/](https://github.com/Extrasklep/openrevolution/tree/master/src): Command line tools
- [src/rt_player](https://github.com/Extrasklep/openrevolution/tree/master/src/rt_player): RtAudio command line player
- [src/lib](https://github.com/Extrasklep/openrevolution/tree/master/src/lib) Library documentation

## Thanks to

- [WiiBrew](https://wiibrew.org/wiki/BRSTM_file): BRSTM file structure reference
- [kenrick95/nikku](https://github.com/kenrick95/nikku) and [BrawlLib](https://github.com/libertyernie/brawltools): DSPADPCM decoder reference
- [jackoalan/gc-dspadpcm-encode](https://github.com/jackoalan/gc-dspadpcm-encode): DSPADPCM encoder
- [gota7](https://gota7.github.io/Citric-Composer/specs/binaryWav.html): BWAV file structure reference
- [Gianmarco Gargiulo](https://gianmarco.ga/): Logo
- [RtAudio](https://github.com/thestk/rtaudio): RtAudio library

## Planned features

- Qt GUI
- Support for more file formats (BCSTM, BFSTM etc.)
- Multithreaded encoding

