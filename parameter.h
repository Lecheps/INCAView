#ifndef PARAMETER_H
#define PARAMETER_H

#include <QString>
#include <QVariant>

class ParameterValue
{
public:

    enum Type
    {
        DOUBLE,
        UINT,
        BOOL,
        PTIME,
        UNKNOWN,
    };

    ParameterValue(const QVariant &, const QString &);
    ParameterValue();

    static Type parseParameterType(const QString &);

    bool isValidValue(const QVariant &);
    bool setValue(const QVariant &valueVar);
    QString getValueDisplayString(int precision = 10);
    //QString getValueDBString();
    int isNotInRange(const ParameterValue&, const ParameterValue&);

    union
    {
        double Double;
        uint64_t Uint;
        int64_t Bool;
        int64_t Time; // In seconds since 1-1-1970, i.e. posix time.
    } value;
    Type type;
};

class Parameter
{
public:
    Parameter(const QString&, ParameterValue&, ParameterValue&, ParameterValue&);

    QString name;
    ParameterValue value;
    ParameterValue min;
    ParameterValue max;
    int parentID;

    bool IsInRange() { return !value.isNotInRange(min, max); }
};

#endif // PARAMETER_H
