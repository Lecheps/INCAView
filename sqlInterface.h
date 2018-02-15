#ifndef SQLINTERFACE_H
#define SQLINTERFACE_H

#include"sqlite3.h"
#include<QtCore>
#include "parameter.h"

class sqlInterface
{
public:
    sqlInterface();
    virtual  ~sqlInterface();

    static bool connectDB();
    static void disconnectDB();
    static void setDBPath(const QString&);
    static bool pathToDBIsSet() { return dbIsSet_; }
    //static void copyDBToLocation(const QString&);

private:
    static QSqlDatabase db_;
    static QSqlQueryModel queryModel_;
    static bool dbIsSet_;
    static QString pathToDB_;

};

#endif // SQLINTERFACE_H

