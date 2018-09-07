#include "sqlinterface.h"
#include <QSqlQuery>
#include "parameter.h"
#include <QDebug>
#include <QSqlError>

SQLInterface::SQLInterface()
{
}

bool SQLInterface::setDatabase(QString& path)
{
    if(dbIsSet_)
    {
        db_.close();
    }

    db_ = QSqlDatabase::addDatabase("QSQLITE");
    db_.setDatabaseName(path);

    if (!db_.open())
    {
        return false;
    }
    else
    {
       dbIsSet_ = true;
       return true;
    }
}

bool SQLInterface::getParameterStructure(QVector<TreeData> &structuredata)
{
    if(dbIsSet_)
    {
        const char *command = "SELECT parent.ID as parentID, child.ID, child.Name "
                                      "FROM ParameterStructure as parent, ParameterStructure as child "
                                      "WHERE child.lft > parent.lft "
                                      "AND child.rgt < parent.rgt "
                                      "AND child.dpt = parent.dpt + 1 "
                                      "UNION "
                                      "SELECT 0 as parentID, child.ID, child.Name "
                                      "FROM ParameterStructure as child "
                                      "WHERE child.dpt = 0 "
                                      "ORDER BY child.ID";

        QSqlQuery query;
        query.prepare(command);
        if(!query.exec())
        {
            // emit logError(query.lastError());
            return false;
        }
        else
        {
            while(query.next())
            {
                TreeData item;
                item.parentID = query.value(0).toInt();
                item.ID       = query.value(1).toInt();
                item.name     = query.value(2).toString();
                structuredata.push_back(item);
            }
        }
    }
    return true;
}

bool SQLInterface::getParameterValuesMinMax(std::map<uint32_t, parameter_min_max_val_serial_entry>& IDtoParam)
{
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
                    entry.min.val_uint = query.value(2).toUInt();
                    entry.max.val_uint = query.value(3).toUInt();
                    entry.value.val_uint = query.value(4).toUInt();
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

    return true;
}

bool SQLInterface::writeParameterValues(QVector<parameter_serial_entry>& writedata)
{
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
        const char *command = sqlcommand[entry.type];
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
                query.bindValue(":value", entry.value.val_uint);
            } break;

            case parametertype_ptime:
            {
                query.bindValue(":value", entry.value.val_ptime);
            } break;

            default:
            {
                //TODO: invalid type, log error
                return false;
            } break;
        }

        if(!query.exec())
        {
            qDebug() << query.lastError();
            // emit logError(query.lastError());
            return false;
        }
    }
    QSqlDatabase::database().commit();

    return true;
}

