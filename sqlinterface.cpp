#include "sqlinterface.h"
#include <QSqlQuery>
#include "parameter.h"
#include <QDebug>
#include <QSqlError>
#include <limits>

SQLInterface::SQLInterface()
{
    db_ = QSqlDatabase::addDatabase("QSQLITE");
}

SQLInterface::~SQLInterface()
{
    if(dbIsSet_ && db_.open())
    {
        db_.close();
    }
}

bool SQLInterface::setDatabase(QString& path)
{
    db_.setDatabaseName(path);

    dbIsSet_ = true;
    return true;
}

bool SQLInterface::getParameterStructure(QVector<TreeData> &structuredata)
{
    if(!db_.open())
    {
        return false;
    }

    const char *command = "SELECT parent.ID as parentID, child.ID, child.name, child.unit, child.description "
                                  "FROM ParameterStructure as parent, ParameterStructure as child "
                                  "WHERE child.lft > parent.lft "
                                  "AND child.rgt < parent.rgt "
                                  "AND child.dpt = parent.dpt + 1 "
                                  "UNION "
                                  "SELECT 0 as parentID, child.ID, child.Name, child.unit, child.description "
                                  "FROM ParameterStructure as child "
                                  "WHERE child.dpt = 0 "
                                  "ORDER BY child.ID";
    QSqlQuery query;
    if(!query.prepare(command))
    {
        qDebug() << query.lastError();
    }
    if(!query.exec())
    {
        // emit logError(query.lastError());
        qDebug() << query.lastError();
        db_.close();
        return false;
    }

    while(query.next())
    {
        TreeData item;
        item.parentID    = query.value(0).toInt();
        item.ID          = query.value(1).toInt();
        item.name        = query.value(2).toString();
        item.unit        = query.value(3).toString();
        item.description = query.value(4).toString();
        structuredata.push_back(item);
    }

    db_.close();
    return true;
}

bool SQLInterface::getParameterValuesMinMax(std::map<uint32_t, parameter_min_max_val_serial_entry>& IDtoParam)
{
    if(!db_.open())
    {
        return false;
    }

    const char *value_tables[4] =
    {
        "bool",
        "double",
        "int",
        "ptime",
    };

    parameter_type types[4] =
    {
        parametertype_bool,
        parametertype_double,
        parametertype_uint,
        parametertype_ptime,
    };

    for(int i = 0; i < 4; ++i)
    {
        char commandbuf[512];
        sprintf(commandbuf,
            "SELECT "
             "ParameterStructure.ID, ParameterStructure.type, ParameterValues_%s.minimum, ParameterValues_%s.maximum, ParameterValues_%s.value "
             "FROM ParameterStructure INNER JOIN ParameterValues_%s "
             "ON ParameterStructure.ID = ParameterValues_%s.ID; ",
             value_tables[i], value_tables[i], value_tables[i], value_tables[i], value_tables[i]);

        QSqlQuery query;
        query.prepare(commandbuf);
        if(!query.exec())
        {
            // emit logError(query.lastError());
            db_.close();
            return false;
        }

        while(query.next())
        {
            parameter_min_max_val_serial_entry entry;
            entry.ID = query.value(0).toInt();
            QString typetxt = query.value(1).toString();
            entry.type = Parameter::parseType(typetxt);
            if(entry.type != types[i])
            {
                //TODO:
                // emit logError("Database: Parameter with ID %d is registered as having type %s, but is in the table for %s.\n")
                //                                   entry.ID, typetxt, value_tables[i]);
                db_.close();
                return false;
            }
            switch(types[i])
            {
                case parametertype_bool :
                {
                    entry.min.val_bool = (bool)query.value(2).toInt();
                    entry.max.val_bool = (bool)query.value(3).toInt();
                    entry.value.val_bool = (bool)query.value(4).toInt();
                } break;

                case parametertype_double :
                {
                    entry.min.val_double = query.value(2).toDouble();
                    entry.max.val_double = query.value(3).toDouble();
                    entry.value.val_double = query.value(4).toDouble();
                } break;

                case parametertype_uint :
                {
                    entry.min.val_uint = query.value(2).toULongLong();
                    entry.max.val_uint = query.value(3).toULongLong();
                    entry.value.val_uint = query.value(4).toULongLong();
                } break;

                case parametertype_ptime :
                {
                    entry.min.val_ptime = query.value(2).toLongLong();
                    entry.max.val_ptime = query.value(3).toLongLong();
                    entry.value.val_ptime = query.value(4).toLongLong();
                } break;
            }

            IDtoParam[entry.ID] = entry;
        }
    }
    db_.close();
    return true;
}

bool SQLInterface::writeParameterValues(QVector<parameter_serial_entry>& writedata)
{
    if(!db_.open())
    {
        return false;
    }

    //WARNING: Volatile! This depends on the order of the enums in parameter_type not being changed.
    const char *sqlcommand[4] =
    {
        "UPDATE ParameterValues_bool SET value = (:value) WHERE ID = (:id);",
        "UPDATE ParameterValues_double SET value =(:value) WHERE ID = (:id);",
        "UPDATE ParameterValues_int SET value = (:value) WHERE ID = (:id);",
        "UPDATE ParameterValues_ptime SET value = (:value) WHERE ID = (:id);",
    };

    size_t numparameters = writedata.size();

    QSqlDatabase::database().transaction();
    for(int i = 0; i < numparameters; ++i)
    {
        parameter_serial_entry &entry = writedata[i];

        QSqlQuery query;
        query.prepare(sqlcommand[entry.type]);
        query.bindValue(":id", entry.ID);
        switch(entry.type)
        {
            case parametertype_double:
            {
                query.bindValue(":value", entry.value.val_double);
            } break;

            case parametertype_bool:
            {
                query.bindValue(":value", (int)entry.value.val_bool);
            } break;

            case parametertype_uint:
            {
                query.bindValue(":value", qulonglong(entry.value.val_uint));
            } break;

            case parametertype_ptime:
            {
                query.bindValue(":value", qlonglong( entry.value.val_ptime));
            } break;

            default:
            {
                //TODO: invalid type, log error
                db_.close();
                return false;
            } break;
        }

        if(!query.exec())
        {
            qDebug() << query.lastError();
            // emit logError(query.lastError());
            db_.close();
            return false;
        }
    }
    QSqlDatabase::database().commit();

    db_.close();
    return true;
}

bool SQLInterface::getResultOrInputStructure(QVector<TreeData> &structuredata, const char *table)
{
    if(!db_.open())
    {
        db_.close();
    }

    char sqlcommand[512];
    sprintf(sqlcommand,
            "SELECT parent.ID AS parentID, child.ID, child.name, child.unit "
            "FROM %s AS parent, %s AS child "
            "WHERE child.lft > parent.lft "
            "AND child.rgt < parent.rgt "
            "AND child.dpt = parent.dpt + 1 "
            "UNION "
            "SELECT 0 as parentID, child.ID, child.name, child.unit "
            "FROM %s as child "
            "WHERE child.dpt = 0 "
            "ORDER BY child.ID",
        table, table, table
    );

    QSqlQuery query;
    query.prepare(sqlcommand);

    if(!query.exec())
    {
        // emit logError(query.lastError());
        return false;
    }

    while(query.next())
    {
        TreeData entry;
        entry.parentID = query.value(0).toInt();
        entry.ID       = query.value(1).toInt();
        entry.name     = query.value(2).toString();
        entry.unit     = query.value(3).toString();
        structuredata.push_back(entry);
    }

    db_.close();

    return true;
}

bool SQLInterface::getResultOrInputValues(const char *table, const QVector<int>& IDs, QVector<QVector<double>> &seriesout, QVector<int64_t> &startdatesout)
{

    if(!db_.open())
    {
        return false;
    }

    char sqlcommand[512];

    //NOTE: For now we only handle cases where the timestep is one day. Otherwise we would also have to read the timestep from somewhere or read out the entire series of time values.
    for(int ID : IDs)
    {
        sprintf(sqlcommand, "SELECT date, value FROM %s WHERE ID=%d;", table, ID);

        QSqlQuery query;
        query.prepare(sqlcommand);

        if(!query.exec())
        {
            // emit logError(query.lastError());
            db_.close();
            return false;
        }

        QVector<double> series;
        series.reserve(100); // We don't know how large it is, but this tends to speed things up.

        int64_t startDate;
        bool first = true;

        while(query.next())
        {
            if(first)
            {
                startDate = query.value(0).toLongLong();
                first = false;
            }

            if(query.value(1).isNull())
            {
                series.push_back(std::numeric_limits<double>::quiet_NaN());
            }
            else
            {
                series.push_back(query.value(1).toDouble());
            }
        }

        seriesout.push_back(series);
        startdatesout.push_back(startDate);
    }

    db_.close();
    return true;
}

bool SQLInterface::getExenameFromParameterInfo(QString& exename)
{
    if(!db_.open())
    {
        return false;
    }

    QSqlQuery query;
    query.prepare("SELECT Exename FROM Info");

    if(!query.exec())
    {
        // emit logError(query.lastError());
        qDebug() << query.lastError();
        db_.close();
        return false;
    }

    query.next();
    exename = query.value(0).toString();

    db_.close();
    return true;
}
