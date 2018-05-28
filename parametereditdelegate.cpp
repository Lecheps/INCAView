#include "parametereditdelegate.h"
#include "parametermodel.h"
#include <QDateEdit>

ParameterEditDelegate::ParameterEditDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{

}

QWidget *ParameterEditDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    const ParameterModel *parmodel = static_cast<const ParameterModel*>(index.model());
    parameter_type type = parmodel->getTypeOfRow(index.row());
    if(type == parametertype_ptime)
    {
        QDateEdit *dateEdit = new QDateEdit(parent);
        dateEdit->setLocale(QLocale()); //NOTE: To make sure that it has the default locale (which we set in mainwindow constructor).
        dateEdit->setDisplayFormat("d. MMMM yyyy");
        return dateEdit;
    }
    else if(type == parametertype_bool)
        return 0; //NOTE: We don't use a delegate for editing bools, instead we just do "click means toggle" in the parametermodel. See ParameterModel::handleClick

    return new MyLineEdit(parent);
}

void ParameterEditDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    const ParameterModel *parmodel = static_cast<const ParameterModel*>(index.model());
    if(parmodel->getTypeOfRow(index.row()) == parametertype_ptime)
    {
        QDateEdit *dateEdit = static_cast<QDateEdit*>(editor);
        dateEdit->setDate(QDateTime::fromSecsSinceEpoch(parmodel->getRawValue(index.row()).val_ptime).date());
    }
    else
    {
        QString value = index.model()->data(index, Qt::EditRole).toString();
        MyLineEdit *lineEdit = static_cast<MyLineEdit*>(editor);
        lineEdit->setText(value);
    }
}

void ParameterEditDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
    ParameterModel *parmodel = static_cast<ParameterModel*>(model);
    if(parmodel->getTypeOfRow(index.row()) == parametertype_ptime)
    {
        QDateEdit *dateEdit = static_cast<QDateEdit*>(editor);
        int64_t intVal = QDateTime(dateEdit->date()).toSecsSinceEpoch();
        model->setData(index, QVariant((qlonglong)intVal), Qt::EditRole);
    }
    else
    {
        MyLineEdit *lineEdit = static_cast<MyLineEdit*>(editor);
        QString value = lineEdit->text();
        model->setData(index, value, Qt::EditRole);
    }
}

void ParameterEditDelegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    editor->setGeometry(option.rect);
}
