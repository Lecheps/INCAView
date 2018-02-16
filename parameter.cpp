#include "parameter.h"
#include "sqlInterface.h"
#include <QDateTime>

ParameterValue::ParameterValue(const QString & valueStr, const QString & typeStr)
{
    type = parseParameterType(typeStr);
    setValue(valueStr);
}

ParameterValue::ParameterValue()
{
    type = UNKNOWN;
    value.Uint = 0;
}

ParameterValue::Type ParameterValue::parseParameterType(const QString& typeStr)
{
    if(typeStr == "DOUBLE") return DOUBLE;
    if(typeStr == "UINT") return UINT;
    if(typeStr == "BOOL") return BOOL;
    if(typeStr == "PTIME") return PTIME;

    return UNKNOWN;
}

bool ParameterValue::isValidValue(const QString & valueStr)
{
    bool valid = false;

    switch(type)
    {
    case DOUBLE:
    {
        valueStr.toDouble(&valid);
    } break;

    case UINT:
    {
        valueStr.toUInt(&valid);
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

bool ParameterValue::setValue(const QString & valueStr)
{
    bool valid = false;

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

QString ParameterValue::getValueString(int precision)
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

int ParameterValue::isNotInRange(const ParameterValue& min, const ParameterValue& max)
{
    switch(type)
    {
    case DOUBLE:
    {
        int result = 0;
        if(value.Double < min.value.Double) result = -1;
        if(value.Double > max.value.Double) result = 1;
        return result;
    } break;

    case UINT:
    {
        int result = 0;
        if(value.Uint < min.value.Uint) result = -1;
        if(value.Uint > max.value.Uint) result = 1;
        return result;
    } break;

    case BOOL:
    {
        return true;
    } break;

    case PTIME:
    {
        int result = 0;
        if(value.Time < min.value.Time) result = -1;
        if(value.Time > max.value.Time) result = 1;
        return result;
    } break;

    case UNKNOWN:
    {
        return -1;
    } break;

    }

    return false;
}

// LAYOUTFORPARAMETER

Parameter::Parameter(const QString& name, ParameterValue& value, ParameterValue& min, ParameterValue& max)
{
    this->min = min; // We store the values for these, not just their text fields, so that it is efficient to check when edited values fall within range.
    this->max = max;
    this->value = value;
    this->name = name;
}

