#ifndef SQLINTERFACE_H
#define SQLINTERFACE_H

#include"sqlite3.h"
#include<QtCore>
#include<QSqlDatabase>
#include<QSqlQuery>
#include<QSqlQueryModel>
#include<QHBoxLayout>
#include<QLineEdit>
#include<QSpacerItem>
#include<QLabel>
#include "parameter.h"

/*
//TODO: make more possible types.
enum class parameterType
{
    DOUBLE,
    UINT,
    BOOL,
    PTIME,
};

inline parameterType parseParameterType(QString& TypeStr)
{
    if(TypeStr == "DOUBLE") return parameterType::DOUBLE;
    if(TypeStr == "UINT") return parameterType::UINT;
    if(TypeStr == "BOOL") return parameterType::BOOL;
    if(TypeStr == "PTIME") return parameterType::PTIME;

    qFatal(("Unknown type " + TypeStr + " for parameter values.").toLatin1().data());
}

union parameterValue
{
    double valueDouble;
    uint64_t valueUint; // The database may not support this precision, but there is no reason to not use as many bits as possible here.
    int64_t valueBool;
    int64_t valueTime; // In seconds since 1-1-1970, i.e. posix time.
};*/

class layoutForParameter
{
public:

    //QHBoxLayout* layout;
    QLabel* parameterNameView;
    QLineEdit* parameterValueView;
    QLabel* parameterMinView;
    QLabel* parameterMaxView;
    parameterValue value;
    parameterValue min;
    parameterValue max;
    bool valueIsValidAndInRange;

    //QSpacerItem* spacer;
    layoutForParameter(QString&, parameterValue&, parameterValue&, parameterValue&, int);
    layoutForParameter();
    ~layoutForParameter();
    void addToGrid(QGridLayout*,int);
    void setVisible(bool);

    //void operator <<  (QGridLayout*);
};

class sqlInterface
{
public:
    sqlInterface();
    virtual  ~sqlInterface();

    static void valueChangedReceiveMessage(const QString&, int);
protected:
    static bool connectToDB();
    static QSqlDatabase db_;
    static QSqlQueryModel queryModel_;
    static bool dbIsLoaded_;
    static QString pathToDB_;
    static std::map<int,layoutForParameter> layoutMap_;
    static void populateLayoutMap(QGridLayout*);

private:


};

#endif // SQLINTERFACE_H

