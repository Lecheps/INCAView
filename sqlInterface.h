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

//TODO: make more possible types.
enum class valueType
{
    DOUBLE,
    UINT,
};

inline valueType getValueType(QString& TypeStr)
{
    if(TypeStr == "DOUBLE") return valueType::DOUBLE;
    if(TypeStr == "UINT") return valueType::UINT;

    qFatal(("Unknown type " + TypeStr + " for parameter values.").toLatin1().data());
}

union valueStorage
{
    double valueDouble;
    uint64_t valueUint;
};

class layoutForValue
{
public:

    //QHBoxLayout* layout;
    QLabel* parameterName;
    QLineEdit* parameterValue;
    QLabel* parameterMin;
    QLabel* parameterMax;
    //QSpacerItem* spacer;
    layoutForValue(QString&, valueStorage&, valueStorage&, valueStorage&, valueType);
    layoutForValue();
    ~layoutForValue();
    void addToGrid(QGridLayout*,int);
    void setVisible(bool);
    //void operator <<  (QGridLayout*);
};

class sqlInterface
{
public:
    sqlInterface();
    virtual  ~sqlInterface();

protected:
    static bool connectToDB();
    static QSqlDatabase db_;
    static QSqlQueryModel queryModel_;
    static QString pathToDB_;
    static std::map<int,layoutForValue> layoutMap_;
    static void populateLayoutMap(QGridLayout*)
;
private:


};

#endif // SQLINTERFACE_H

