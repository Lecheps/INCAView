#ifndef SQLINTERFACE_H
#define SQLINTERFACE_H

//#include "sqlite3.h"
#include <QtCore>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlQueryModel>

//TODO: this class is sort of useless at the moment, and could easily be rolled into MainWindow

class sqlInterface
{
public:
    sqlInterface();
    virtual  ~sqlInterface();

    static bool connectDB();
    static void disconnectDB();
    static void setDBPath(const QString&);
    static bool pathToDBIsSet() { return dbIsSet_; }

private:
    static QSqlDatabase db_;
    static QSqlQueryModel queryModel_;
    static bool dbIsSet_;
    static QString pathToDB_;

};

#endif // SQLINTERFACE_H

