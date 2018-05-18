#ifndef PARAMETER_H
#define PARAMETER_H

#include <QString>
#include <QVariant>
#include "sqlhandler/parameterserialization.h"

class Parameter
{
public:
    Parameter(const QString& name, int ID, int parentID, const parameter_min_max_val_serial_entry &entry);

    bool isValidValue(const QString &valueVar);
    bool setValue(const QString &valueVar);
    int isNotInRange(const parameter_value& newval);
    bool IsInRange() { return !isNotInRange(value); }

    static QString getValueDisplayString(parameter_value value, parameter_type type, int precision = 10);

    QString name;
    int ID;
    int parentID;
    parameter_type type;
    parameter_value value;
    parameter_value min;
    parameter_value max;
};

#endif // PARAMETER_H
