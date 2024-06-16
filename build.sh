echo "Building converter..."
g++ -O2 -std=c++0x src/converter.cpp -o brstm_converter -Wall
echo "Building player..."
if [[ -f /usr/lib/librtaudio.so.7 ]]; then echo "rtaudio 6 detected..." && g++ -O2 src/rt_player/main_rtaudio6.cpp -o brstm_rt -lrtaudio -lpthread -Wall
elif [[ -f /usr/lib/librtaudio.so.6 ]]; then  echo "rtaudio 5 detected..." g++ -O2 src/rt_player/main.cpp -o brstm_rt -lrtaudio -lpthread -Wall
else echo -e "\033[31mRTAUDIO WAS NOT DETECTED - UNABLE TO BUILD PLAYER\033[0m"
fi
#echo "Building decoder..."
#g++ -O2 src/decoder.cpp -o brstm_decoder
#echo "Building encoder..."
#g++ -O2 -std=c++0x src/encoder.cpp -o brstm_encoder
#echo "Building reencoder..."
#g++ -O2 -std=c++0x src/reencoder.cpp -o brstm_reencoder
#echo "Building rebuilder..."
#g++ -O2 -std=c++0x src/rebuilder.cpp -o brstm_rebuilder
