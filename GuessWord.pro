# Where files can be found
INCLUDEPATH += src/
INCLUDEPATH += ../Shared/

# Where files go
OBJECTS_DIR = build/
MOC_DIR = build/

# Frameworks and compiler
TEMPLATE = app
QT += gui
QT += widgets
CONFIG += c++17
CONFIG += release
CONFIG += silent

# Don't allow deprecated versions of methods (before Qt 6.8)
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060800

# Resources
RESOURCES = GuessWord.qrc

# Shared classes
HEADERS += ../Shared/CallTracer.h
SOURCES += ../Shared/CallTracer.cpp
HEADERS += ../Shared/MessageLogger.h
SOURCES += ../Shared/MessageLogger.cpp
HEADERS += ../Shared/StringHelper.h
SOURCES += ../Shared/StringHelper.cpp

# Specific classes
HEADERS += src/AllWords.h
SOURCES += src/AllWords.cpp
HEADERS += src/Application.h
SOURCES += src/Application.cpp
HEADERS += src/Deploy.h
SOURCES += src/main.cpp
HEADERS += src/MainWindow.h
SOURCES += src/MainWindow.cpp
