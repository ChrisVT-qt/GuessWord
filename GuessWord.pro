# Where files can be found
INCLUDEPATH += src/
INCLUDEPATH += shared/

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

# macOS stuff
# Build for macOS 14
# QMAKE_MACOSX_DEPLOYMENT_TARGET = 14.0
# Build for either Intel and Apple Silicon
# QMAKE_APPLE_DEVICE_ARCHS = arm64
# CMAKE_OSX_ARCHITECTURES = arm64

# Don't allow deprecated versions of methods (before Qt 6.8)
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060800

# Resources
RESOURCES = GuessWord.qrc

# Shared classes
HEADERS += shared/CallTracer.h
SOURCES += shared/CallTracer.cpp
HEADERS += shared/MessageLogger.h
SOURCES += shared/MessageLogger.cpp
HEADERS += shared/StringHelper.h
SOURCES += shared/StringHelper.cpp

# Specific classes
HEADERS += src/AllWords.h
SOURCES += src/AllWords.cpp
HEADERS += src/Application.h
SOURCES += src/Application.cpp
HEADERS += src/Deploy.h
SOURCES += src/main.cpp
HEADERS += src/MainWindow.h
SOURCES += src/MainWindow.cpp
