#include "lineeditdelegate.h"
#include "parametermodel.h"
#include <QDateEdit>

LineEditDelegate::LineEditDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{

}

QWidget *LineEditDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    ParameterModel *parmodel = (ParameterModel*)(index.model());
    parameter_type type = parmodel->getTypeOfRow(index.row());
    if(type == parametertype_ptime)
    {
        QDateEdit *dateEdit = new QDateEdit(parent);
        dateEdit->setLocale(QLocale()); //NOTE: To make sure that it has the default locale (which we set in mainwindow constructor).
        dateEdit->setDisplayFormat("d. MMMM yyyy");
        return dateEdit;
    }
    else if(type == parametertype_bool)
        return 0; //NOTE: We don't use a delegate for editing bools, instead we just do click=toggle in the parametermodel.

    return new MyLineEdit(parent);
}

void LineEditDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    ParameterModel *parmodel = (ParameterModel*)(index.model());
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

void LineEditDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
    ParameterModel *parmodel = (ParameterModel*)(model);
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

void LineEditDelegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    editor->setGeometry(option.rect);
}
