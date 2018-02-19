#include "sqlInterface.h"

QSqlDatabase sqlInterface::db_;
QSqlQueryModel sqlInterface::queryModel_;
QString sqlInterface::pathToDB_;
bool sqlInterface::dbIsSet_ = false;

sqlInterface::sqlInterface()
{
   db_ = QSqlDatabase::addDatabase("QSQLITE");
}

sqlInterface::~sqlInterface()
{

}


bool sqlInterface::connectDB()
{
    db_.setDatabaseName(pathToDB_);
    return db_.open();
}

void sqlInterface::disconnectDB()
{
    db_.close();
}

void sqlInterface::setDBPath(const QString& path)
{
    pathToDB_ = path;
    dbIsSet_ = true;
}
