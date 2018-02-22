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
                return layout->value.getValueString(precision);
            } break;
            case 2:
            {
                return layout->min.getValueString(precision);
            } break;
            case 3:
            {
                return layout->max.getValueString(precision);
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
                bool valueWasChanged = param->value.setValue(strVal);

                if(valueWasChanged)
                {
                    emit parameterWasEdited(strVal, ID);
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

void ParameterModel::addParameter(int ID, const QString& name, const QString& typeStr, const QString& valueStr, const QString& minStr, const QString& maxStr)
{
    ParameterValue value(valueStr, typeStr);
    ParameterValue min(minStr, typeStr);
    ParameterValue max(maxStr, typeStr);
    Parameter *param = new Parameter(name, value, min, max);
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

void ParameterModel::setParameterVisible(int ID)
{
    if(IDtoParam_.count(ID) != 0 && std::find(visibleParamID_.begin(), visibleParamID_.end(), ID) == visibleParamID_.end())
    {
        int newindex = visibleParamID_.size();
        beginInsertRows(QModelIndex(), newindex, newindex);
        visibleParamID_.push_back(ID);
        endInsertRows();
    }
}
