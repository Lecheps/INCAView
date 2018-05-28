#ifndef LINEEDITDELEGATE_H
#define LINEEDITDELEGATE_H

#include <QStyledItemDelegate>
#include <QLineEdit>

class ParameterEditDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    ParameterEditDelegate(QObject *parent = 0);

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                          const QModelIndex &index) const override;

    void setEditorData(QWidget *editor, const QModelIndex &index) const override;
    void setModelData(QWidget *editor, QAbstractItemModel *model,
                      const QModelIndex &index) const override;

    void updateEditorGeometry(QWidget *editor,
        const QStyleOptionViewItem &option, const QModelIndex &index) const override;
};


class MyLineEdit : public QLineEdit
{
    Q_OBJECT
public:
    MyLineEdit(QWidget* parent = 0) : QLineEdit(parent) {}
protected:
    void focusInEvent(QFocusEvent *e)
    {
        QLineEdit::focusInEvent(e);
        deselect();
    }
};

#endif // LINEEDITDELEGATE_H
