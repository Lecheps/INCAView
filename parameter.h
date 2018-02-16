#ifndef PARAMETER_H
#define PARAMETER_H

#include <QString>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlQueryModel>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QSpacerItem>
#include <QLabel>

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

    ParameterValue(const QString &, const QString &);
    ParameterValue();

    static Type parseParameterType(const QString &);

    bool isValidValue(const QString &);
    bool setValue(const QString &);
    QString getValueString(int precision = 3);
    //Type getType() { return type; }
    int isNotInRange(const ParameterValue&, const ParameterValue&);

private:
    union
    {
        double Double;
        uint64_t Uint; // The database may not support this precision, but there is no reason to not use as many bits as possible here.
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

    bool IsInRange() { return !value.isNotInRange(min, max); }
};

#endif // PARAMETER_H
