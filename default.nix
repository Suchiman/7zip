# Nix package for the Qt port of the 7-Zip file manager.
#
#   nix-build            # builds ./result/bin/7zQ
#   nix-shell            # a shell with the toolchain, then ./build-qt.sh
#
{ pkgs ? import <nixpkgs> { } }:

let
  lib = pkgs.lib;

  # This tree is built in place, so the copy that goes to the store has to
  # leave the build output behind: object files, the qmake-generated Makefile,
  # moc output and the binaries. 7-Zip's own makefiles are all lowercase
  # ("makefile"), so dropping "Makefile" only removes the generated one.
  source = lib.cleanSourceWith {
    name = "7zQ-source";
    src = lib.cleanSource ./.;
    filter = path: type:
      let base = baseNameOf (toString path); in
      !(  (type == "directory" && base == "_o")
       || base == "Makefile"
       || base == ".qmake.stash"
       || base == "result"
       || base == "7zQ"
       || base == "7zz"
       || lib.hasSuffix ".o" base
       || lib.hasPrefix "moc_" base
       || lib.hasPrefix "result-" base
       );
  };
in
pkgs.stdenv.mkDerivation rec {
  pname = "7zQ";
  version = "26.03";

  src = source;

  nativeBuildInputs = [
    pkgs.qt6.qmake
    pkgs.qt6.wrapQtAppsHook
    pkgs.pkg-config
  ];

  buildInputs = [
    pkgs.qt6.qtbase
  ];

  # The project file lives in CPP/7zip/UI/Qt, not in the source root, and the
  # engine has to be built before it, so the build phase runs qmake itself.
  dontUseQmakeConfigure = true;

  buildPhase = ''
    runHook preBuild

    # 1. the platform independent 7-Zip engine, as one static library
    #    (same sources and flags the 7zz console program uses)
    make -C CPP/7zip/Bundles/Qt7z -f makefile.gcc lib -j$NIX_BUILD_CORES

    # 2. the Qt user interface
    cd CPP/7zip/UI/Qt
    qmake 7zQ.pro PREFIX=$out
    make -j$NIX_BUILD_CORES
    cd -

    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall

    install -Dm755 CPP/7zip/UI/Qt/7zQ $out/bin/7zQ

    mkdir -p $out/share/applications
    cp CPP/7zip/UI/Qt/7zQ.desktop $out/share/applications/7zQ.desktop

    runHook postInstall
  '';

  meta = with lib; {
    description = "Qt port of the 7-Zip file manager (7zFM)";
    longDescription = ''
      A native Qt 6 build of 7-Zip's graphical file manager. The user
      interface is a port of CPP/7zip/UI/FileManager and CPP/7zip/UI/GUI;
      everything below it -- the codecs, archive handlers, UI/Common and the
      UI/Agent "archive as a folder" layer -- is 7-Zip's own code, compiled
      from this source tree.
    '';
    homepage = "https://www.7-zip.org/";
    license = licenses.lgpl21Plus;
    platforms = platforms.linux;
    mainProgram = "7zQ";
  };
}
