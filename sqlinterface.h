#ifndef SQLINTERFACE_H
#define SQLINTERFACE_H

#include "sqlhandler/serialization.h"
#include "treemodel.h"
#include <QSqlDatabase>

class SQLInterface
{
public:
    SQLInterface();
    ~SQLInterface();

    bool getParameterStructure(QVector<TreeData> &structuredata);
    bool getParameterValuesMinMax(std::map<uint32_t, parameter_min_max_val_serial_entry>& IDtoParam);
    bool writeParameterValues(QVector<parameter_serial_entry>& writedata);

    bool getResultOrInputStructure(QVector<TreeData> &structuredata, const char *table);
    bool getResultOrInputValues(const char *table, const QVector<int>& IDs, QVector<QVector<double>> &seriesout, int64_t &startdateout);

    bool setDatabase(QString& path);
    bool databaseIsSet() { return dbIsSet_; }

private:
    bool dbIsSet_ = false;
    QSqlDatabase db_;
};

#endif // SQLINTERFACE_H
