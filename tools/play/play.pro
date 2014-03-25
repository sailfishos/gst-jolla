TEMPLATE = app
TARGET = play
INCLUDEPATH += .

CONFIG += link_pkgconfig
PKGCONFIG += egl gstreamer-1.0 nemo-gstreamer-interfaces-1.0

HEADERS += openglwindow.h player.h
SOURCES += main.cpp openglwindow.cpp player.cpp
