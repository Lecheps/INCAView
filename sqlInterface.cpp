#include "sqlInterface.h"


layoutForParameter::layoutForParameter(QString& name, parameterValue& value, parameterValue& min, parameterValue& max, int dbID)
{
    //layout = new QHBoxLayout;
    parameterNameView = new QLabel;
    parameterValueView = new QLineEdit;
    parameterMinView = new QLabel;
    parameterMaxView = new QLabel;
    parameterNameView->setText(name);

    this->min = min; // We store the values for these, not just their text fields, so that it is efficient to check when edited values fall within range.
    this->max = max;
    this->value = value;

    this->valueIsValidAndInRange = value.isValid() && value.isInRange(min, max);

    //NOTE: Had to make sqlInterface::valueChangedReceiveMessage public for this to work, so this is really just global functionality. Not generally a good solution, but it works atm.
    QObject::connect(parameterValueView, &QLineEdit::textEdited,
                     [dbID](const QString& newValue){sqlInterface::valueChangedReceiveMessage(newValue, dbID);});

    int precision = 5;
    parameterValueView->setText(value.getValueString(precision));
    parameterMinView->setText(min.getValueString(precision));
    parameterMaxView->setText(max.getValueString(precision));
}
layoutForParameter::layoutForParameter(){}
layoutForParameter::~layoutForParameter(){}
void layoutForParameter::addToGrid(QGridLayout* grid, int rowNumber)
{
    grid->addWidget(parameterNameView,rowNumber,0,1,1);
    grid->addWidget(parameterValueView,rowNumber,1,1,1);
    grid->addWidget(parameterMinView,rowNumber,2,1,1);
    grid->addWidget(parameterMaxView,rowNumber,3,1,1);
}

void layoutForParameter::setVisible(bool isVisible)
{
    parameterNameView->setVisible(isVisible);
    parameterValueView->setVisible(isVisible);
    parameterMinView->setVisible(isVisible);
    parameterMaxView->setVisible(isVisible);
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
std::map<int,layoutForParameter> sqlInterface::layoutMap_;
bool sqlInterface::dbIsLoaded_;

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
        parameterValue value(query.value(5).toString(), query.value(1).toString());
        parameterValue min(query.value(3).toString(), query.value(1).toString());;
        parameterValue max(query.value(4).toString(), query.value(1).toString());;

        layoutForParameter dummy = layoutForParameter(name,value,min,max,ID);
        dummy.addToGrid(grid, cnt+1);
        dummy.setVisible(false);
        layoutMap_[ID] = dummy;
        ++cnt;
    }
    db_.close();
}

void sqlInterface::valueChangedReceiveMessage(const QString& newValue, int dbID)
{
    if(layoutMap_.count(dbID) != 0)
    {
        layoutForParameter& layout = layoutMap_[dbID];

        layout.valueIsValidAndInRange = layout.value.setValue(newValue) && layout.value.isInRange(layout.min, layout.max);

        if(layout.valueIsValidAndInRange)
        {
            layout.parameterValueView->setStyleSheet("QLineEdit { color : black; }");

            //update db
            connectToDB();

            QSqlQuery query;
            query.prepare( "UPDATE "
                           " ParameterValues "
                           " SET "
                           " value = '" + newValue +
                            "' WHERE "
                            " ID = " + QString::number(dbID) + ";");
            query.exec();

            db_.close();
        }
        else
        {
            layout.parameterValueView->setStyleSheet("QLineEdit { color : red; }");
        }
    }
}
