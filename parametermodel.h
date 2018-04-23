#ifndef PARAMETERVIEWMODEL_H
#define PARAMETERVIEWMODEL_H

#include <QAbstractTableModel>
#include "parameter.h"

struct ParameterEditAction
{
    int parameterID;
    parameter_value oldValue, newValue;
};

class ParameterModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit ParameterModel(QObject *parent = 0);
    ~ParameterModel();

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
    bool setData(const QModelIndex & index, const QVariant & value, int role = Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex &index) const;

    bool areAllParametersInRange() const;
    void addParameter(const QString&, int, int, const parameter_min_max_val_serial_entry&);
    void clearVisibleParameters();
    void setChildrenVisible(int);

    void *serializeParameterData(size_t *size);

    void setValue(int, parameter_value&); //NOTE: this is only to be used by the MainWindow's undo function
private:
    std::vector<int> visibleParamID_;
    std::map<int, Parameter*> IDtoParam_;
signals:
    void parameterWasEdited(ParameterEditAction);
};

#endif // PARAMETERVIEWMODEL_H
