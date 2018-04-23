#include "parameter.h"
#include "sqlInterface.h"
#include <QDateTime>

/*
ParameterValue::ParameterValue(const QVariant & valueVar, const QString & typeStr)
{
    type = parseParameterType(typeStr);
    setValue(valueVar);
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

bool ParameterValue::isValidValue(const QVariant& valueVar)
{
    bool valid = false;

    switch(type)
    {
    case DOUBLE:
    {
        valueVar.toDouble(&valid);
    } break;

    case UINT:
    {
        valueVar.toUInt(&valid);
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

bool ParameterValue::setValue(const QVariant & valueVar)
{
    bool changed = false;

    switch(type)
    {
    case DOUBLE:
    {
        double newVal = valueVar.toDouble();
        if(value.Double != newVal)
        {
            value.Double = valueVar.toDouble();
            changed = true;
        }
    } break;

    case UINT:
    {
        uint64_t newVal = valueVar.toUInt();
        if(value.Uint != newVal)
        {
            value.Uint = newVal;
            changed = true;
        }
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

    return changed;
}

QString ParameterValue::getValueDisplayString(int precision)
{
    //This is for displaying the value in the parameter view
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
        QDateTime date = QDateTime::fromSecsSinceEpoch(value.Time);
        return QLocale().toString(date, "d. MMMM\nyyyy");
    } break;

    case UNKNOWN:
    {
        return "unknown type";
    } break;

    }
    return "";
}


QString ParameterValue::getValueDBString()
{
    // This is for storing the value back in the database. Must match how this type is represented in SQLLite db by INCA.
    switch(type)
    {
    case DOUBLE:
    {
        return QString::number(value.Double, 'g');
    } break;

    case UINT:
    {
        return QString::number(value.Uint);
    } break;

    case BOOL:
    {
        return value.Bool ? "1" : "0"; //TODO: check if this is correct!
    } break;

    case PTIME:
    {
        return QString::number(value.Time);
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
        return 0;
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


Parameter::Parameter(const QString& name, ParameterValue& value, ParameterValue& min, ParameterValue& max)
{
    this->min = min; // We store the values for these, not just their text fields, so that it is efficient to check when edited values fall within range.
    this->max = max;
    this->value = value;
    this->name = name;
}

*/

Parameter::Parameter(const QString& name, int ID, int parentID, const parameter_min_max_val_serial_entry& entry)
{
    this->name = name;
    this->ID = ID;
    this->parentID = parentID;
    this->type = (parameter_type)entry.type;
    this->min = entry.min;
    this->max = entry.max;
    this->value = entry.value;
}

bool Parameter::isValidValue(const QString &valueVar)
{
    bool valid = false;

    switch(type)
    {
    case parametertype_double:
    {
        valueVar.toDouble(&valid);
    } break;

    case parametertype_uint:
    {
        valueVar.toUInt(&valid);
    } break;

    case parametertype_bool:
    {
        //TODO: Not yet implemented!
    } break;

    case parametertype_ptime:
    {
        //TODO: Not yet implemented!
    } break;

    default:
    {
        //Do nothing
    } break;
    }

    return valid;
}

bool Parameter::setValue(const QString &valueVar)
{
    bool changed = false;

    switch(type)
    {
    case parametertype_double:
    {
        double newVal = valueVar.toDouble();
        if(value.val_double != newVal)
        {
            value.val_double = newVal;
            changed = true;
        }
    } break;

    case parametertype_uint:
    {
        uint64_t newVal = valueVar.toUInt();
        if(value.val_uint != newVal)
        {
            value.val_uint = newVal;
            changed = true;
        }
    } break;

    case parametertype_bool:
    {
        //TODO: Not yet implemented!
    } break;

    case parametertype_ptime:
    {
        //TODO: Not yet implemented!
    } break;

    default:
    {
        //Do nothing
    } break;

    }

    return changed;
}

QString Parameter::getValueDisplayString(parameter_value value, parameter_type type, int precision)
{
    //This is for displaying the value in the parameter view
    switch(type)
    {
    case parametertype_double:
    {
        return QString::number(value.val_double, 'g', precision);
    } break;

    case parametertype_uint:
    {
        return QString::number(value.val_uint);
    } break;

    case parametertype_bool:
    {
        return value.val_bool ? "true" : "false";
    } break;

    case parametertype_ptime:
    {
        QDateTime date = QDateTime::fromSecsSinceEpoch(value.val_ptime);
        return QLocale().toString(date, "d. MMMM\nyyyy");
    } break;

    default:
    {
        return "Unsupported type";
    } break;

    }
    return "";
}

int Parameter::isNotInRange(const parameter_value& newval)
{
    switch(type)
    {
    case parametertype_double:
    {
        int result = 0;
        if(newval.val_double < min.val_double) result = -1;
        if(newval.val_double > max.val_double) result = 1;
        return result;
    } break;

    case parametertype_uint:
    {
        int result = 0;
        if(newval.val_uint < min.val_uint) result = -1;
        if(newval.val_uint > max.val_uint) result = 1;
        return result;
    } break;

    case parametertype_bool:
    {
        return 0;
    } break;

    case parametertype_ptime:
    {
        int result = 0;
        if(newval.val_ptime < newval.val_ptime) result = -1;
        if(newval.val_ptime > max.val_ptime) result = 1;
        return result;
    } break;

    default:
    {
        return -1;
    } break;

    }

    return false;
}
