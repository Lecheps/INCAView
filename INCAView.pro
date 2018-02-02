#-------------------------------------------------
#
# Project created by QtCreator 2017-10-15T11:41:03
#
#-------------------------------------------------
QMAKE_CXXFLAGS += -std=c++11

QT       += core gui sql

greaterThan(QT_MAJOR_VERSION, 5): QT += widgets printsupport

TARGET = INCAView
TEMPLATE = app


SOURCES += main.cpp\
        mainwindow.cpp \
    sqlInterface.cpp \
    treeitem.cpp \
    treemodel.cpp \
    /home/jose-luis/Documents/qcustomplot/qcustomplot.cpp

HEADERS  += mainwindow.h \
    sqlInterface.h \
    treemodel.h \
    treeitem.h \
    /home/jose-luis/Documents/qcustomplot/qcustomplot.h

FORMS    += mainwindow.ui


unix:!macx: LIBS += -L$$PWD/../Documents/INCA/libraries/sqlite3/libs/ -lsqlite3

INCLUDEPATH += $$PWD/../Documents/INCA/libraries/sqlite3\
                /usr/local/boost_1_65_1
DEPENDPATH += $$PWD/../Documents/INCA/libraries/sqlite3


unix:!macx: PRE_TARGETDEPS += $$PWD/../Documents/INCA/libraries/sqlite3/libs/libsqlite3.a
