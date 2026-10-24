QT += core gui qml quick quickcontrols2 dbus

CONFIG += c++17 release
TARGET = joist
TEMPLATE = app

HEADERS += \
    src/backend.h \
    src/cli.h \
    src/filepicker.h \
    src/labels.h \
    src/notifier.h \
    src/pdfexport.h \
    src/portalfilepicker.h \
    src/project.h \
    src/theme.h \
    src/translator.h

SOURCES += \
    src/main.cpp \
    src/backend.cpp \
    src/cli.cpp \
    src/labels.cpp \
    src/notifier.cpp \
    src/pdfexport.cpp \
    src/portalfilepicker.cpp \
    src/project.cpp \
    src/theme.cpp \
    src/translator.cpp

RESOURCES += src/resources.qrc
