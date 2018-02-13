#include "parameter.h"
#include <QDateTime>

parameterValue::parameterValue(const QString & valueStr, const QString & typeStr)
{
    type = parseParameterType(typeStr);
    setValue(valueStr);
}

parameterValue::parameterValue()
{
    type = DOUBLE;
    value.Double = 0.0;
}

parameterValue::Type parameterValue::parseParameterType(const QString& typeStr)
{
    if(typeStr == "DOUBLE") return DOUBLE;
    if(typeStr == "UINT") return UINT;
    if(typeStr == "BOOL") return BOOL;
    if(typeStr == "PTIME") return PTIME;

    qFatal(("Unknown type " + typeStr + " for parameter values.").toLatin1().data());
}

bool parameterValue::setValue(const QString & valueStr)
{
    switch(type)
    {
    case DOUBLE:
    {
        value.Double = valueStr.toDouble(&valid);
    }break;

    case UINT:
    {
        value.Uint = valueStr.toUInt(&valid);
    }break;

    case BOOL:
    {
        //TODO: Not yet implemented!
    }break;

    case PTIME:
    {
        //TODO: Not yet implemented!
    }break;

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
    }break;

    case UINT:
    {
        return QString::number(value.Uint);
    }break;

    case BOOL:
    {
        return value.Bool ? "true" : "false";
    }break;

    case PTIME:
    {
        QDateTime dt = QDateTime::fromSecsSinceEpoch(value.Time);
        return dt.toString("d. MMMM\nyyyy");
    }break;

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
    }break;

    case UINT:
    {
        return (value.Uint >= min.value.Uint) && (value.Uint <= max.value.Uint);
    }break;

    case BOOL:
    {
        return true;
    }break;

    case PTIME:
    {
        return (value.Time >= min.value.Time ) && (value.Time  <= max.value.Time );
    }break;

    }

    return false;
}

