TEMPLATE = app
TARGET = play
INCLUDEPATH += .

CONFIG += link_pkgconfig
PKGCONFIG += egl gstreamer-0.10 meego-gstreamer-interfaces-0.10

HEADERS += openglwindow.h player.h
SOURCES += main.cpp openglwindow.cpp player.cpp
