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
        Parameter* param = IDtoParam_.at(ID);
        int precision = 10;
        switch(index.column())
        {
            case 0:
            {
                return param->name;
            } break;
            case 1:
            {
                return Parameter::getValueDisplayString(param->value, param->type, precision);
            } break;
            case 2:
            {
                return Parameter::getValueDisplayString(param->min, param->type, precision);
            } break;
            case 3:
            {
                return Parameter::getValueDisplayString(param->max, param->type, precision);
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
        if( (param->isNotInRange(param->value)==-1 && index.column()==2) ||
            (param->isNotInRange(param->value)==1 && index.column()==3) )
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
            bool valid = false;
            QString strVal;
            int64_t timeVal;
            uint64_t boolVal;
            if(param->type == parametertype_ptime)
            {
                timeVal = value.toLongLong();
                valid = true; // All integer values represent a valid date.
            }
            else if(param->type == parametertype_bool)
            {
                boolVal = value.toULongLong();
                valid = true; // We control what is passed here, so it is valid.
            }
            else
            {
                strVal = value.toString();
                valid = param->isValidValue(strVal);
            }

            if(valid)
            {
                parameter_value oldVal = param->value;
                bool valueWasChanged = false;
                if(param->type == parametertype_ptime)
                {
                    valueWasChanged = timeVal == param->value.val_ptime;
                    param->value.val_ptime = timeVal;
                }
                else if(param->type == parametertype_bool)
                {
                    valueWasChanged = true; //Since this is a toggle, the value was always changed..
                    param->value.val_bool = boolVal;
                }
                else
                {
                    valueWasChanged = param->setValue(strVal);
                }

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

void ParameterModel::handleClick(const QModelIndex &index)
{
    //NOTE: we override editing for bool types so that click == toggle.
    if(index.column() == 1 && getTypeOfRow(index.row()) == parametertype_bool)
    {
        parameter_value oldVal = getRawValue(index.row());
        setData(index, QVariant((qulonglong)!oldVal.val_bool), Qt::EditRole);
    }
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

void ParameterModel::addParameter(const QString& name, int ID, int parentID, const parameter_min_max_val_serial_entry& entry)
{
    Parameter *param = new Parameter(name, ID, parentID, entry);
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

//NOTE: this should only be used by the LineEditDelegate (or by this class itself)
parameter_type ParameterModel::getTypeOfRow(int row)
{
    int ID = visibleParamID_[row];
    return IDtoParam_[ID]->type;
}

//NOTE: this should only be used by the LineEditDelegate (or by this class itself)
parameter_value ParameterModel::getRawValue(int row)
{
    int ID = visibleParamID_[row];
    return IDtoParam_[ID]->value;
}


//NOTE! this is only to be used by the MainWindow's undo function
void ParameterModel::setValue(int ID, parameter_value& value)
{
    beginResetModel();
    IDtoParam_[ID]->value = value;
    endResetModel();
}

void ParameterModel::serializeParameterData(QVector<parameter_serial_entry>& outdata)
{
    for(auto par_pair : IDtoParam_)
    {
        uint32_t ID = par_pair.first;
        Parameter *param = par_pair.second;

        parameter_serial_entry entry = {};
        entry.ID = ID;
        entry.type = param->type;
        entry.value = param->value;

        outdata.push_back(entry);
    }

    return;
}
