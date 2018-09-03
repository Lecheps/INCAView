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

    ui->lineEditUsername->setText("magnus"); //TODO: last login name should maybe be stored in a file

    treeParameters_ = 0;
    treeResults_ = 0;
    parameterModel_ = 0;

    ui->radioButtonDaily->click();

    toggleWeExpectToBeConnected(false);

    QObject::connect(ui->radioButtonDaily, &QRadioButton::clicked, this, &MainWindow::updateGraphsAndResultSummary);
    QObject::connect(ui->radioButtonMonthlyAverages, &QRadioButton::clicked, this, &MainWindow::updateGraphsAndResultSummary);
    QObject::connect(ui->radioButtonYearlyAverages, &QRadioButton::clicked, this, &MainWindow::updateGraphsAndResultSummary);
    QObject::connect(ui->radioButtonErrors, &QRadioButton::clicked, this, &MainWindow::updateGraphsAndResultSummary);
    QObject::connect(ui->radioButtonErrorHistogram, &QRadioButton::clicked, this, &MainWindow::updateGraphsAndResultSummary);
    QObject::connect(ui->radioButtonErrorNormalProbability, &QRadioButton::clicked, this, &MainWindow::updateGraphsAndResultSummary);

    //NOTE: the lineeditdelegate is used by the tableviewparameters to provide an input widget when editing parameter values.
    lineEditDelegate = new ParameterEditDelegate();
    ui->tableViewParameters->setItemDelegateForColumn(1, lineEditDelegate);
    ui->tableViewParameters->verticalHeader()->hide();
    ui->tableViewParameters->setEditTriggers(QAbstractItemView::AllEditTriggers);

    ui->pushSaveParameters->setEnabled(false);

    ui->treeViewResults->setSelectionMode(QTreeView::ExtendedSelection); // Allows to ctrl-select multiple items.

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

    QLocale::setDefault(QLocale::English);
    ui->widgetPlotResults->setLocale(QLocale::English);
    ui->widgetPlotResults->setInteraction(QCP::iRangeDrag, true);
    ui->widgetPlotResults->setInteraction(QCP::iRangeZoom, true);
    ui->widgetPlotResults->axisRect(0)->setRangeDrag(Qt::Horizontal);
    ui->widgetPlotResults->axisRect(0)->setRangeZoom(Qt::Horizontal);
    //NOTE: If we want a rectangle zoom for the plot, look at http://www.qcustomplot.com/index.php/support/forum/227

    QObject::connect(ui->widgetPlotResults, &QCustomPlot::mouseMove, this, &MainWindow::updateGraphToolTip);

    sshInterface_ = new SSHInterface("35.198.76.72", "magnus", "hubkey"); //NOTE: We have to think about whether the login info for the hub should be hard coded.

    QObject::connect(sshInterface_, &SSHInterface::log, this, &MainWindow::log);
    QObject::connect(sshInterface_, &SSHInterface::logError, this, &MainWindow::logSSHError);
    QObject::connect(sshInterface_, &SSHInterface::runINCAFinished, this, &MainWindow::onRunINCAFinished);
    QObject::connect(sshInterface_, &SSHInterface::runINCAError, this, &MainWindow::handleRunINCAError);

    ui->progressBarRunInca->setVisible(false);

    plotter_ = new Plotter(ui->widgetPlotResults, ui->textResultsInfo);


}

MainWindow::~MainWindow()
{
    delete ui;
    delete plotter_;
    delete sshInterface_;
}

void MainWindow::log(const QString& Message)
{
    ui->textLog->append(QTime::currentTime().toString("hh:mm:  ") + Message);
    QApplication::processEvents();
}

void MainWindow::logError(const QString& Message)
{
    ui->tabWidget->setCurrentIndex(1);
    log("<font color=red>" + Message + "</font>");
}


void MainWindow::logSSHError(const QString& message)
{
    logError(message);

    //TODO: We may want to do this regularly in some other way instead. It is not too reliable to do it here.. Maybe put it on a regular timer that is called once in a while.
    if(weExpectToBeConnected_ && !sshInterface_->isInstanceConnected())
    {
        const char *disconnectionMessage = sshInterface_->getDisconnectionMessage();
        logError(QString("SSH Disconnected:") + disconnectionMessage);
        handleInvoluntarySSHDisconnect();
    }
}

void MainWindow::resetWindowTitle()
{
    //TODO: this should reflect both connection status and what db we have loaded.
    if(weExpectToBeConnected_)
    {
        QString titlepath = QString("%1/%2").arg(ui->lineEditUsername->text()).arg(selectedProjectDbPath_);
        if(parametersHaveBeenEditedSinceLastSave_)
        {
            setWindowTitle("*" + titlepath + " - INCAView");
            return;
        }
        else
        {
            setWindowTitle(titlepath + " - INCAView");
            return;
        }
    }
    setWindowTitle("INCA View");
}

void MainWindow::on_pushConnect_clicked()
{
    //NOTE: Even if the disabling of these elements are handled by toggleWeExpectToBeConnected below, we want to do them here too since
    // we don't want them to be enabled for the half second the connection takes.
    ui->pushConnect->setEnabled(false);
    ui->lineEditUsername->setEnabled(false);

    QByteArray username = ui->lineEditUsername->text().toLatin1();

    //TODO: Check that username is a single word in lower caps or with '-'.

    QString instancename = QString("incaview-") + username.data();
    QByteArray instancename2 = instancename.toLatin1();

    log(QString("Attempting to create a google compute instance for ") + username.data() + ". This may take a few seconds ...");

    //bool success = sshInterface_->connectSession(name.data(), ip.data(), keyPath_);
    bool success = sshInterface_->createInstance(username.data(), instancename2.data());

    if(success)
    {
        log("Connection successful");

        toggleWeExpectToBeConnected(true);
    }
    else
    {
        //NOTE: SSH errors are reported to the log elsewhere.

        toggleWeExpectToBeConnected(false);
    }
}


void MainWindow::on_pushLoadProject_clicked()
{
    QString fileName = QFileDialog::getOpenFileName(this,
        tr("Open project database"), "", tr("Database files (*.db)"));
    //TODO: test that the user did not click cancel etc.

    bool success = projectDb_.setDatabase(fileName);
    selectedProjectDbPath_ = fileName;
    if(success)
    {
        loadParameterData();

        ui->pushLoadProject->setEnabled(false); //TODO: we should have a way of closing a project database in order to open a new one.

        ui->treeViewParameters->expandToDepth(3);
        ui->treeViewParameters->resizeColumnToContents(0);
        ui->treeViewParameters->setColumnHidden(1, true);

        if(sshInterface_->isInstanceConnected()) ui->pushRun->setEnabled(true);

        toggleParametersHaveBeenEditedSinceLastSave(false);

        resetWindowTitle();
    }
    else
    {
        //TODO: error handling
    }

}

void MainWindow::loadParameterData()
{
    if(projectDb_.databaseIsSet())
    {
        log("Loading parameter structure...");

        parameterModel_ = new ParameterModel();
        treeParameters_ = new TreeModel("Parameter Structure");

        std::map<uint32_t, parameter_min_max_val_serial_entry> IDtoParam;
        projectDb_.getParameterValuesMinMax(IDtoParam);

        QVector<TreeData> structuredata;
        projectDb_.getParameterStructure(structuredata);
        for(TreeData& data : structuredata)
        {
            auto parref = IDtoParam.find(data.ID);
            if(parref == IDtoParam.end())
            {
                //This ID corresponds to something that does not have a value (i.e. an indexer or index), and so we add it to the tree structure.
                treeParameters_->addItem(data);
            }
            else
            {
                //This ID corresponds to something that has a value (i.e. a parameter), and so we add it to the parameter model.
                parameter_min_max_val_serial_entry& par = parref->second;
                parameterModel_->addParameter(data.name, data.ID, data.parentID, par);
            }
        }

        ui->tableViewParameters->setModel(parameterModel_);
        ui->treeViewParameters->setModel(treeParameters_);

        QObject::connect(parameterModel_, &ParameterModel::parameterWasEdited, this, &MainWindow::parameterWasEdited);
        QObject::connect(ui->treeViewParameters->selectionModel(), &QItemSelectionModel::selectionChanged, this, &MainWindow::updateParameterView);
        QObject::connect(ui->tableViewParameters, &QTableView::clicked, parameterModel_, &ParameterModel::handleClick);

        log("Loading complete");
    }
    else
    {
        logError("Tried to load parameter data without having a valid project database.");
    }
}

void MainWindow::loadResultStructure(const char *remotedbpath)
{
    if(treeResults_) delete treeResults_;

    treeResults_ = new TreeModel("Results Structure");

    QVector<TreeData> resultstreedata;
    sshInterface_->getResultsStructure(remotedbpath, resultstreedata);
    for(TreeData& item : resultstreedata)
    {
        treeResults_->addItem(item);
    }

    ui->treeViewResults->setModel(treeResults_);

    QObject::connect(ui->treeViewResults->selectionModel(), &QItemSelectionModel::selectionChanged, this, &MainWindow::updateGraphsAndResultSummary);
}

void MainWindow::toggleWeExpectToBeConnected(bool connected)
{
    weExpectToBeConnected_ = connected;

    if(connected)
    {
        ui->pushConnect->setEnabled(false);
        ui->lineEditUsername->setEnabled(false);
        ui->pushDisconnect->setEnabled(true);
        if(projectDb_.databaseIsSet()) ui->pushRun->setEnabled(true);
    }
    else
    {
        ui->pushConnect->setEnabled(true);
        ui->lineEditUsername->setEnabled(true);
        ui->pushDisconnect->setEnabled(false);
        ui->pushRun->setEnabled(false);
        ui->radioButtonDaily->setEnabled(false);
        ui->radioButtonMonthlyAverages->setEnabled(false);
        ui->radioButtonYearlyAverages->setEnabled(false);
        ui->radioButtonErrors->setEnabled(false);
        ui->radioButtonErrorHistogram->setEnabled(false);
        ui->radioButtonErrorNormalProbability->setEnabled(false);
    }
    resetWindowTitle();
}


void MainWindow::on_pushDisconnect_clicked()
{
    bool success = sshInterface_->destroyInstance();
    //TODO: If we were not successful destroying the instance, what do we do?

    toggleWeExpectToBeConnected(false);

    if(treeResults_) delete treeResults_; //NOTE: The destructor of the QAbstractItemModel automatically disconnects it from it's view.
    treeResults_ = 0;

    clearGraphsAndResultSummary();
}


void MainWindow::handleInvoluntarySSHDisconnect()
{
    toggleWeExpectToBeConnected(false);

    if(treeResults_) delete treeResults_; //NOTE: The destructor of the QAbstractItemModel automatically disconnects it from it's view.
    treeResults_ = 0;

    clearGraphsAndResultSummary();

    logError("We were disconnected from the SSH connection.");
}


void MainWindow::on_pushSaveParameters_clicked()
{
    if(parametersHaveBeenEditedSinceLastSave_)
    {
        log("Saving parameters...");

        //NOTE: Serialize parameter values and send them to the remote database
        QVector<parameter_serial_entry> parameterdata;
        parameterModel_->serializeParameterData(parameterdata); //TODO: we should probably only save the parameters that have been changed instead of all of them..

        bool success = projectDb_.writeParameterValues(parameterdata);
        if(success)
        {
            toggleParametersHaveBeenEditedSinceLastSave(false);

            editUndoStack_.clear();

            log("Saving parameters complete.");
        }
        else
        {
            //TODO: We should probably do some error handling here.
        }
    }
}


void MainWindow::on_pushRun_clicked()
{
    if(weExpectToBeConnected_) //NOTE: The button should also be disabled in this case. This check is just for safety
    {
        if(sshInterface_->isInstanceConnected())
        {
            if(parameterModel_->areAllParametersInRange())
            {
                runModel();
            }
            else
            {
                QMessageBox msgBox(QMessageBox::Warning, tr("Invalid parameters"), tr("Not all parameter values are in the [Min, Max] range. This may cause the model to crash. Run the model anyway?"));
                QPushButton *runButton = msgBox.addButton(tr("Run model"), QMessageBox::ActionRole);
                QPushButton *abortButton = msgBox.addButton(QMessageBox::Cancel);

                msgBox.exec();

                if (msgBox.clickedButton() == runButton) {
                    runModel();
                } else if (msgBox.clickedButton() == abortButton) {
                    // NOTE: Do nothing.
                }
            }
        }
        else
        {
            handleInvoluntarySSHDisconnect();
        }
    }
}

void MainWindow::runModel()
{
    if(!projectDb_.databaseIsSet())
    {
        //TODO: error (though it shoudl not be possible to reach this state)
        return;
    }

    if(weExpectToBeConnected_)
    {
        if(!sshInterface_->isInstanceConnected())
        {
            handleInvoluntarySSHDisconnect();
            return;
        }
    }
    else
    {
        //TODO: error (though it shoudl not be possible to reach this state)
        return;
    }


    ui->pushRun->setEnabled(false);

    log("Attempting to run INCA...");

    on_pushSaveParameters_clicked(); //NOTE: Save the parameters to the database.

    //TODO: Upload the entire parameter database to the instance!
    const char *remoteParameterDbName = "parameters.db";

    QByteArray dbpath = selectedProjectDbPath_.toLatin1();
    sshInterface_->uploadEntireFile(dbpath.data(), "~/", remoteParameterDbName);

    if(parameterModel_->timestepsLoaded_)
    {
        //TODO: We should retrieve the latest edited value for timesteps here (or update it in the parametermodel when it receives an edit).
        ui->progressBarRunInca->setMaximum(parameterModel_->timesteps_);
    }
    else
    {
        //TODO: What do we do?
    }

    const char *remoteexepath = "bla.exe"; //TODO: This should probably be stored and read from the local database

    //TODO: We probably should send info about what path we want for the remote result db?
    //sshInterface_->runModel(remoteexepath, remoteParameterDbName, ui->progressBarRunInca);
}

void MainWindow::onRunINCAFinished()
{
    //NOTE: this is a slot that receives a signal from the sshInterface when it is complete.

    log("INCA run finished.");

    const char *remoteResultDbPath = "results.db";

    //NOTE: TODO: This may not be correct: In the future, the result structure may have changed after a new model run.
    if(!treeResults_)
    {
        loadResultStructure(remoteResultDbPath);
    }

    updateGraphsAndResultSummary(); //In case somebody had a graph selected, it is updated with a plot of the data generated from the last run.
    ui->pushRun->setEnabled(true);
}

void MainWindow::handleRunINCAError(const QString& message)
{
    //NOTE: this is a slot that receives a signal from the sshInterface.

    logError(message);

    ui->pushRun->setEnabled(true);
    //TODO: other cleanup?
}

void MainWindow::closeEvent (QCloseEvent *event)
{
    if(parametersHaveBeenEditedSinceLastSave_)
    {
        QMessageBox::StandardButton resBtn = QMessageBox::question( this, tr("Closing INCAView without saving."),
                                                                    tr("If you exit INCAView without saving the parameters or running the model, your changes to the parameters will not be stored. Do you still want to exit?\n"),
                                                                    QMessageBox::Yes | QMessageBox::No ,
                                                                    QMessageBox::Yes);
        if (resBtn != QMessageBox::Yes) {
            event->ignore();
        } else {
            event->accept();
        }
    }

    bool success = sshInterface_->destroyInstance();
    //TODO: If we were not successful destroying the instance, what do we do?
}

void MainWindow::updateParameterView(const QItemSelection& selected, const QItemSelection& deselected)
{
    parameterModel_->clearVisibleParameters();

    QModelIndexList indexes = selected.indexes();
    if(indexes.count() >= 1)
    {
        // NOTE: The selection mode for this view is configured so that we can only select one row at the time.
        //    The first item in indexes points to the name, the second to the ID.
        QModelIndex index = indexes[1];
        int ID = treeParameters_->data(index).toInt();

        parameterModel_->setChildrenVisible(ID);
    }
}

void MainWindow::updateGraphsAndResultSummary()
{
    if(weExpectToBeConnected_)
    {
        if(sshInterface_->isInstanceConnected())
        {
            QModelIndexList indexes = ui->treeViewResults->selectionModel()->selectedIndexes();

            QVector<QString> resultNames;
            QVector<int> IDs;
            for(auto index : indexes)
            {
                if(index.column() == 0)
                {
                    auto idx = index.model()->index(index.row(),index.column() + 1, index.parent());
                    int ID = (treeResults_->itemData(idx))[0].toInt();
                    if( ID != 0 && treeResults_->childCount(ID) == 0) //NOTE: If it has children in the tree, it is an indexer or index, not a result series.
                    {
                        IDs.push_back(ID);
                    }
                    QString name = treeResults_->getName(ID);
                    QString parentName = treeResults_->getParentName(ID);
                    resultNames.push_back(name + " (" + parentName + ")");
                }
            }

            if(IDs.count())
            {
                PlotMode mode = PlotMode_Daily;
                if(ui->radioButtonMonthlyAverages->isChecked()) mode = PlotMode_MonthlyAverages;
                else if(ui->radioButtonYearlyAverages->isChecked()) mode = PlotMode_YearlyAverages;
                else if(ui->radioButtonErrors->isChecked()) mode = PlotMode_Error;
                else if(ui->radioButtonErrorHistogram->isChecked()) mode = PlotMode_ErrorHistogram;
                else if(ui->radioButtonErrorNormalProbability->isChecked()) mode = PlotMode_ErrorNormalProbability;

                QVector<int> uncachedIDs;
                plotter_->whichIDsAreNotCached(IDs, uncachedIDs);

                QVector<QVector<double>> resultsets;
                //TODO: Formalize the paths to the remote databases in some way so that they are not just spread around in the code here.
                const char *remoteResultDbPath = "results.db";
                bool success = sshInterface_->getResultSets(remoteResultDbPath, uncachedIDs, resultsets);

                if(success)
                {
                    QDateTime date;
                    if(parameterModel_->startDateLoaded_)
                    {
                        //TODO: This is not reliable! A different start date could have been saved as a parameter since the last run.
                        // instead we should load the dates from the result set.
                        int64_t starttime = parameterModel_->startDate_;
                        date = QDateTime::fromSecsSinceEpoch(starttime);
                    }
                    else
                    {
                        //NOTE: Do we really want to use the current time here, or should we do something else to the x axis?
                        date = QDateTime::currentDateTime();
                    }

                    plotter_->plotGraphs(IDs, resultNames, resultsets, uncachedIDs, mode, date);
                }
            }
            updateGraphToolTip(0);
        }
        else
        {
            handleInvoluntarySSHDisconnect();
        }
    }
}


void MainWindow::updateGraphToolTip(QMouseEvent *event)
{
    //NOTE: this is for changing the labelGraphValues label to print the values of the visible graphs when the mouse hovers over them.
    // It is not really a tooltip. Better name?

    if(event && ui->widgetPlotResults->graphCount() > 0)
    {
        double x = ui->widgetPlotResults->xAxis->pixelToCoord(event->pos().x());

        QModelIndexList indexes = ui->treeViewResults->selectionModel()->selectedIndexes();
        QString valueString= "";
        bool first = true;
        for(int i = 0; i < ui->widgetPlotResults->graphCount(); ++i)
        {
            if(first) first = false;
            else valueString.append("\n");

            QCPGraph *graph = ui->widgetPlotResults->graph(i);
            double value = graph->data()->findBegin(x)->value;

            int ID = indexes[2*i + 1].data().toInt();
            if(ui->radioButtonErrors->isChecked())
            {
                if(i == 0) valueString.append("Error: ");
                else valueString.append("Linearly regressed error: ");
            }
            else if(ui->radioButtonDaily->isChecked() || ui->radioButtonMonthlyAverages->isChecked() || ui->radioButtonYearlyAverages->isChecked())
            {
                valueString.append(treeResults_->getName(ID)).append(" (").append(treeResults_->getParentName(ID)).append("): ");
            }

            bool foundrange;
            QCPRange range = graph->getKeyRange(foundrange);
            if(foundrange && range.contains(x))
                valueString.append(QString::number(value, 'g', 5));
            else
                valueString.append("Date out of range");
        }

        QDateTime date = QDateTime::fromSecsSinceEpoch((int64_t)x);
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


void MainWindow::clearGraphsAndResultSummary()
{
    ui->widgetPlotResults->clearGraphs();
    ui->widgetPlotResults->replot();
    updateGraphToolTip(0);
    ui->textResultsInfo->clear();
}


void MainWindow::parameterWasEdited(ParameterEditAction param)
{
    editUndoStack_.push_back(param);

    toggleParametersHaveBeenEditedSinceLastSave(true);
}


void MainWindow::toggleParametersHaveBeenEditedSinceLastSave(bool changed)
{
    parametersHaveBeenEditedSinceLastSave_ = changed;

    if(changed)
    {
        ui->pushSaveParameters->setEnabled(true);
    }
    else
    {
        ui->pushSaveParameters->setEnabled(false);
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
    else if(ui->textLog->hasFocus())
    {
        QApplication::clipboard()->setText(ui->textLog->textCursor().selectedText());
    }
}

void MainWindow::undo(bool checked)
{   
    if(editUndoStack_.count() > 0)
    {
        ParameterEditAction param = editUndoStack_.last();
        editUndoStack_.pop_back();
        parameterModel_->setValue(param.parameterID, param.oldValue);
    }
}

