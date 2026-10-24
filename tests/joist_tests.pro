QT += core gui quick quickcontrols2 testlib dbus
CONFIG += c++17 testcase
TARGET = joist_tests
TEMPLATE = app

INCLUDEPATH += ../src

HEADERS += ../src/backend.h ../src/filepicker.h ../src/labels.h ../src/notifier.h ../src/pdfexport.h \
           ../src/project.h ../src/theme.h ../src/translator.h
SOURCES += joist_tests.cpp ../src/backend.cpp ../src/labels.cpp ../src/notifier.cpp ../src/pdfexport.cpp \
           ../src/project.cpp ../src/theme.cpp ../src/translator.cpp
DEFINES += SRCDIR=\\\"$$PWD/../src\\\"
