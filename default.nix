{ cmake, stdenv, python3, python3Packages, pico-sdk, picotool, gcc, pkgsCross, nodejs }:

stdenv.mkDerivation {
  name = "wifi-jtag";
  src = ./src;
  nativeBuildInputs = [ cmake picotool python3 pkgsCross.arm-embedded.stdenv.cc nodejs ];
  PICO_SDK_PATH = pico-sdk;
  preConfigure = ''
    echo cc is $CC
    type cmake
  '';
  configurePhase = ''
    export NIX_LDFLAGS_arm_none_eabi="-L${pkgsCross.arm-embedded.stdenv.cc.libc}/arm-none-eabi/lib/thumb/v6-m/nofp $NIX_LDFLAGS_arm_none_eabi"
    mkdir build
    cd build
    cmake .. -DPICO_BOARD=pico_w -DWIFI_SSID=dlink-1234 -DWIFI_PASSWORD=pw
  '';
  enableParallelBuilding = true;
  installPhase = ''
    mkdir $out/
    cp *.elf *.dis *.map *.uf2 *.bin $out/
  '';
}
