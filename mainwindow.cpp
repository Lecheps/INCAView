#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <functional>
#include "sshInterface.h"
#include "sqlhandler/parameterserialization.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    remoteUsername_ = "";
    serverAddress_ = "";
    remoteDBpath_ = "";

    //ui->pushSave->setEnabled(false);
    //ui->pushSaveAs->setEnabled(false);
    ui->pushRun->setEnabled(false);
    //ui->pushConnect->setEnabled(false);

    ui->radioButtonDaily->click();
    ui->radioButtonDaily->setEnabled(false);
    ui->radioButtonMonthlyAverages->setEnabled(false);
    ui->radioButtonYearlyAverages->setEnabled(false);

    QObject::connect(ui->radioButtonDaily, &QRadioButton::clicked, this, &MainWindow::updateGraphsAndResultSummary);
    QObject::connect(ui->radioButtonMonthlyAverages, &QRadioButton::clicked, this, &MainWindow::updateGraphsAndResultSummary);
    QObject::connect(ui->radioButtonYearlyAverages, &QRadioButton::clicked, this, &MainWindow::updateGraphsAndResultSummary);

    resetWindowTitle();

    //NOTE: the lineeditdelegate is used by the tableviewparameters to provide an input widget when editing parameter values.
    lineEditDelegate = new LineEditDelegate();
    ui->tableViewParameters->setItemDelegateForColumn(1, lineEditDelegate);
    ui->tableViewParameters->verticalHeader()->hide();
    ui->tableViewParameters->setEditTriggers(QAbstractItemView::AllEditTriggers);

    //NOTE: we override the ctrl-c functionality in order to copy the table view correctly.
    //So anything that should be copyable to the clipboard has to be explicitly handled in MainWindow::copyToClipboard.
    QAction *ctrlc = new QAction("copy");
    ctrlc->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_C));
    QObject::connect(ctrlc, &QAction::triggered, this, &MainWindow::copyToClipboard);
    ui->centralWidget->addAction(ctrlc);

    QAction *ctrlz = new QAction("undo");
    ctrlz->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_Z));
    QObject::connect(ctrlz, &QAction::triggered, this, &MainWindow::undo);
    ui->centralWidget->addAction(ctrlz);

    //The graph colors are used to select colors for graphs in the widgetPlotResults. Used in MainWindow::updateGraphsAndResultSummary
    graphColors_ = {{0, 130, 200}, {230, 25, 75}, {60, 180, 75}, {245, 130, 48}, {145, 30, 180},
                    {70, 240, 240}, {240, 50, 230}, {210, 245, 60}, {250, 190, 190}, {0, 128, 128}, {230, 190, 255},
                    {170, 110, 40}, {128, 0, 0}, {170, 255, 195}, {128, 128, 0}, {255, 215, 180}, {0, 0, 128}, {255, 225, 25}};

    QLocale::setDefault(QLocale::English);
    ui->widgetPlotResults->setLocale(QLocale::English);
    ui->widgetPlotResults->setInteraction(QCP::iRangeDrag, true);
    ui->widgetPlotResults->setInteraction(QCP::iRangeZoom, true);
    ui->widgetPlotResults->axisRect(0)->setRangeDrag(Qt::Horizontal);
    ui->widgetPlotResults->axisRect(0)->setRangeZoom(Qt::Horizontal);

    QObject::connect(ui->widgetPlotResults, QCustomPlot::mouseMove, this, &MainWindow::updateGraphToolTip);

    qDebug(QDir::currentPath().toLatin1().data());

}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::resetWindowTitle()
{
    QString titlepath = QString("%1@%2/%3").arg(remoteUsername_).arg(serverAddress_).arg(remoteDBpath_);
    if(ssh.isSessionConnected() && stuffHasBeenEditedSinceLastSave)
    {
        setWindowTitle("*" + titlepath + " - INCA View");
        return;
    }
    else if(ssh.isSessionConnected())
    {
        setWindowTitle(titlepath + " - INCA View");
        return;
    }
    setWindowTitle("INCA View");
}

void MainWindow::on_pushConnect_clicked()
{
    if(!ssh.isSessionConnected())
    {
        ui->pushConnect->setEnabled(false);

        //TODO: Query the user about these
        serverAddress_ = "35.197.231.4";
        remoteUsername_ = "magnus";
        remoteDBpath_ = "test.db";
        const char *keypath = "C:\\testkeys\\testkey";
        bool success = ssh.connectSession(remoteUsername_, serverAddress_, keypath);

        if(success)
        {
            ui->pushRun->setEnabled(true);

            editUndoStack_.clear();

            //ui->pushConnect->setEnabled(true);
            //ui->pushSaveAs->setEnabled(true);
            //ui->pushRun->setEnabled(true);

            ui->radioButtonDaily->setEnabled(true);
            ui->radioButtonMonthlyAverages->setEnabled(true);
            ui->radioButtonYearlyAverages->setEnabled(true);

            //loadedDBPath_ = pathToDB;

            //copyAndOverwriteFile(loadedDBPath_, tempWorkingDBPath);
            //setDBPath(tempWorkingDBPath);

            resetWindowTitle();

            parameterModel_ = new ParameterModel();
            treeParameters_ = new TreeModel("Parameter Structure");
            treeResults_ = new TreeModel("Results Structure");
            populateParameterModels(treeParameters_, parameterModel_);
            populateTreeModelResults(treeResults_);

            ui->tableViewParameters->setModel(parameterModel_);
            ui->treeViewParameters->setModel(treeParameters_);
            ui->treeViewParameters->expandToDepth(3);
            ui->treeViewParameters->setColumnHidden(1, true);
            ui->treeViewParameters->resizeColumnToContents(0);
            ui->treeViewResults->setModel(treeResults_);
            ui->treeViewResults->expandToDepth(1);
            ui->treeViewResults->resizeColumnToContents(0);
            ui->treeViewResults->setColumnHidden(1, true);
            ui->treeViewResults->setSelectionMode(QTreeView::ExtendedSelection); // Allows to ctrl-select multiple items.

            QObject::connect(parameterModel_, &ParameterModel::parameterWasEdited, this, &MainWindow::parameterWasEdited);
            QObject::connect(ui->treeViewParameters->selectionModel(), &QItemSelectionModel::selectionChanged, this, &MainWindow::updateParameterView);
            QObject::connect(ui->treeViewResults->selectionModel(), &QItemSelectionModel::selectionChanged, this, &MainWindow::updateGraphsAndResultSummary);

            //NOTE: this is to mark that the x axis is uninitialized so that it can be initialized later. TODO: is there a better way to do it?
            ui->widgetPlotResults->xAxis->setRange(0.0, 0.1);

            //updateGraphsAndResultSummary();
        }
        else
        {
            ui->pushConnect->setEnabled(true);
            //TODO: error handling.
        }
    }
}

/*
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
                    delete parameterModel_;

                    ui->widgetPlotResults->clearGraphs();
                    ui->widgetPlotResults->replot();
                }

                stuffHasBeenEditedSinceLastSave = false;
                editUndoStack_.clear();

                //ui->pushConnect->setEnabled(true);
                //ui->pushSaveAs->setEnabled(true);
                //ui->pushRun->setEnabled(true);

                ui->radioButtonDaily->setEnabled(true);
                ui->radioButtonMonthlyAverages->setEnabled(true);
                ui->radioButtonYearlyAverages->setEnabled(true);


                //loadedDBPath_ = pathToDB;

                //copyAndOverwriteFile(loadedDBPath_, tempWorkingDBPath);
                //setDBPath(tempWorkingDBPath);

                resetWindowTitle();

                parameterModel_ = new ParameterModel();
                treeParameters_ = new TreeModel("Parameter Structure");
                treeResults_ = new TreeModel("Results Structure");
                populateParameterModels(treeParameters_, parameterModel_);
                populateTreeModelResults(treeResults_);

                ui->tableViewParameters->setModel(parameterModel_);
                ui->treeViewParameters->setModel(treeParameters_);
                ui->treeViewParameters->expandToDepth(3);
                ui->treeViewParameters->setColumnHidden(1, true);
                ui->treeViewParameters->resizeColumnToContents(0);
                ui->treeViewResults->setModel(treeResults_);
                ui->treeViewResults->expandToDepth(1);
                ui->treeViewResults->resizeColumnToContents(0);
                ui->treeViewResults->setColumnHidden(1, true);
                ui->treeViewResults->setSelectionMode(QTreeView::ExtendedSelection); // Allows to ctrl-select multiple items.

                QObject::connect(parameterModel_, &ParameterModel::parameterWasEdited, this, &MainWindow::parameterWasEdited);
                QObject::connect(ui->treeViewParameters->selectionModel(), &QItemSelectionModel::selectionChanged, this, &MainWindow::updateParameterView);
                QObject::connect(ui->treeViewResults->selectionModel(), &QItemSelectionModel::selectionChanged, this, &MainWindow::updateGraphsAndResultSummary);

                //NOTE: this is to mark that the x axis is uninitialized so that it can be initialized later. TODO: is there a better way to do it?
                ui->widgetPlotResults->xAxis->setRange(0.0, 0.1);
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
*/

/*
void MainWindow::on_pushSave_clicked()
{
    if(pathToDBIsSet()) // Just for safety. The button is disabled in this case.
    {
        if(saveCheckParameters())
        {
            tryToSave(tempWorkingDBPath, loadedDBPath_);
        }
    }
}

void MainWindow::on_pushSaveAs_clicked()
{
    if(pathToDBIsSet()) // Just for safety. The button is disabled in this case.
    {
        if(saveCheckParameters())
        {
            QString pathToSave = QFileDialog::getSaveFileName(this, tr("Select location to store a backup copy of the database"), "c:/Users/Magnus/Documents/INCAView/", tr("Database files (*.db)"));

            if(!pathToSave.isEmpty()&& !pathToSave.isNull()) //In case the user clicks cancel, or replies no to an overwrite query.
            {
                if(tryToSave(tempWorkingDBPath, pathToSave))
                {
                    loadedDBPath_ = pathToSave;
                    resetWindowTitle();
                }
            }
        }
    }
}

bool MainWindow::saveCheckParameters()
{
    bool reallySave = false;

    if(parameterModel_->areAllParametersInRange())
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
            reallySave = false;
        }
    }

    return reallySave;
}


bool MainWindow::tryToSave(const QString& oldpath, const QString& newpath)
{
    bool saveWorked = copyAndOverwriteFile(oldpath, newpath);
    if(saveWorked)
    {
        toggleStuffHasBeenEditedSinceLastSave(false);
        editUndoStack_.clear();
    }
    else
    {
        QMessageBox::warning(this, "Save failed", "The operating system failed to save the file. Check if the file is in use by another program.", QMessageBox::Ok);
    }
    return saveWorked;
}
*/

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
        if(!success)
        {
            qDebug("failed to copy");
            qDebug(oldpath.toLatin1().data());
            qDebug(newpath.toLatin1().data());
        }
    }
    else
    {
        qDebug("failed to delete");
        qDebug(newpath.toLatin1().data());
    }

    return success;
}


void MainWindow::on_pushRun_clicked()
{
    if(ssh.isSessionConnected())
    {
        if(parameterModel_->areAllParametersInRange())
        {
            runINCA();
        }
        else
        {
            QMessageBox msgBox(QMessageBox::Warning, tr("Invalid parameters"), tr("Not all parameters are in the [min, max] range. This may cause INCA to crash. Run the model anyway?"));
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
    //Serialize parameters and send them to the remote database
    QVector<parameter_serial_entry> parameterdata;
    parameterModel_->serializeParameterData(parameterdata);
    ssh.writeParameterValues(remoteDBpath_, parameterdata);
    //TODO: actually run INCA

    updateGraphsAndResultSummary();
}

void MainWindow::updateParameterView(const QItemSelection& selected, const QItemSelection& deselected)
{
    parameterModel_->clearVisibleParameters();

    QModelIndexList indexes = selected.indexes();
    if(indexes.count() >= 1)
    {
        // The selection mode for this view is so that we can only select one row at the time. The first item in indexes points to the name, the second to the ID.
        QModelIndex index = indexes[1];
        int ID = treeParameters_->data(index).toInt();

        parameterModel_->setChildrenVisible(ID);
    }
}

void MainWindow::updateGraphsAndResultSummary()
{
    if(ssh.isSessionConnected())
    {
        ui->widgetPlotResults->clearGraphs();
        ui->widgetPlotResults->yAxis->setRange(0, 2.0*QCPRange::minRange); //Find a better way to clear it?
        ui->textResultsInfo->clear();

        QModelIndexList indexes = ui->treeViewResults->selectionModel()->selectedIndexes();

        QVector<int> IDs;
        for(auto index : indexes)
        {
            auto idx = index.model()->index(index.row(),index.column() + 1, index.parent());
            int ID = (treeParameters_->itemData(idx))[0].toInt();
            if(index.column() == 0 && ID != 0)
            {
                IDs.push_back(ID);
            }
        }

        int firstunassignedcolor = 0;
        if(IDs.count() && ssh.isSessionConnected())
        {
            QVector<QVector<double>> resultsets;
            ssh.getResultSets(IDs, resultsets);
            Q_ASSERT(resultsets.count() == IDs.count());

            for(int i = 0; i < IDs.count(); ++i)
            {
                int ID = IDs[i];
                QVector<double>& yval = resultsets[i];
                int cnt = yval.count();
                QVector<double> xval(cnt);

                //qDebug(QString::number(cnt).toLatin1().data());

                //TODO: Currently times are just for debugging!!! Should read actual times from a different source.
                auto startdate = QDateTime::currentDateTime();
                startdate = QDateTime(startdate.date()); //Remove hours, minutes, seconds
                uint starttime = startdate.toTime_t();
                double min = std::numeric_limits<double>::max();
                double max = std::numeric_limits<double>::min();

                for(int j = 0; j < cnt; ++j)
                {
                    double value = yval[j];
                    //qDebug(QString::number(value).toLatin1().data());
                    xval[j] = (double)(starttime + 24*3600*j);
                    min = value < min ? value : min;
                    max = value > max ? value : max;
                }

                if(cnt != 0)
                {
                    QString name = treeResults_->getName(ID);
                    QString parentName = treeResults_->getParentName(ID);
                    QColor& color = graphColors_[firstunassignedcolor++];
                    if(firstunassignedcolor == graphColors_.count()) firstunassignedcolor = 0; // Cycle the colors

                    double mean = 0;
                    for(double d : yval) mean += d;
                    mean /= (double)cnt;
                    double stddev = 0;
                    for(double d : yval) stddev += (d - mean)*(d - mean);
                    stddev = std::sqrt(stddev / (double) cnt);

                    ui->textResultsInfo->append(QString(
                            "%1 (%2) <font color=%3>&#9608;&#9608;</font><br/>" //NOTE: this is somewhat reliant on the font on the local system having the character &#9608; (FULL BLOCK)
                            "min: %4<br/>"
                            "max: %5<br/>"
                            "average: %6<br/>"
                            "standard deviation: %7<br/>"
                            "<br/>"
                          ).arg(name, parentName, color.name())
                           .arg(min, 0, 'g', 5)                          //TODO: should this be moved below to display the min of yearly averages when yearly averages are selected?
                           .arg(max, 0, 'g', 5)
                           .arg(mean, 0, 'g', 5)
                           .arg(stddev, 0, 'g', 5)
                    );

                    QCPGraph* graph = ui->widgetPlotResults->addGraph();
                    graph->setPen(QPen(color));

                    if(ui->radioButtonYearlyAverages->isChecked())
                    {
                        QVector<double> displayedx, displayedy;
                        min = std::numeric_limits<double>::max();
                        max = std::numeric_limits<double>::min();

                        QDateTime date = QDateTime::fromTime_t(starttime);
                        int prevyear = date.date().year();
                        double sum = 0;
                        int dayscnt = 0;
                        for(int j = 0; j < cnt; ++j)
                        {
                            sum += yval[j];
                            dayscnt++;
                            int curyear = date.date().year();
                            if(curyear != prevyear)
                            {
                                double value = sum / (double) dayscnt;
                                displayedy.push_back(value);
                                displayedx.push_back(QDateTime(QDate(prevyear, 1, 1)).toTime_t());
                                min = value < min ? value : min;
                                max = value > max ? value : max;

                                sum = 0;
                                dayscnt = 0;
                                prevyear = curyear;
                            }
                            date = date.addDays(1);
                        }
                        QSharedPointer<QCPAxisTickerDateTime> dateTicker(new QCPAxisTickerDateTime);
                        dateTicker->setDateTimeFormat("yyyy");
                        ui->widgetPlotResults->xAxis->setTicker(dateTicker);

                        graph->setData(displayedx, displayedy);
                    }
                    else if(ui->radioButtonMonthlyAverages->isChecked())
                    {
                        QVector<double> displayedx, displayedy;
                        min = std::numeric_limits<double>::max();
                        max = std::numeric_limits<double>::min();

                        QDateTime date = QDateTime::fromTime_t(starttime);
                        int prevmonth = date.date().month();
                        int prevyear = date.date().year();
                        double sum = 0;
                        int dayscnt = 0;
                        for(int j = 0; j < cnt; ++j)
                        {
                            sum += yval[j];
                            dayscnt++;
                            int curmonth = date.date().month();
                            if(curmonth != prevmonth)
                            {
                                double value = sum / (double) dayscnt;
                                displayedy.push_back(value);
                                displayedx.push_back(QDateTime(QDate(prevyear, prevmonth, 1)).toTime_t());
                                min = value < min ? value : min;
                                max = value > max ? value : max;

                                sum = 0;
                                dayscnt = 0;
                                prevmonth = curmonth;
                                prevyear = date.date().year();
                            }
                            date = date.addDays(1);
                        }
                        QSharedPointer<QCPAxisTickerDateTime> dateTicker(new QCPAxisTickerDateTime);
                        dateTicker->setDateTimeFormat("MMMM\nyyyy");
                        ui->widgetPlotResults->xAxis->setTicker(dateTicker);

                        graph->setData(displayedx, displayedy);
                    }
                    else
                    {
                        QSharedPointer<QCPAxisTickerDateTime> dateTicker(new QCPAxisTickerDateTime);
                        dateTicker->setDateTimeFormat("d. MMMM\nyyyy");
                        ui->widgetPlotResults->xAxis->setTicker(dateTicker);

                        graph->setData(xval, yval);
                    }

                    QCPRange existingYRange = ui->widgetPlotResults->yAxis->range();
                    double newmax = existingYRange.upper < max ? max : existingYRange.upper;
                    double newmin = existingYRange.lower > min ? min : existingYRange.lower;
                    if(newmax - newmin < QCPRange::minRange)
                    {
                        newmax = newmin + 2.0*QCPRange::minRange;
                    }
                    ui->widgetPlotResults->yAxis->setRange(newmin, newmax);

                    if(ui->widgetPlotResults->xAxis->range().upper - ui->widgetPlotResults->xAxis->range().lower < 1.0) //TODO: is there a better way to check if it is uninitialized?
                        ui->widgetPlotResults->xAxis->setRange(xval.first(), xval.last());

                }
            }
        }

        ui->widgetPlotResults->replot();
        updateGraphToolTip(0); // Clear the graph tooltip.
    }
}


void MainWindow::updateGraphToolTip(QMouseEvent *event)
{
    //NOTE: this is for changing the labelGraphValues label to print the values of the visible graphs when the mouse hovers over them. Not really a tooltip. Better name?
    if(ssh.isSessionConnected())
    {
        if(event && ui->widgetPlotResults->graphCount() > 0)
        {
            uint x = (uint)ui->widgetPlotResults->xAxis->pixelToCoord(event->pos().x());

            QModelIndexList indexes = ui->treeViewResults->selectionModel()->selectedIndexes();
            QString valueString= "";
            bool first = true;
            for(int i = 0; i < ui->widgetPlotResults->graphCount(); ++i)
            {
                if(first) first = false;
                else valueString.append("\n");

                QCPGraph *graph = ui->widgetPlotResults->graph(i);
                double value = graph->data()->findBegin((double)x)->value;

                int ID = indexes[2*i + 1].data().toInt();
                valueString.append(treeResults_->getName(ID)).append(" (").append(treeResults_->getParentName(ID)).append("): ");

                valueString.append(QString::number(value, 'g', 5));
            }

            QDateTime date = QDateTime::fromTime_t(x);
            QString dateString;
            if(ui->radioButtonYearlyAverages->isChecked())
                dateString = QLocale().toString(date, "yyyy").append(" yearly average");
            else if(ui->radioButtonMonthlyAverages->isChecked())
                dateString = QLocale().toString(date, "MMMM yyyy").append(" monthly average");
            else
                dateString = QLocale().toString(date, "d. MMMM yyyy");

            ui->labelGraphValues->setText(QString("%1:\n%2").arg(dateString).arg(valueString));
        }
        else
        {
            ui->labelGraphValues->setText("Date:\nValues:");
        }
    }
    else
    {
        ui->labelGraphValues->setText("Not connected to the cloud.");
    }
}

void MainWindow::populateParameterModels(TreeModel* treemodel, ParameterModel *parametermodel)
{
    if(ssh.isSessionConnected())
    {

        std::map<uint32_t, parameter_min_max_val_serial_entry> IDtoParam;
        ssh.getParameterValuesMinMax(remoteDBpath_, IDtoParam);

        QVector<TreeData> structuredata;
        ssh.getParameterStructure(remoteDBpath_, structuredata);
        for(TreeData& item : structuredata)
        {
            auto parref = IDtoParam.find(item.ID);
            if(parref == IDtoParam.end())
            {
                //This ID corresponds to something that does not have a value, and so we add it to the tree structure.
                treemodel->addItem(item);
            }
            else
            {
                //This ID corresponds to something that has a value, and so we add it to the parameter model.
                parameter_min_max_val_serial_entry& par = parref->second;
                parametermodel->addParameter(item.name, item.ID, item.parentID, par);
            }
        }
    }
}

void MainWindow::populateTreeModelResults(TreeModel* model)
{
    QVector<TreeData> resultstreedata;
    ssh.getResultsStructure(remoteDBpath_, resultstreedata);
    for(TreeData& item : resultstreedata)
    {
        model->addItem(item);
    }
}

void MainWindow::parameterWasEdited(ParameterEditAction param)
{
    /*
    connectDB();
    QSqlQuery query;
    query.prepare( "UPDATE "
                   " ParameterValues "
                   " SET "
                   " value = :val"
                    " WHERE "
                    " ID = :id ;");
    query.bindValue(":id", param.parameterID);
    query.bindValue(":val", param.newValue.getValueDBString());
    query.exec();
     disconnectDB();
     */

    editUndoStack_.push_back(param);

    toggleStuffHasBeenEditedSinceLastSave(true);
}


void MainWindow::toggleStuffHasBeenEditedSinceLastSave(bool newval)
{
    if(stuffHasBeenEditedSinceLastSave && !newval)
    {
        stuffHasBeenEditedSinceLastSave = false;
        //ui->pushSave->setEnabled(false);
    }

    if(!stuffHasBeenEditedSinceLastSave && newval)
    {
        stuffHasBeenEditedSinceLastSave = true;
        //ui->pushSave->setEnabled(true);
    }
    resetWindowTitle();
}

void MainWindow::copyToClipboard(bool checked)
{
    //NOTE: we override the ctrl-c functionality in order to copy the table view correctly. So anything that should be copyable to the clipboard has to be explicitly handled here.

    if(parameterModel_ && ui->tableViewParameters->hasFocus())
    {
        QModelIndexList selected = ui->tableViewParameters->selectionModel()->selectedIndexes();
        if(selected.count() > 0)
        {
            QString selectedText;
            QModelIndex previous = selected.first();
            selected.removeFirst();
            for (auto next: selected)
            {
                QVariant data = parameterModel_->data(previous);
                QString text = data.toString();
                selectedText.append(text);
                if(next.row() != previous.row()) selectedText.append('\n');
                else selectedText.append('\t');
                previous = next;
            }
            QVariant data = parameterModel_->data(previous);
            QString text = data.toString();
            selectedText.append(text);
            QApplication::clipboard()->setText(selectedText);
        }
    }
    else if(ui->textResultsInfo->hasFocus())
    {
        QApplication::clipboard()->setText(ui->textResultsInfo->textCursor().selectedText());
    }
}

void MainWindow::undo(bool checked)
{   
    if(editUndoStack_.count() > 0)
    {
        ParameterEditAction param = editUndoStack_.last();
        editUndoStack_.pop_back();
        parameterModel_->setValue(param.parameterID, param.oldValue);

        /*
        connectDB();
        QSqlQuery query;
        query.prepare( "UPDATE "
                       " ParameterValues "
                       " SET "
                       " value = :val"
                        " WHERE "
                        " ID = :id ;");
        query.bindValue(":id", param.parameterID);
        query.bindValue(":val", param.oldValue.getValueDBString());
        query.exec();
        disconnectDB();
        */
    }
}
