#include "sqlInterface.h"


layoutForValue::layoutForValue(QString& name, valueStorage& value, valueStorage& min, valueStorage& max, valueType type)
{
    //layout = new QHBoxLayout;
    parameterName = new QLabel;
    parameterValue = new QLineEdit;
    parameterMin = new QLabel;
    parameterMax = new QLabel;
    parameterName->setText(name);

    switch(type)
    {
    case valueType::DOUBLE:
    {
        parameterValue->setText(QString::number(value.valueDouble, 'g', 5));
        parameterMin->setText(QString::number(min.valueDouble, 'g', 5));
        parameterMax->setText(QString::number(max.valueDouble, 'g', 5));
    } break;

    case valueType::UINT:
    {
        parameterValue->setText(QString::number(value.valueUint));
        parameterMin->setText(QString::number(min.valueUint));
        parameterMax->setText(QString::number(max.valueUint));
    } break;

    case valueType::BOOL:
    {
        parameterValue->setText(value.valueBool ? "true" : false);
        parameterMin->setText("");
        parameterMax->setText("");
    } break;

    case valueType::PTIME:
    {
        QDateTime dt = QDateTime::fromSecsSinceEpoch(value.valueTime);
        parameterValue->setText(dt.toString("d. MMMM\nyyyy"));
        parameterMin->setText("");
        parameterMax->setText("");
    } break;

    default:
    {
        qFatal("Received a value type that is not handled!");
    }break;
    }

}
layoutForValue::layoutForValue(){}
layoutForValue::~layoutForValue(){}
void layoutForValue::addToGrid(QGridLayout* grid, int rowNumber)
{
    grid->addWidget(parameterName,rowNumber,0,1,1);
    grid->addWidget(parameterValue,rowNumber,1,1,1);
    grid->addWidget(parameterMin,rowNumber,2,1,1);
    grid->addWidget(parameterMax,rowNumber,3,1,1);
}

void layoutForValue::setVisible(bool isVisible)
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
std::map<int,layoutForValue> sqlInterface::layoutMap_;

void sqlInterface::populateLayoutMap(QGridLayout* grid)
{
    //NOTE: Rudimentary headers for the parameter view. They currently don't look in style with the rest of the views.
    QStringList headerLabels;
    headerLabels << "Name" << "Value" << "Min" << "Max";
    int col = 0;
    for (auto& label : headerLabels)
    {
        QLabel *qlabel = new QLabel(label);
        qlabel->setMaximumHeight(20);
        qlabel->setStyleSheet("background-color: silver; color: black; border: 1px solid #6c6c6c;");
        grid->addWidget(qlabel, 0, col++, 1, 1);
    }


    connectToDB();
    QSqlQuery query;
    query.prepare(   "SELECT "
                        "ParameterStructure.name,ParameterStructure.type,ParameterValues.* "
                    "FROM "
                    "    ParameterStructure INNER JOIN ParameterValues "
                    "    ON ParameterStructure.ID = ParameterValues.ID; "
                );
    query.exec();
    int cnt = 0;
    while (query.next())
    {
        int ID = query.value(2).toInt();
        QString name = query.value(0).toString();
        QString typeStr = query.value(1).toString();
        valueType type = getValueType(typeStr);
        valueStorage value;
        valueStorage min;
        valueStorage max;

        switch(type)
        {
        case valueType::DOUBLE:
        {
            value.valueDouble = query.value(5).toDouble();
            min.valueDouble = query.value(3).toDouble();
            max.valueDouble = query.value(4).toDouble();
        }break;

        case valueType::UINT:
        {
            value.valueUint = query.value(5).toUInt();
            min.valueUint = query.value(3).toUInt();
            max.valueUint = query.value(4).toUInt();
        }break;

        case valueType::BOOL:
        {
            //NOTE: this is untested. How are bools represented in the database?
            value.valueBool = query.value(5).toBool();
        }break;

        case valueType::PTIME:
        {
            //NOTE: this is untested. How is ptime represented in the database?
            value.valueTime = query.value(5).toInt();
        }break;

        default:
        {
            qFatal("Received an unknown value type!");
        }break;
        }

        layoutForValue dummy = layoutForValue(name,value,min,max,type);
        dummy.addToGrid(grid, cnt+1);
        dummy.setVisible(false);
        layoutMap_[ID] = dummy;
        ++cnt;
    }
    db_.close();
}
