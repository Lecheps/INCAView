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

class parameterValue
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

    parameterValue(const QString &, const QString &);
    parameterValue();

    static Type parseParameterType(const QString &);

    bool isValid() { return valid; }
    bool setValue(const QString &);
    QString getValueString(int precision = 3);
    //Type getType() { return type; }
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

class layoutForParameter : public QObject
{

    Q_OBJECT
public:
    //QHBoxLayout* layout;

    //QSpacerItem* spacer;
    layoutForParameter(QString&, parameterValue&, parameterValue&, parameterValue&);
    //layoutForParameter() {};
    ~layoutForParameter();

    void addToGrid(QGridLayout*,int);
    void setVisible(bool);

    void valueEditedReceiveMessage(const QString&);

    bool isValueValidAndInRange() { return valueIsValidAndInRange; }
private:
    QLabel* parameterNameView;
    QLineEdit* parameterValueView;
    QLabel* parameterMinView;
    QLabel* parameterMaxView;
    parameterValue value;
    parameterValue min;
    parameterValue max;
    bool valueIsValidAndInRange;

signals:
    void signalValueWasEdited(const QString&);
};

#endif // PARAMETER_H
