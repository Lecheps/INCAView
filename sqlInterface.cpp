#include "sqlInterface.h"


layoutForDouble:: layoutForDouble(QString& name,double& value,double& min, double& max)
{
    //layout = new QHBoxLayout;
    parameterName = new QLabel;
    parameterValue = new QLineEdit;
    parameterMin = new QLabel;
    parameterMax = new QLabel;
    parameterName->setText(name);
    parameterValue->setText(QString::number(value, 'f', 5));
    parameterMin->setText(QString::number(min, 'f', 5));
    parameterMax->setText(QString::number(max, 'f', 5));

}
layoutForDouble::layoutForDouble(){}
layoutForDouble::~layoutForDouble(){}
void layoutForDouble::addToGrid(QGridLayout* grid, int &rowNumber)
{
    grid->addWidget(parameterName,rowNumber,0,1,1);
    grid->addWidget(parameterValue,rowNumber,1,1,1);
    grid->addWidget(parameterMin,rowNumber,2,1,1);
    grid->addWidget(parameterMax,rowNumber,3,1,1);
}

void layoutForDouble::setVisible(bool isVisible)
{
    parameterName->setVisible(isVisible);
    parameterValue->setVisible(isVisible);
    parameterMin->setVisible(isVisible);
    parameterMax->setVisible(isVisible);
}

sqlInterface::sqlInterface()
{
   db_ = QSqlDatabase::addDatabase("QSQLITE");
}

sqlInterface::~sqlInterface()
{

}


bool sqlInterface::connectToDB()
{
    db_.setDatabaseName(pathToDB_);
    return db_.open();
}

QSqlDatabase sqlInterface::db_;
QSqlQueryModel sqlInterface::queryModel_;
QString sqlInterface::pathToDB_;
std::map<int,layoutVariant> sqlInterface::layoutMap_;

void sqlInterface::populateLayoutMap(QGridLayout* grid)
{
    connectToDB();
    QSqlQuery query;
    query.prepare(   "SELECT "
                        "ParameterStructure.name,ParameterStructure.type,ParameterValues.* "
                    "FROM "
                    "    ParameterStructure INNER JOIN ParameterValues "
                    "    ON ParameterStructure.ID = ParameterValues.ID "
                    "WHERE "
                    "    ParameterStructure.type = 'DOUBLE'; "
                );
    query.exec();
    int cnt = 0;
    while (query.next())
    {
        int ID = query.value(2).toInt();
        QString name = query.value(0).toString();
        double value = query.value(5).toDouble();
        double min = query.value(3).toDouble();
        double max = query.value(4).toDouble();
        layoutForDouble dummy = layoutForDouble(name,value,min,max);
        dummy.addToGrid(grid,cnt);
        dummy.setVisible(false);
        layoutMap_[ID] = dummy;
        ++cnt;
    }
   db_.close();
}
