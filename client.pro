QT += quick qml network
CONFIG += c++17

# SDL2 for the controller (present in the firmware). The path comes from the SDK.
isEmpty(MARVELL_ROOTFS) {
    MARVELL_ROOTFS = $$(MARVELL_ROOTFS)
}
INCLUDEPATH += $$MARVELL_ROOTFS/usr/include/SDL2
LIBS += -lSDL2

# minimal ffmpeg (AAC decoder + resampler only), built for the device.
FFMPEG = $$PWD/third_party/ffmpeg
INCLUDEPATH += $$FFMPEG/include
LIBS += -L$$FFMPEG/lib -lavcodec -lswresample -lavutil

# device hardware API
LIBS += -L$$MARVELL_ROOTFS/usr/lib -lSLVideo -lSLAudio
CONFIG -= app_bundle

TARGET = polyplink
TEMPLATE = app

DEFINES += QT_DEPRECATED_WARNINGS

SOURCES += src/main.cpp \
           src/inputpump.cpp \
           src/authnetwork.cpp \
           src/authcontroller.cpp \
           src/librarycontroller.cpp \
           src/settingscontroller.cpp \
           src/playbackcontroller.cpp \
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
           src/authcontroller.h \
           src/librarycontroller.h \
           src/settingscontroller.h \
           src/playbackcontroller.h \
           src/jellyfinclient.h \
           src/serverurl.h \
           src/redact.h \
           src/discovery.h \
           src/discoveryaddress.h \
           src/language.h \
           src/appcontroller.h \
           src/tsdemux.h \
           src/player.h

RESOURCES += qml.qrc

# The UI is compiled into the binary: no loose files to copy.
QMAKE_CXXFLAGS += -Wall -Wextra -Wno-unused-parameter

target.path = /usr/local/bin
INSTALLS += target
