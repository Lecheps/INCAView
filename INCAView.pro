#-------------------------------------------------
#
# Project created by QtCreator 2017-10-15T11:41:03
#
#-------------------------------------------------
QMAKE_CXXFLAGS += -std=c++11

QT       += core gui sql widgets printsupport

#greaterThan(QT_MAJOR_VERSION, 5): QT += widgets printsupport

TARGET = INCAView
TEMPLATE = app


SOURCES += main.cpp\
        mainwindow.cpp \
    sqlInterface.cpp \
    treeitem.cpp \
    treemodel.cpp \
    qcustomplot.cpp \
    parameter.cpp \
    parametermodel.cpp \
    lineeditdelegate.cpp \
    sshInterface.cpp

HEADERS  += mainwindow.h \
    sqlInterface.h \
    treemodel.h \
    treeitem.h \
    qcustomplot.h \
    parameter.h \
    parametermodel.h \
    lineeditdelegate.h \
    sshInterface.h \
    sqlhandler/parameterserialization.h

FORMS    += mainwindow.ui


LIBS += -L$$PWD/lib/ -lssh\
# LIBS += -L$$PWD/libs/ -lpython36\
        #-L$$PWD/../INCA/INCA/libraries/sqlite3/libs/ -lsqlite3\


INCLUDEPATH += $$PWD/include
               #$$PWD/../INCA/INCA/libraries/sqlite3\
               #$$PWD/../INCA/INCA/libraries/boost/include\

DEPENDPATH += $$PWD/../INCA/INCA/libraries/sqlite3


PRE_TARGETDEPS += $$PWD/lib/libssh.dll.a
#$$PWD/libs/libpython36.a
                 #$$PWD/../INCA/INCA/libraries/sqlite3/libs/libsqlite3.a\

