# 7zQ.pro -- Qt port of the 7-Zip file manager
#
# The 7-Zip engine (codecs, archive handlers, UI/Common, UI/Agent) is built
# separately into a static library by ../../Bundles/Qt7z/makefile.gcc, with
# the same compiler flags the 7zz console program uses. This project builds
# only the Qt user interface and links that library.
#
#   make -C ../../Bundles/Qt7z -f makefile.gcc lib
#   qmake6 && make

QT          += widgets
CONFIG      += c++17
CONFIG      -= app_bundle
TARGET       = 7zQ
TEMPLATE     = app

DEFINES     += _REENTRANT _FILE_OFFSET_BITS=64 _LARGEFILE_SOURCE NDEBUG

Z7_LIB       = $$PWD/../../Bundles/Qt7z/_o/lib7z.a

# --whole-archive is required, not an optimisation choice: every archive
# format and codec registers itself from a static constructor in its own
# *Register.o, and nothing references those objects by symbol. Without it the
# linker drops them and CCodecs::Load() finds no formats at all.
LIBS        += -Wl,--whole-archive $$Z7_LIB -Wl,--no-whole-archive -lpthread -ldl
PRE_TARGETDEPS += $$Z7_LIB

# 7-Zip's own sources use relative includes, so no extra include paths are
# needed; only the Qt ones, which qmake adds.

HEADERS += \
    StdAfx.h \
    Z7Qt.h \
    PropertyNameTable.h \
    Codecs.h \
    IconUtils.h \
    FsFolder.h \
    RootFolder.h \
    PanelModel.h \
    Panel.h \
    MainWindow.h \
    Callbacks.h \
    Operations.h \
    ProgressDialog.h \
    OverwriteDialog.h \
    PasswordDialog.h \
    ExtractDialog.h \
    CompressDialog.h \
    BenchmarkDialog.h \
    OptionsDialog.h \
    SmallDialogs.h

SOURCES += \
    main.cpp \
    Z7Qt.cpp \
    Codecs.cpp \
    IconUtils.cpp \
    FsFolder.cpp \
    RootFolder.cpp \
    PanelModel.cpp \
    Panel.cpp \
    MainWindow.cpp \
    Callbacks.cpp \
    Operations.cpp \
    ProgressDialog.cpp \
    OverwriteDialog.cpp \
    PasswordDialog.cpp \
    ExtractDialog.cpp \
    CompressDialog.cpp \
    BenchmarkDialog.cpp \
    OptionsDialog.cpp \
    SmallDialogs.cpp \
    WorkDirSettings.cpp

target.path = $$PREFIX/bin
INSTALLS += target
