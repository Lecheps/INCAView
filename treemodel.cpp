/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

/*
    treemodel.cpp

    Provides a simple tree model to show how to create and use hierarchical
    models.
*/

#include "treeitem.h"
#include "treemodel.h"


#include <QStringList>
#include <QDebug>
#include "unordered_map"

TreeModel::TreeModel(const QString &data, QObject *parent)
    : QAbstractItemModel(parent)
{
    QList<QVariant> rootData;
    rootData << "Title" << "Summary";
    rootItem = new TreeItem(rootData);
    setupModelData(data.split(QString("\n")), rootItem);
}

TreeModel::TreeModel(QObject *parent)
    : QAbstractItemModel(parent)
{
    QList<QVariant> rootData;
    rootData << "Parameter Structure" << "Database ID";
    rootItem = new TreeItem(rootData);

    connectDB();
    QSqlQueryModel uniqueParents;
    uniqueParents.setQuery("select ID,name from ParameterStructure where "
                           "dpt = 0;"); //This will return only indexers, indexes and the root node
    std::unordered_map<int,TreeItem*> uniqueKeys;
    for (int i = 0; i < uniqueParents.rowCount(); ++i)
    {
        QModelIndex keyIndex = uniqueParents.index(i, 0);
        QModelIndex keyString = uniqueParents.index(i,1);
        int index = uniqueParents.data(keyIndex).toInt();
        std::string str = uniqueParents.data(keyString).toString().toStdString();
        QList<QVariant> data;
        QVariant name(QString(str.c_str()));
        data << name << index;
        qDebug() << index;
        TreeItem* dummy = new TreeItem(data,rootItem);

        rootItem->appendChild(dummy);
        uniqueKeys[index] = dummy;
    }
    disconnectDB();

    connectDB();
    QSqlQueryModel getStructure;
    getStructure.setQuery("SELECT parent.ID as parentID, child.ID as childID, parent.name as parentName, child.Name as childName "
                          "FROM ParameterStructure as Parent, ParameterStructure as child "
                          "WHERE child.lft > parent.lft "
                          "AND child.rgt < parent.rgt "
                          "AND child.dpt = parent.dpt + 1 "
                          "AND child.type is null;"
                          );
    for (int i = 0; i < getStructure.rowCount(); ++i)
    {
        QModelIndex parentIndex = getStructure.index(i, 0);
        QModelIndex childIndex = getStructure.index(i, 1);
        QModelIndex parentStrIndex = getStructure.index(i, 2);
        QModelIndex childStrIndex = getStructure.index(i, 3);
        int parentKey = getStructure.data(parentIndex).toInt();
        int childKey = getStructure.data(childIndex).toInt();
        QString parentStrKey = getStructure.data(parentStrIndex).toString();
        QString childStrKey = getStructure.data(childStrIndex).toString();
        QList<QVariant> parentData, childData;
        QVariant parentName(parentStrKey);
        QVariant childName(childStrKey);
        parentData << parentName << parentKey;
        childData << childName << childKey;

        if (parentKey == 1)
        {
            TreeItem* childPnt = new TreeItem(childData,uniqueKeys[1]);
            uniqueKeys[childKey] = childPnt;
            uniqueKeys[1]->appendChild(childPnt);
        }
        else
        {
            uniqueKeys[childKey] = new TreeItem(childData,uniqueKeys[parentKey]);
            uniqueKeys[parentKey]->appendChild(uniqueKeys[childKey]);

        }
    }
    disconnectDB();
}


TreeModel::TreeModel(bool dummy,QObject *parent)
    : QAbstractItemModel(parent)
{
    QList<QVariant> rootData;
    rootData << "Results Structure" << "Database ID";
    rootItem = new TreeItem(rootData);

    connectDB();
    QSqlQueryModel uniqueParents;
    uniqueParents.setQuery("select ID,name from ResultsStructure where "
                           "isIndexer or isIndex;"); //This will return only indexers, indexes and the root node
    std::unordered_map<int,TreeItem*> uniqueKeys;
    for (int i = 0; i < uniqueParents.rowCount(); ++i)    {
        QModelIndex keyIndex = uniqueParents.index(i, 0);
        QModelIndex keyString = uniqueParents.index(i,1);
        int index = uniqueParents.data(keyIndex).toInt();
        std::string str = uniqueParents.data(keyString).toString().toStdString();
        QList<QVariant> data;
        QVariant name(QString(str.c_str()));
        data << name << index;
        qDebug() << index;
        TreeItem* dummy = new TreeItem(data,rootItem);

        rootItem->appendChild(dummy);
        uniqueKeys[index] = dummy;
    }
    disconnectDB();

    connectDB();
    QSqlQueryModel getStructure;
    getStructure.setQuery("SELECT parent.ID as parentID, child.ID as childID, parent.name as parentName, child.Name as childName "
                          "FROM ResultsStructure as Parent, ResultsStructure as child "
                          "WHERE child.lft > parent.lft "
                          "AND child.rgt < parent.rgt "
                          "AND child.dpt = parent.dpt + 1 "
                          "AND (child.isIndexer is null AND child.isIndex is null)"
                          );
    for (int i = 0; i < getStructure.rowCount(); ++i)
    {
        QModelIndex parentIndex = getStructure.index(i, 0);
        QModelIndex childIndex = getStructure.index(i, 1);
        QModelIndex parentStrIndex = getStructure.index(i, 2);
        QModelIndex childStrIndex = getStructure.index(i, 3);
        int parentKey = getStructure.data(parentIndex).toInt();
        int childKey = getStructure.data(childIndex).toInt();
        QString parentStrKey = getStructure.data(parentStrIndex).toString();
        QString childStrKey = getStructure.data(childStrIndex).toString();
        QList<QVariant> parentData, childData;
        QVariant parentName(parentStrKey);
        QVariant childName(childStrKey);
        parentData << parentName << parentKey;
        childData << childName << childKey;

        if (parentKey == 1)
        {
            TreeItem* childPnt = new TreeItem(childData,uniqueKeys[1]);
            uniqueKeys[childKey] = childPnt;
            uniqueKeys[1]->appendChild(childPnt);
        }
        else
        {
            uniqueKeys[childKey] = new TreeItem(childData,uniqueKeys[parentKey]);
            uniqueKeys[parentKey]->appendChild(uniqueKeys[childKey]);

        }
    }
    disconnectDB();
}


TreeModel::~TreeModel()
{
    delete rootItem;
}

int TreeModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return static_cast<TreeItem*>(parent.internalPointer())->columnCount();
    else
        return rootItem->columnCount();
}

QVariant TreeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    if (role != Qt::DisplayRole)
        return QVariant();

    TreeItem *item = static_cast<TreeItem*>(index.internalPointer());

    return item->data(index.column());
}



Qt::ItemFlags TreeModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return 0;

    return QAbstractItemModel::flags(index);
}



QVariant TreeModel::headerData(int section, Qt::Orientation orientation,
                               int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
        return rootItem->data(section);

    return QVariant();
}



QModelIndex TreeModel::index(int row, int column, const QModelIndex &parent)
            const
{
    if (!hasIndex(row, column, parent))
        return QModelIndex();

    TreeItem *parentItem;

    if (!parent.isValid())
        parentItem = rootItem;
    else
        parentItem = static_cast<TreeItem*>(parent.internalPointer());

    TreeItem *childItem = parentItem->child(row);
    if (childItem)
        return createIndex(row, column, childItem);
    else
        return QModelIndex();
}



QModelIndex TreeModel::parent(const QModelIndex &index) const
{
    if (!index.isValid())
        return QModelIndex();

    TreeItem *childItem = static_cast<TreeItem*>(index.internalPointer());
    TreeItem *parentItem = childItem->parentItem();

    if (parentItem == rootItem)
        return QModelIndex();

    return createIndex(parentItem->row(), 0, parentItem);
}



int TreeModel::rowCount(const QModelIndex &parent) const
{
    TreeItem *parentItem;
    if (parent.column() > 0)
        return 0;

    if (!parent.isValid())
        parentItem = rootItem;
    else
        parentItem = static_cast<TreeItem*>(parent.internalPointer());

    return parentItem->childCount();
}


void TreeModel::setupModelData(const QStringList &lines, TreeItem *parent)
{
    QList<TreeItem*> parents;
    QList<int> indentations;
    parents << parent;
    indentations << 0;

    int number = 0;

    while (number < lines.count()) {
        int position = 0;
        while (position < lines[number].length()) {
            if (lines[number].at(position) != ' ')
                break;
            position++;
        }

        QString lineData = lines[number].mid(position).trimmed();

        if (!lineData.isEmpty()) {
            // Read the column data from the rest of the line.
            QStringList columnStrings = lineData.split("\t", QString::SkipEmptyParts);
            QList<QVariant> columnData;
            for (int column = 0; column < columnStrings.count(); ++column)
                columnData << columnStrings[column];

            if (position > indentations.last()) {
                // The last child of the current parent is now the new parent
                // unless the current parent has no children.

                if (parents.last()->childCount() > 0) {
                    parents << parents.last()->child(parents.last()->childCount()-1);
                    indentations << position;
                }
            } else {
                while (position < indentations.last() && parents.count() > 0) {
                    parents.pop_back();
                    indentations.pop_back();
                }
            }

            // Append a new item to the current parent's list of children.
//            qDebug() << columnData << " " << parents.last()->parentItem();
            parents.last()->appendChild(new TreeItem(columnData, parents.last()));
        }

        ++number;
    }
}
