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
#include<boost/variant.hpp>

class layoutForDouble
{
public:

    //QHBoxLayout* layout;
    QLabel* parameterName;
    QLineEdit* parameterValue;
    QLabel* parameterMin;
    QLabel* parameterMax;
    //QSpacerItem* spacer;
    layoutForDouble(QString&, double&, double&, double&);
    layoutForDouble();
    ~layoutForDouble();
    void addToGrid(QGridLayout*,int&);
    void setVisible(bool);
    //void operator <<  (QGridLayout*);
};

typedef boost::variant<layoutForDouble,double> layoutVariant;

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
    static std::map<int,layoutVariant> layoutMap_;
    static void populateLayoutMap(QGridLayout*)
;
private:


};

#endif // SQLINTERFACE_H

