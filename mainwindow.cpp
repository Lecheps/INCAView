#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    dbIsLoaded_ = false;

    ui->setupUi(this);
    tabParameterLayout_ = new QGridLayout;//new QGridLayout;//QFormLayout;QVBoxLayout;
    ui->listViewParameters->setLayout(tabParameterLayout_);

    ui->pushSave->setEnabled(false);
    ui->pushRun->setEnabled(false);

    this->setWindowTitle("INCA view");
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_pushLoad_clicked()
{
    if(dbIsLoaded_)
    {
        //TODO: cleanup any old data and do what is needed to stop stuff from breaking.
    }
    ui->pushSave->setEnabled(true);
    ui->pushRun->setEnabled(true);

    pathToDB_ = QFileDialog::getOpenFileName(this,tr("Select output database"),"c:/Users/Magnus/Documents/INCAView/test.db",tr("Database files (*.db)"));
    dbIsLoaded_ = true;
    populateLayoutMap(tabParameterLayout_);
    treeParameters_ = new TreeModel();
    treeResults_ = new TreeModel(true);

    ui->treeView->setModel(treeParameters_);
    ui->treeView->expandToDepth(3);
    //ui->treeView->setColumnHidden(1,TRUE);
    ui->treeView->resizeColumnToContents(0);
    ui->treeViewResults->setModel(treeResults_);
    ui->treeViewResults->expandToDepth(1);
    ui->treeViewResults->resizeColumnToContents(0);
}

void MainWindow::on_pushSave_clicked()
{
    if(dbIsLoaded_) // Just for safety. The button is disabled in this case.
    {
        QString pathToSave = QFileDialog::getSaveFileName(this, tr("Select location to store a backup copy of the database"), "c:/Users/Magnus/Documents/INCAView/", tr("Database files (*.db)"));

        QFile::copy(pathToDB_, pathToSave);
    }
}

void MainWindow::on_pushRun_clicked()
{
    if(dbIsLoaded_) // Just for safety. The button is disabled in this case.
    {
        bool validValues = true;
        for(auto& key_value : layoutMap_)
        {
            if(!key_value.second.valueIsValidAndInRange)
            {
                validValues = false;
                break;
            }
        }
        if(validValues)
        {
            runINCA();
        }
        else
        {
            QMessageBox msgBox(QMessageBox::Warning, tr("Invalid parameters"), tr("Not all parameters are set with valid values. Run with most recent valid values?"));
            QPushButton *runButton = msgBox.addButton(tr("Run model"), QMessageBox::ActionRole);
            QPushButton *abortButton = msgBox.addButton(QMessageBox::Cancel);

            msgBox.exec();

            if (msgBox.clickedButton() == runButton) {
                runINCA();
            } else if (msgBox.clickedButton() == abortButton) {
                // abort
            }
        }
    }
}

void MainWindow::runINCA()
{
    //TODO: implement
}

void MainWindow::on_treeView_clicked(const QModelIndex &index)
{
    for (auto & ID : itemsInGrid_)
    {
        layoutMap_[ID].setVisible(false);
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
        layoutMap_[ID].setVisible(true);
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

    //qDebug() << ID << " -> " << query.size();
    //qDebug() << query.size();

    //TODO: Currently times are just for debugging!!! Should read actual times from database.
    double starttime = QDateTime::currentDateTime().toTime_t();
    QVector<QCPGraphData> timeData(0);

    //QVector<double> x,y;
    int cnt = 0;
    double min = std::numeric_limits<double>::max();
    double max = std::numeric_limits<double>::min();
    while (query.next())
    {
        double value = query.value(0).toDouble();

        QCPGraphData entry(starttime + 24*3600*(cnt++), value);

        timeData.append(entry);

        //x.append(cnt++);
        //y.append(value);
        min = value < min ? value : min;
        max = value > max ? value : max;
    }
    db_.close();

    if(max - min < QCPRange::minRange)
    {
        max = min + 2.0*QCPRange::minRange;
    }
    //qDebug() << "Min: " << min << "Max: " << max;

    // create graph and assign data to it:
    ui->widgetPlot->addGraph();
    //ui->widgetPlot->graph(0)->setData(timeData);
    ui->widgetPlot->graph(0)->data()->set(timeData);
    //ui->widgetPlot->xAxis->setRange(0,cnt);

    QSharedPointer<QCPAxisTickerDateTime> dateTicker(new QCPAxisTickerDateTime);
    dateTicker->setDateTimeFormat("d. MMMM\nyyyy");
    ui->widgetPlot->xAxis->setTicker(dateTicker);

    ui->widgetPlot->yAxis->setRange(min,max);
    ui->widgetPlot->xAxis->setRange(starttime, starttime+24*3600*(cnt-1));

    ui->widgetPlot->replot();

    //ui->tabWidget->setCurrentIndex(1);

}
