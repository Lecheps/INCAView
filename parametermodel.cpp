#include "parametermodel.h"
#include <QFont>
#include <QBrush>

ParameterModel::ParameterModel(QObject *parent)
    :QAbstractTableModel(parent)
{

}

ParameterModel::~ParameterModel()
{
    for(auto& key_value : IDtoParam_)
    {
        delete key_value.second;
    }
    IDtoParam_.clear();
    visibleParamID_.clear();
}

int ParameterModel::rowCount(const QModelIndex &parent) const
{
    return visibleParamID_.size();
}

int ParameterModel::columnCount(const QModelIndex &parent) const
{
    return 4;
}

QVariant ParameterModel::data(const QModelIndex &index, int role) const
{
    switch(role)
    {
    case Qt::DisplayRole:
    case Qt::EditRole:
    {
        int ID = visibleParamID_[index.row()];
        Parameter* layout = IDtoParam_.at(ID);
        int precision = 10;
        switch(index.column())
        {
            case 0:
            {
                return layout->name;
            } break;
            case 1:
            {
                return layout->value.getValueDisplayString(precision);
            } break;
            case 2:
            {
                return layout->min.getValueDisplayString(precision);
            } break;
            case 3:
            {
                return layout->max.getValueDisplayString(precision);
            } break;
        }
    } break;

    case Qt::FontRole:
    {
        if(index.column()!=1)
        {
            QFont boldFont;
            boldFont.setBold(true);
            return boldFont;
        }

    } break;

    case Qt::ForegroundRole:
    {
        int ID = visibleParamID_[index.row()];
        Parameter* param = IDtoParam_.at(ID);
        if( (param->value.isNotInRange(param->min, param->max)==-1 && index.column()==2) ||
            (param->value.isNotInRange(param->min, param->max)==1 && index.column()==3) )
        {
            QBrush colorbrush;
            colorbrush.setColor(Qt::red);
            return colorbrush;
        }
    } break;
    }
    return QVariant();
}

QVariant ParameterModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(role == Qt::DisplayRole && orientation == Qt::Horizontal)
    {
        switch(section)
        {
        case 0:
        {
            return QString("Name");
        } break;
        case 1:
        {
            return QString("Value");
        } break;
        case 2:
        {
            return QString("Min");
        } break;
        case 3:
        {
            return QString("Max");
        } break;
        }
    }
    return QVariant();
}

bool ParameterModel::setData(const QModelIndex & index, const QVariant & value, int role)
{
    if(role == Qt::EditRole)
    {
        if(index.column() == 1)
        {
            int ID = visibleParamID_[index.row()];
            Parameter* param = IDtoParam_.at(ID);
            QString strVal = value.toString();
            bool valid = param->value.isValidValue(strVal);
            if(valid)
            {
                ParameterValue oldVal = param->value;
                bool valueWasChanged = param->value.setValue(strVal);

                if(valueWasChanged)
                {
                    ParameterEditAction editAction;
                    editAction.parameterID = ID;
                    editAction.oldValue = oldVal;
                    editAction.newValue = param->value;
                    emit parameterWasEdited(editAction);
                    return true;
                }
            }
            return false;
        }
        else
        {
            return false;
        }
    }
    return true;
}

Qt::ItemFlags ParameterModel::flags(const QModelIndex &index) const
{
    if(index.column() == 1)
    {
        return Qt::ItemIsEditable | QAbstractTableModel::flags(index);
    }
    return QAbstractTableModel::flags(index);
}

bool ParameterModel::areAllParametersInRange() const
{
    bool result = true;
    for(auto& key_value : IDtoParam_)
    {
        Parameter* param = key_value.second;
        if(!param->IsInRange())
        {
            result = false;
            break;
        }
    }
    return result;
}

void ParameterModel::addParameter(int ID, int parentID, const QString& name, const QString& typeStr, const QVariant &valueVar, const QVariant &minVar, const QVariant &maxVar)
{
    ParameterValue value(valueVar, typeStr);
    ParameterValue min(minVar, typeStr);
    ParameterValue max(maxVar, typeStr);
    Parameter *param = new Parameter(name, value, min, max);
    param->parentID = parentID;
    IDtoParam_[ID] = param;
}

void ParameterModel::clearVisibleParameters()
{
    if(visibleParamID_.size() > 0)
    {
        beginRemoveRows(QModelIndex(), 0, visibleParamID_.size() - 1);
        visibleParamID_.clear();
        endRemoveRows();
    }
}

void ParameterModel::setChildrenVisible(int parentID)
{
    for(auto id_par : IDtoParam_)
    {
        Parameter *param = id_par.second;
        int childID = id_par.first;
        if(param->parentID == parentID)
        {
            int newindex = visibleParamID_.size();
            beginInsertRows(QModelIndex(), newindex, newindex);
            visibleParamID_.push_back(childID);
            endInsertRows();
        }
    }
}

//NOTE! this is only to be used by the MainWindow's undo function
void ParameterModel::setValue(int ID, ParameterValue& value)
{
    beginResetModel();
    IDtoParam_[ID]->value = value;
    endResetModel();
}

#include "sqlhandler/parameterserialization.h"

void* ParameterModel::serializeParameterData(size_t *size)
{

    QVector<parameter_serial_entry> outdata;
    for(auto par_pair : IDtoParam_)
    {
        uint32_t ID = par_pair.first;
        Parameter *param = par_pair.second;

        parameter_serial_entry entry = {};
        entry.ID = ID;
        switch(param->value.type)
        {
            case ParameterValue::DOUBLE:
            {
                entry.type = parametertype_double;
                entry.value.val_double = param->value.value.Double;
            } break;

            case ParameterValue::BOOL:
            {
                entry.type = parametertype_bool;
                entry.value.val_bool = param->value.value.Bool;
            } break;

            case ParameterValue::UINT:
            {
                entry.type = parametertype_uint;
                entry.value.val_uint = param->value.value.Uint;
            } break;

            case ParameterValue::PTIME:
            {
                entry.type = parametertype_ptime;
                entry.value.val_ptime = param->value.value.Time;
            } break;

            case ParameterValue::UNKNOWN:
            {
                qDebug("Tried to serialize unknown parameter type.");
            } break;
        }
        outdata.push_back(entry);
    }

    uint64_t count = outdata.count();
    *size = sizeof(uint64_t) + count*sizeof(parameter_serial_entry);
    void *result = malloc(*size);

    uint8_t *data = (uint8_t *)result;
    *(uint64_t *)data = count;
    data += sizeof(uint64_t);
    memcpy(data, outdata.data(), count*sizeof(parameter_serial_entry));

    return result;
}
