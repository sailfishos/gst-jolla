TEMPLATE = app
TARGET = play
INCLUDEPATH += .

LIBS += -lgstnemointerfaces-0.10
CONFIG += link_pkgconfig
PKGCONFIG += egl gstreamer-0.10

HEADERS += openglwindow.h player.h
SOURCES += main.cpp openglwindow.cpp player.cpp
