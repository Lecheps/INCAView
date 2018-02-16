#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <functional>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    ui->pushSave->setEnabled(false);
    ui->pushSaveAs->setEnabled(false);
    ui->pushRun->setEnabled(false);

    this->setWindowTitle("INCA view");
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_pushLoad_clicked()
{

    QString pathToDB = QFileDialog::getOpenFileName(this,tr("Select output database"),"c:/Users/Magnus/Documents/INCAView/test.db",tr("Database files (*.db)"));

    if(!pathToDB.isEmpty()&& !pathToDB.isNull())
    {
        if(QFile::exists(pathToDB))
        {
            bool actuallyLoad = true;

            if(stuffHasBeenEditedSinceLastSave)
            {
                QMessageBox::StandardButton resBtn = QMessageBox::question( this, tr("Opening new database without saving."),
                                                                            tr("Do you want to open a new database without saving the changes made to the one that is open?\n"),
                                                                            QMessageBox::Yes | QMessageBox::No ,
                                                                            QMessageBox::Yes);
                if (resBtn == QMessageBox::Yes) {
                    actuallyLoad = true;
                } else {
                    actuallyLoad = false;
                }
            }

            if(actuallyLoad)
            {
                if(pathToDBIsSet())
                {
                    // A DB is already loaded. Remove the old loaded data before loading in the new one.
                    delete treeParameters_;
                    delete treeResults_;
                    delete parameterModel;
                }

                stuffHasBeenEditedSinceLastSave = false;

                ui->pushSaveAs->setEnabled(true);
                ui->pushRun->setEnabled(true);

                loadedDBPath_ = pathToDB;

                setWindowTitle("INCA view: " + loadedDBPath_);

                copyAndOverwriteFile(loadedDBPath_, tempWorkingDBPath);

                setDBPath(tempWorkingDBPath);

                parameterModel = new ParameterModel();
                populateParameterModel();
                ui->tableViewParameters->setModel(parameterModel);
                ui->tableViewParameters->verticalHeader()->hide();

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
        }
        else
        {
            QMessageBox msgBox(QMessageBox::Warning, tr("Nonexistent file"), tr("The file ") + pathToDB + tr(" does not exist."));
            msgBox.addButton(QMessageBox::Ok);
            msgBox.exec();
        }
    }
}

void MainWindow::on_pushSave_clicked()
{
    if(pathToDBIsSet()) // Just for safety. The button is disabled in this case.
    {
        tryToSave(tempWorkingDBPath, loadedDBPath_);
    }
}

void MainWindow::on_pushSaveAs_clicked()
{
    if(pathToDBIsSet()) // Just for safety. The button is disabled in this case.
    {
        QString pathToSave = QFileDialog::getSaveFileName(this, tr("Select location to store a backup copy of the database"), "c:/Users/Magnus/Documents/INCAView/", tr("Database files (*.db)"));

        if(!pathToSave.isEmpty()&& !pathToSave.isNull()) //In case the user cancel, or no to the overwrite question.
        {
            if(tryToSave(tempWorkingDBPath, pathToSave))
            {
                loadedDBPath_ = pathToSave;
                setWindowTitle("INCA view: " + loadedDBPath_);
            }
        }
    }
}


bool MainWindow::copyAndOverwriteFile(const QString& oldpath, const QString& newpath)
{
    bool success = true;
    if(QFile::exists(newpath))
    {
        success = QFile::remove(newpath);
    }
    if(success)
    {
        success = QFile::copy(oldpath, newpath);
    }
    return success;
}

bool MainWindow::tryToSave(const QString& oldpath, const QString& newpath)
{
    bool reallySave = false;

    if(parameterModel->areAllParametersValidAndInRange())
    {
        reallySave = true;
    }
    else
    {
        QMessageBox msgBox(QMessageBox::Warning, tr("Invalid parameters"), tr("Not all parameters values are in the [min, max] range. Save anyway?"));
        QPushButton *saveButton = msgBox.addButton(tr("Save"), QMessageBox::ActionRole);
        QPushButton *abortButton = msgBox.addButton(QMessageBox::Cancel);

        msgBox.exec();

        if (msgBox.clickedButton() == saveButton) {
            reallySave = true;
        } else if (msgBox.clickedButton() == abortButton) {
            // abort
        }
    }

    bool saveWorked = false;
    if(reallySave)
    {
        saveWorked = copyAndOverwriteFile(oldpath, newpath);

        if(saveWorked)
        {
            toggleStuffHasBeenEditedSinceLastSave(false);
        }
        else
        {
            QMessageBox::warning(this, "Save failed", "The operating system failed to save the file", QMessageBox::Ok);
        }
    }

    return saveWorked;
}


void MainWindow::on_pushRun_clicked()
{
    if(pathToDBIsSet()) // Just for safety. The button is disabled in this case.
    {
        if(parameterModel->areAllParametersValidAndInRange())
        {
            runINCA();
        }
        else
        {
            QMessageBox msgBox(QMessageBox::Warning, tr("Invalid parameters"), tr("Not all parameters are in the [min, max] range. Run the model anyway?"));
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

void MainWindow::closeEvent (QCloseEvent *event)
{
    if(stuffHasBeenEditedSinceLastSave)
    {
        QMessageBox::StandardButton resBtn = QMessageBox::question( this, tr("Closing INCA view without saving."),
                                                                    tr("Do you want to exit without saving the changes made to the database?\n"),
                                                                    QMessageBox::Yes | QMessageBox::No ,
                                                                    QMessageBox::Yes);
        if (resBtn != QMessageBox::Yes) {
            event->ignore();
        } else {
            event->accept();
        }
    }
}

void MainWindow::runINCA()
{
    //TODO: implement

    //Because the results part of the db is changed:
    //toggleStuffHasBeenEditedSinceLastSave(true);
}

void MainWindow::on_treeView_clicked(const QModelIndex &index)
{
    parameterModel->clearVisibleParameters();

    connectDB();
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
        parameterModel->setParameterVisible(ID);
    }

    ui->tableViewParameters->resizeColumnToContents(2);
    ui->tableViewParameters->resizeColumnToContents(3);

    disconnectDB();

}

void MainWindow::on_treeViewResults_clicked(const QModelIndex &index)
{
    connectDB();
    auto idx = index.model()->index(index.row(),index.column() + 1, index.parent());
    int ID = (treeParameters_->itemData(idx))[0].toInt();

    QSqlQuery query;
    query.prepare("SELECT value FROM Results "
                  "WHERE ID=:ID;"
                );
    query.bindValue(":ID",ID);
    query.exec();


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
    disconnectDB();

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
}

void MainWindow::populateParameterModel()
{
    connectDB();
    QSqlQuery query;
    query.prepare(   "SELECT "
                        "ParameterStructure.name,ParameterStructure.type,ParameterValues.* "
                    "FROM "
                    "    ParameterStructure INNER JOIN ParameterValues "
                    "    ON ParameterStructure.ID = ParameterValues.ID; "
                );
    query.exec();
    while (query.next())
    {
        parameterModel->addParameter(
                    query.value(2).toInt(), // ID
                    query.value(0).toString(), // name
                    query.value(1).toString(), // type
                    query.value(5).toString(), // value
                    query.value(3).toString(), // min
                    query.value(4).toString() // max
                    );
    }
    disconnectDB();

    QObject::connect(parameterModel, &ParameterModel::parameterWasEdited, this, &MainWindow::parameterWasEdited);
}

void MainWindow::parameterWasEdited(const QString& newValue, int dbID, bool inrange)
{
    connectDB();

    QSqlQuery query;
    query.prepare( "UPDATE "
                   " ParameterValues "
                   " SET "
                   " value = '" + newValue + "'"
                    " WHERE "
                    " ID = :id ;");
    //query.bindValue(":newvalue", newValue);
    query.bindValue(":id", dbID);
    query.exec();

    disconnectDB();

    toggleStuffHasBeenEditedSinceLastSave(true);
}


void MainWindow::toggleStuffHasBeenEditedSinceLastSave(bool newval)
{
    if(stuffHasBeenEditedSinceLastSave && !newval)
    {
        stuffHasBeenEditedSinceLastSave = false;
        ui->pushSave->setEnabled(false);
    }

    if(!stuffHasBeenEditedSinceLastSave && newval)
    {
        stuffHasBeenEditedSinceLastSave = true;
        ui->pushSave->setEnabled(true);
    }
}
