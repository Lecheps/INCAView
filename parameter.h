#ifndef PARAMETER_H
#define PARAMETER_H

#include <QString>

class parameterValue
{
public:

    enum Type
    {
        DOUBLE,
        UINT,
        BOOL,
        PTIME,
    };

    static Type parseParameterType(const QString &);

    parameterValue(const QString &, const QString &);

    parameterValue();

    bool isValid() { return valid; }

    bool setValue(const QString &);

    QString getValueString(int precision = 3);

    Type getType() { return type; }

    bool isInRange(const parameterValue&, const parameterValue&);

private:
    union
    {
        double Double;
        uint64_t Uint; // The database may not support this precision, but there is no reason to not use as many bits as possible here.
        int64_t Bool;
        int64_t Time; // In seconds since 1-1-1970, i.e. posix time.
    } value;
    bool valid;
    Type type;
};

#endif // PARAMETER_H
