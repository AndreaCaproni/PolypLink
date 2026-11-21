QT += quick qml network
CONFIG += c++17

# SDL2 per il controller (presente nel firmware). Il percorso viene dallo SDK.
isEmpty(MARVELL_ROOTFS) {
    MARVELL_ROOTFS = $$(MARVELL_ROOTFS)
}
INCLUDEPATH += $$MARVELL_ROOTFS/usr/include/SDL2
LIBS += -lSDL2

# ffmpeg minimale (solo decoder AAC + resampler), compilato per il dispositivo.
FFMPEG = $$PWD/third_party/ffmpeg
INCLUDEPATH += $$FFMPEG/include
LIBS += -L$$FFMPEG/lib -lavcodec -lswresample -lavutil

# API hardware del dispositivo
LIBS += -L$$MARVELL_ROOTFS/usr/lib -lSLVideo -lSLAudio
CONFIG -= app_bundle

TARGET = polyplink
TEMPLATE = app

DEFINES += QT_DEPRECATED_WARNINGS

SOURCES += src/main.cpp \
           src/inputpump.cpp \
           src/authnetwork.cpp \
           src/jellyfinclient.cpp \
           src/serverurl.cpp \
           src/redact.cpp \
           src/discovery.cpp \
           src/language.cpp \
           src/appcontroller.cpp \
           src/tsdemux.cpp \
           src/player.cpp

HEADERS += src/inputpump.h \
           src/authnetwork.h \
           src/jellyfinclient.h \
           src/serverurl.h \
           src/redact.h \
           src/discovery.h \
           src/language.h \
           src/appcontroller.h \
           src/tsdemux.h \
           src/player.h

RESOURCES += qml.qrc

# La UI e' compilata dentro il binario: nessun file sparso da copiare.
QMAKE_CXXFLAGS += -Wall -Wextra -Wno-unused-parameter

target.path = /usr/local/bin
INSTALLS += target
