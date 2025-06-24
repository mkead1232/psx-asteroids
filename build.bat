make clean
make
mkpsxiso.exe -o ASTEROIDS.iso cuesheet.xml
move *.o build
move *.ps-exe build
move *.dep build
move *.elf build
move *.iso build
move *.cue build