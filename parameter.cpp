#include "parameter.h"
#include "sqlInterface.h"
#include <QDateTime>

parameterValue::parameterValue(const QString & valueStr, const QString & typeStr)
{
    type = parseParameterType(typeStr);
    setValue(valueStr);
}

parameterValue::parameterValue()
{
    type = UNKNOWN;
    value.Uint = 0;
}

parameterValue::Type parameterValue::parseParameterType(const QString& typeStr)
{
    if(typeStr == "DOUBLE") return DOUBLE;
    if(typeStr == "UINT") return UINT;
    if(typeStr == "BOOL") return BOOL;
    if(typeStr == "PTIME") return PTIME;

    return UNKNOWN;
}

bool parameterValue::setValue(const QString & valueStr)
{
    switch(type)
    {
    case DOUBLE:
    {
        value.Double = valueStr.toDouble(&valid);
    } break;

    case UINT:
    {
        value.Uint = valueStr.toUInt(&valid);
    } break;

    case BOOL:
    {
        //TODO: Not yet implemented!
    } break;

    case PTIME:
    {
        //TODO: Not yet implemented!
    } break;

    case UNKNOWN:
    {
        //Do nothing
    } break;

    }

    return valid;
}

QString parameterValue::getValueString(int precision)
{
    switch(type)
    {
    case DOUBLE:
    {
        return QString::number(value.Double, 'g', precision);
    } break;

    case UINT:
    {
        return QString::number(value.Uint);
    } break;

    case BOOL:
    {
        return value.Bool ? "true" : "false";
    } break;

    case PTIME:
    {
        QDateTime dt = QDateTime::fromSecsSinceEpoch(value.Time);
        return dt.toString("d. MMMM\nyyyy");
    } break;

    case UNKNOWN:
    {
        return "unknown type";
    } break;

    }
    return "";
}

bool parameterValue::isInRange(const parameterValue& min, const parameterValue& max)
{
    switch(type)
    {
    case DOUBLE:
    {
        return (value.Double >= min.value.Double) && (value.Double <= max.value.Double);
    } break;

    case UINT:
    {
        return (value.Uint >= min.value.Uint) && (value.Uint <= max.value.Uint);
    } break;

    case BOOL:
    {
        return true;
    } break;

    case PTIME:
    {
        return (value.Time >= min.value.Time ) && (value.Time  <= max.value.Time );
    } break;

    case UNKNOWN:
    {
        return true; //If the value is of unknown type, we should not complain about it being out of range
    } break;

    }

    return false;
}

// LAYOUTFORPARAMETER

layoutForParameter::layoutForParameter(QString& name, parameterValue& value, parameterValue& min, parameterValue& max)
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

    QObject::connect(parameterValueView, &QLineEdit::textEdited, this, &layoutForParameter::valueEditedReceiveMessage);

    int precision = 5;
    parameterValueView->setText(value.getValueString(precision));
    parameterMinView->setText(min.getValueString(precision));
    parameterMaxView->setText(max.getValueString(precision));
}

layoutForParameter::~layoutForParameter()
{
    delete parameterNameView;
    delete parameterValueView;
    delete parameterMinView;
    delete parameterMaxView;
}

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

void layoutForParameter::valueEditedReceiveMessage(const QString& newValue)
{
    valueIsValidAndInRange = value.setValue(newValue) && value.isInRange(min, max);

    if(valueIsValidAndInRange)
    {
        parameterValueView->setStyleSheet("QLineEdit { color : black; }");

        emit signalValueWasEdited(newValue);
    }
    else
    {
        parameterValueView->setStyleSheet("QLineEdit { color : red; }");
    }
}

