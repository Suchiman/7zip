# Development shell for the Qt port: gcc, make and Qt 6.
#
#   nix-shell
#   ./build-qt.sh
#   ./CPP/7zip/UI/Qt/7zQ
#
{ pkgs ? import <nixpkgs> { } }:

pkgs.mkShell {
  packages = [
    pkgs.gcc
    pkgs.gnumake
    pkgs.binutils
    pkgs.qt6.qtbase
    pkgs.qt6.qmake
    pkgs.pkg-config
  ];

  shellHook = ''
    echo "7zQ dev shell -- run ./build-qt.sh"
  '';
}
