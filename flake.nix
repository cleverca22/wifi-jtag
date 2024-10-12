{
  inputs = {
    nixpkgs.url = "github:cleverca22/nixpkgs/arm-multilib";
    nixpkgs2.url = "github:nixos/nixpkgs/nixos-unstable";
  };
  outputs = { self, nixpkgs, nixpkgs2 }:
  let
    picotool = nixpkgs2.legacyPackages.x86_64-linux.picotool;
    pico-sdk = nixpkgs.legacyPackages.x86_64-linux.fetchFromGitHub {
      owner = "raspberrypi";
      repo = "pico-sdk";
      rev = "efe2103f9b28458a1615ff096054479743ade236";
      sha256 = "sha256-fVSpBVmjeP5pwkSPhhSCfBaEr/FEtA82mQOe/cHFh0A=";
      fetchSubmodules = true;
    };
  in {
    packages.x86_64-linux = {
      default = nixpkgs.legacyPackages.x86_64-linux.callPackage ./. { inherit pico-sdk picotool; };
    };
  };
}
