#include "parametereditdelegate.h"
#include "parametermodel.h"
#include <QDateEdit>
#include <QDebug>

ParameterEditDelegate::ParameterEditDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{

}

QWidget *ParameterEditDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    const ParameterModel *parmodel = static_cast<const ParameterModel*>(index.model());
    const Parameter *par = parmodel->getParameterAtRow(index.row());
    if(par->type == parametertype_ptime)
    {
        QDateEdit *dateEdit = new QDateEdit(parent);
        dateEdit->setLocale(QLocale()); //NOTE: To make sure that it has the default locale (which we set in mainwindow constructor).
        dateEdit->setDisplayFormat("d. MMMM yyyy");

        dateEdit->setMinimumDate(Parameter::valueAsQDate(par->min));
        QDate maxdate = Parameter::valueAsQDate(par->max);
        //qDebug()<<maxdate.toString("d. MMMM yyyy");
        dateEdit->setMaximumDate(Parameter::valueAsQDate(par->max));
        return dateEdit;
    }
    else if(par->type == parametertype_bool)
        return 0; //NOTE: We don't use a delegate for editing bools, instead we just do "click means toggle" in the parametermodel. See ParameterModel::handleClick

    return new MyLineEdit(parent);
}

void ParameterEditDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    const ParameterModel *parmodel = static_cast<const ParameterModel*>(index.model());
    const Parameter *par = parmodel->getParameterAtRow(index.row());
    if(par->type == parametertype_ptime)
    {
        QDateEdit *dateEdit = static_cast<QDateEdit*>(editor);
        dateEdit->setDate(Parameter::valueAsQDate(par->value));
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
    const Parameter *par = parmodel->getParameterAtRow(index.row());
    if(par->type == parametertype_ptime)
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
