#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    ui->tabWidget->setTabText(0,"Parameters");
    ui->tabWidget->setTabText(1,"Results");
    tabParameterLayout_ = new QGridLayout;//new QGridLayout;//QFormLayout;QVBoxLayout;
    //ui->tab_Parameters->setLayout(tabParameterLayout_);
    ui->listViewParameters->setLayout(tabParameterLayout_);
    this->setWindowTitle("INCA view");
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_pushLoad_clicked()
{
    pathToDB_ = QFileDialog::getOpenFileName(this,tr("Select output database"),"/home/jose-luis/Documents/INCA/Netbeans_projects/Core_HBV",tr("Database files (*.db)"));
    populateLayoutMap(tabParameterLayout_);
    treeParameters_ = new TreeModel();
    treeResults_ = new TreeModel(true);
    ui->treeView->setModel(treeParameters_);
    ui->treeView->expandToDepth(1);
    ui->treeView->setColumnHidden(1,TRUE);
    ui->treeView->resizeColumnToContents(0);
    ui->treeViewResults->setModel(treeResults_);
    ui->treeViewResults->expandToDepth(1);

}



void MainWindow::on_pushResults_clicked()
{
    //When clicked, display the results database

}

void MainWindow::on_pushParameters_clicked()
{
    //When clicked, display the parameter structure

}


void MainWindow::on_treeView_clicked(const QModelIndex &index)
{
    for (auto & ID : itemsInGrid_)
    {
        boost::get<layoutForDouble>(layoutMap_[ID]).setVisible(false);
    }

    itemsInGrid_.clear();
    connectToDB();
    auto idx = index.model()->index(index.row(),index.column() + 1, index.parent());
    int ID = (treeParameters_->itemData(idx))[0].toInt();

    QSqlQuery query;
    query.prepare("select child.name, child.ID from ParameterStructure as parent, ParameterStructure as child "
                "where child.lft > parent.lft "
                "and child.rgt < parent.rgt "
                "and child.dpt = (parent.dpt + 1) "
                "and parent.id = :ID "
                "and child.type is not null;"
                );
    query.bindValue(":ID",ID);
    query.exec();

    while (query.next())
    {
        int ID = query.value(1).toInt();
        boost::get<layoutForDouble>(layoutMap_[ID]).setVisible(true);
        itemsInGrid_.push_back(ID);
    }
    db_.close();

}

void MainWindow::on_treeViewResults_clicked(const QModelIndex &index)
{
    connectToDB();
    auto idx = index.model()->index(index.row(),index.column() + 1, index.parent());
    int ID = (treeParameters_->itemData(idx))[0].toInt();

    QSqlQuery query;
    query.prepare("SELECT value FROM Results "
                  "WHERE ID=:ID;"
                );
    query.bindValue(":ID",ID);
    query.exec();

    qDebug() << ID << " -> " << query.size();
    //qDebug() << query.size();

    QVector<double> x,y;
    int cnt = 0;
    double min = std::numeric_limits<double>::max();
    double max = std::numeric_limits<double>::min();
    while (query.next())
    {
        double value = query.value(0).toDouble();
        x.append(cnt++);
        y.append(value);
        min = value < min ? value : min;
        max = value > max ? value : max;
    }
    db_.close();

    // create graph and assign data to it:
    ui->widgetPlot->addGraph();
    ui->widgetPlot->graph(0)->setData(x, y);
    ui->widgetPlot->xAxis->setRange(0,cnt);
    ui->widgetPlot->yAxis->setRange(min,max);

    ui->widgetPlot->replot();

}
