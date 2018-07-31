#include "parameter.h"
#include <QDateTime>
#include <QLocale>

Parameter::Parameter(const QString& name, int ID, int parentID, const parameter_min_max_val_serial_entry& entry)
{
    this->name = name;
    this->ID = ID;
    this->parentID = parentID;
    type = (parameter_type)entry.type;
    min = entry.min;
    max = entry.max;
    value = entry.value;

    if(this->type == parametertype_ptime)
    {
        Parameter::clipTimeValue(min);
        Parameter::clipTimeValue(max);
        Parameter::clipTimeValue(value);
    }
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
        valid = true; //NOTE: This is handled differently, so we don't do anything here
    } break;

    case parametertype_ptime:
    {
        valid = true; //NOTE: This is handled differently, so we don't do anything here
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
        //NOTE! This is handled differently, so we don't set it here.
    } break;

    case parametertype_ptime:
    {
        //NOTE! This is handled differently, so we don't set it here.
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
        QDate date = Parameter::valueAsQDate(value);
        return QLocale().toString(date, "d. MMMM yyyy");
    } break;

    default:
    {
        return "Unsupported type";
    } break;

    }
    return "";
}

QDate Parameter::valueAsQDate(parameter_value value)
{
    return QDateTime::fromSecsSinceEpoch(value.val_ptime).date();
}

void Parameter::clipTimeValue(parameter_value &value)
{
    //NOTE: The range of valid seconds for a QDateTime may vary on different architectures, so if we are unlucky, this clipping may not be good enough.

    if(value.val_ptime > std::numeric_limits<qint64>::max() / 10000)
    {
        value.val_ptime = std::numeric_limits<qint64>::max() / 10000;
    }
    else if(value.val_ptime < std::numeric_limits<qint64>::min() / 10000)
    {
        value.val_ptime = std::numeric_limits<qint64>::min() / 10000;
    }
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
        if(newval.val_ptime < min.val_ptime) result = -1;
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
