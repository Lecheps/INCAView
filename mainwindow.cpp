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

    ui->lineEditUsername->setText("magnus"); //TODO: last login name should maybe be stored in a file. It should at least not be hard coded to "magnus"

    treeParameters_ = nullptr;
    treeResults_ = nullptr;
    parameterModel_ = nullptr;

    ui->radioButtonDaily->click();

    setWeExpectToBeConnected(false);

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

    ui->pushCreateDatabase->setEnabled(false);

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

    //NOTE: We have to think about whether the login info for the hub should be hard coded.
    //NOTE: The hub ssh keys have to be distributed with the exe and be placed in the same folder as the exe.
    sshInterface_ = new SSHInterface("35.198.76.72", "magnus", "hubkey");

    QObject::connect(sshInterface_, &SSHInterface::log, this, &MainWindow::log);
    QObject::connect(sshInterface_, &SSHInterface::logError, this, &MainWindow::logSSHError);
    QObject::connect(sshInterface_, &SSHInterface::runINCAFinished, this, &MainWindow::onRunINCAFinished);
    QObject::connect(sshInterface_, &SSHInterface::runINCAError, this, &MainWindow::handleRunINCAError);

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
    QString dbState = selectedProjectDbPath_;
    if(parametersHaveBeenEditedSinceLastSave_) dbState = "*" + dbState;

    QString loginState = "";
    QString username =  ui->lineEditUsername->text();
    if(weExpectToBeConnected_) loginState = " - " + username;

    QString title = QString("%1%2 INCAView").arg(dbState).arg(loginState);

    setWindowTitle(title);
}

void MainWindow::on_pushConnect_clicked()
{
    //NOTE: Even if the disabling of these elements are handled by toggleWeExpectToBeConnected below, we want to do them here too since
    // we don't want them to be enabled for the half second the connection takes.
    ui->pushConnect->setEnabled(false);
    ui->lineEditUsername->setEnabled(false);

    QByteArray username = ui->lineEditUsername->text().toLatin1();

    //TODO: Check that username is a single word in lower caps or with '-'. If that is not always possible, we should generate the instance name in a different way

    QString instancename = QString("incaview-") + username.data();
    QByteArray instancename2 = instancename.toLatin1();

    log(QString("Attempting to get a google compute instance for ") + username.data());

    //bool success = sshInterface_->connectSession(name.data(), ip.data(), keyPath_);
    bool success = sshInterface_->createInstance(username.data(), instancename2.data());

    if(success)
    {
        log("Connection successful");

        setWeExpectToBeConnected(true);
    }
    else
    {
        //NOTE: SSH errors are reported to the log elsewhere.

        setWeExpectToBeConnected(false);
    }
}


void MainWindow::on_pushLoadProject_clicked()
{
    QString fileName = QFileDialog::getOpenFileName(this,
        tr("Open project database"), "", tr("Database files (*.db)"));

    if(!fileName.isEmpty() && !fileName.isNull())     //NOTE: in case the user clicked cancel.
    {
        loadParameterDatabase(fileName);
    }
}

void MainWindow::loadParameterDatabase(QString fileName)
{
    if(projectDb_.databaseIsSet())
    {
        if(parametersHaveBeenEditedSinceLastSave_)
        {
            //NOTE: Alternatively we could just save the parameters without asking?
            QMessageBox::StandardButton resBtn = QMessageBox::question( this, tr("Loading a new database without saving."),
                tr("If you load a new database without saving the parameters or running the model, your changes to the parameters will not be stored. Do you still want to load a new database?\n"),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::Yes);

            if (resBtn != QMessageBox::Yes) return;
        }

        if(treeResults_) delete treeResults_;
        treeResults_ = nullptr;

        updateGraphsAndResultSummary();
    }

    bool success = projectDb_.setDatabase(fileName);
    if(success)
    {
        loadParameterData();

        selectedProjectDbPath_ = fileName;

        ui->treeViewParameters->expandToDepth(3);
        ui->treeViewParameters->resizeColumnToContents(0);
        ui->treeViewParameters->setColumnHidden(1, true);

        if(sshInterface_->isInstanceConnected()) ui->pushRun->setEnabled(true);

        toggleParametersHaveBeenEditedSinceLastSave(false);

        resetWindowTitle();
    }
    else
    {
        ui->pushRun->setEnabled(false);
        //TODO: error handling
    }
}

void MainWindow::on_pushCreateDatabase_clicked()
{
    if(!weExpectToBeConnected_)
    {
        //NOTE: This should not be possible. The button should not be active in that case.
        return;
    }
    if(!sshInterface_->isInstanceConnected())
    {
        handleInvoluntarySSHDisconnect();
        return;
    }

    QString fileName = QFileDialog::getOpenFileName(this,
        tr("Select parameter file to convert"), "", tr("Data files (*.dat)"));  //TODO: should not restrict it to .dat

    if(fileName.isEmpty() || fileName.isNull()) //NOTE: In case the user clicked cancel etc.
    {
        return;
    }

    const char *remoteParameterFileName = "parameters.dat";

    QString exepath = ui->lineEditModelname->text();
    QByteArray exepath2 = exepath.toLatin1();

    QByteArray filename2 = fileName.toLatin1();
    bool success = sshInterface_->uploadEntireFile(filename2.data(), "~/", remoteParameterFileName);

    if(!success) return;

    success = sshInterface_->createParameterDatabase(remoteParameterFileName, exepath2.data());

    if(!success) return;

    QString saveFileName = QFileDialog::getSaveFileName(this,
        tr("Select location to store database file"), "", tr("Database files (*.db)"));

    if(fileName.isEmpty() || fileName.isNull()) //NOTE: In case the user clicked cancel etc.
    {
        return;
    }

    QByteArray saveFileName2 = saveFileName.toLatin1();

    //TODO: Don't hard code the location of the remote parameter database file?
    success = sshInterface_->downloadEntireFile(saveFileName2.data(), "parameters.db");

    if(!success) return;

    loadParameterDatabase(saveFileName);
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
            auto parref = IDtoParam.find(data.ID); //NOTE: See if there is a parameter with this ID.
            if(parref == IDtoParam.end())
            {
                //This ID corresponds to something that is not a parameter (i.e. an indexer, and index or a root node), and so we add it to the tree structure.
                treeParameters_->addItem(data);
            }
            else
            {
                //This ID corresponds to a parameter, and so we add it to the parameter model.
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
    QVector<TreeData> resultstreedata;
    bool success = sshInterface_->getResultsStructure(remotedbpath, resultstreedata);
    if(!success) return;

    if(treeResults_) delete treeResults_;

    treeResults_ = new TreeModel("Results Structure");

    for(TreeData& item : resultstreedata)
    {
        treeResults_->addItem(item);
    }

    ui->treeViewResults->setModel(treeResults_);

    QObject::connect(ui->treeViewResults->selectionModel(), &QItemSelectionModel::selectionChanged, this, &MainWindow::updateGraphsAndResultSummary);

    //TODO: The following should be done somewhere else?

    ui->radioButtonDaily->setEnabled(true);
    ui->radioButtonMonthlyAverages->setEnabled(true);
    ui->radioButtonYearlyAverages->setEnabled(true);
    ui->radioButtonErrors->setEnabled(true);
    ui->radioButtonErrorHistogram->setEnabled(true);
    ui->radioButtonErrorNormalProbability->setEnabled(true);

    ui->treeViewResults->expandToDepth(3);
    ui->treeViewResults->resizeColumnToContents(0);
    ui->treeViewResults->setColumnHidden(1, true);
}

void MainWindow::setWeExpectToBeConnected(bool connected)
{
    weExpectToBeConnected_ = connected;

    if(connected)
    {
        ui->pushConnect->setEnabled(false);
        ui->lineEditUsername->setEnabled(false);
        ui->pushDisconnect->setEnabled(true);
        if(projectDb_.databaseIsSet()) ui->pushRun->setEnabled(true);
        ui->pushCreateDatabase->setEnabled(true);
    }
    else
    {
        ui->pushConnect->setEnabled(true);
        ui->lineEditUsername->setEnabled(true);
        ui->pushDisconnect->setEnabled(false);
        ui->pushRun->setEnabled(false);
        ui->pushCreateDatabase->setEnabled(false);
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

    setWeExpectToBeConnected(false);

    if(treeResults_) delete treeResults_; //NOTE: The destructor of the QAbstractItemModel automatically disconnects it from it's view.
    treeResults_ = 0;

    clearGraphsAndResultSummary();
}


void MainWindow::handleInvoluntarySSHDisconnect()
{
    //TODO: Should we attempt to destroy the compute instance?
    // OR it would probably be better to attempt to reconnect to it?


    setWeExpectToBeConnected(false);

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
        //TODO: we should probably only save the parameters that have been changed instead of all of them.. However this operation is very fast, so it doesn't seem to matter.
        parameterModel_->serializeParameterData(parameterdata);

        bool success = projectDb_.writeParameterValues(parameterdata);
        if(success)
        {
            toggleParametersHaveBeenEditedSinceLastSave(false);

            editUndoStack_.clear();

            log("Saving parameters complete.");
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
        //TODO: error (though it should not be possible to reach this state)
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
        //TODO: error (though it should not be possible to reach this state)
        return;
    }


    ui->pushRun->setEnabled(false);

    log("Attempting to run Model...");

    on_pushSaveParameters_clicked(); //NOTE: Save the parameters to the database.

    //TODO: Upload the entire parameter database to the instance!
    const char *remoteParameterDbName = "parameters.db";

    QByteArray dbpath = selectedProjectDbPath_.toLatin1();
    sshInterface_->uploadEntireFile(dbpath.data(), "~/", remoteParameterDbName);

    QString exepath = ui->lineEditModelname->text();
    QByteArray exepath2 = exepath.toLatin1();

    //TODO: We probably should send info about what path we want for the remote result db. For now this is hard coded.
    sshInterface_->runModel(exepath2.data(), remoteParameterDbName);
}

void MainWindow::onRunINCAFinished()
{
    //NOTE: this is a slot that responds to a signal from the sshInterface when the model run is complete.

    log("Model run and result output finished.");

    const char *remoteResultDbPath = "results.db";

    //NOTE: TODO: This may not be correct: In the future, the result structure may have changed after a new model run if we allow changing indexes in editor or allow uploading input files.
    if(!treeResults_)
    {
        loadResultStructure(remoteResultDbPath);
    }

    plotter_->clearCache();

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
        //NOTE: Alternatively we could just save the parameters without asking?
        QMessageBox::StandardButton resBtn = QMessageBox::question( this, tr("Closing INCAView without saving."),
                                                                    tr("If you exit INCAView without saving the parameters or running the model, your changes to the parameters will not be stored. Do you still want to exit?\n"),
                                                                    QMessageBox::Yes | QMessageBox::No ,
                                                                    QMessageBox::Yes);
        if (resBtn != QMessageBox::Yes) {
            event->ignore();
        } else {
            bool success = sshInterface_->destroyInstance(); //TODO: If we were not successful destroying the instance, what do we do?
            event->accept();
        }
    }
    else
    {
        bool success = sshInterface_->destroyInstance(); //TODO: If we were not successful destroying the instance, what do we do?
    }
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
    if(!weExpectToBeConnected_)
    {
        //NOTE: Should not be possible to reach this state.
        return;
    }

    if(!sshInterface_->isInstanceConnected())
    {
        handleInvoluntarySSHDisconnect();
        return;
    }

    if(!treeResults_)
    {
        clearGraphsAndResultSummary();
        return;
    }

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

    if(!IDs.empty())
    {
        PlotMode mode = PlotMode_Daily;
        if(ui->radioButtonMonthlyAverages->isChecked()) mode = PlotMode_MonthlyAverages;
        else if(ui->radioButtonYearlyAverages->isChecked()) mode = PlotMode_YearlyAverages;
        else if(ui->radioButtonErrors->isChecked()) mode = PlotMode_Error;
        else if(ui->radioButtonErrorHistogram->isChecked()) mode = PlotMode_ErrorHistogram;
        else if(ui->radioButtonErrorNormalProbability->isChecked()) mode = PlotMode_ErrorNormalProbability;

        QVector<int> uncachedIDs;
        plotter_->filterUncachedIDs(IDs, uncachedIDs);

        bool success = true;
        if(!uncachedIDs.empty())
        {
            //TODO: Formalize the paths to the remote databases in some way so that they are not just scattered around in the code.
            const char *remoteResultDbPath = "results.db";
            QVector<QVector<double>> resultsets;
            success = sshInterface_->getResultSets(remoteResultDbPath, uncachedIDs, resultsets);
            plotter_->addToCache(uncachedIDs, resultsets);
        }

        if(success)
        {
           //NOTE: Using the currentDateTime is temporary. Instead we should load the dates from the result database
            QDateTime date = QDateTime::currentDateTime();
            plotter_->plotGraphs(IDs, resultNames, mode, date);
        }
    }
    else
    {
        plotter_->clearPlots();
    }

    updateGraphToolTip(nullptr);
}


void MainWindow::updateGraphToolTip(QMouseEvent *event)
{
    //NOTE: this is for changing the labelGraphValues label to print the values of the visible graphs when the mouse hovers over them.
    // It is not really a tooltip. Better name?

    if(!(ui->radioButtonErrorHistogram->isChecked() || ui->radioButtonErrorNormalProbability->isChecked()) 
            && ui->widgetPlotResults->graphCount() > 0)
    {
        double x = -100.0;
        if(event) x = ui->widgetPlotResults->xAxis->pixelToCoord(event->pos().x());

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
            {
                valueString.append("Cursor outside of graph");
            }
        }

        QDateTime date = QDateTime::fromSecsSinceEpoch((int64_t)x);
        QString dateString = "";

        if(ui->radioButtonYearlyAverages->isChecked())
            dateString = QLocale().toString(date, "yyyy").append(" yearly average");
        else if(ui->radioButtonMonthlyAverages->isChecked())
            dateString = QLocale().toString(date, "MMMM yyyy").append(" monthly average");
        else if(ui->radioButtonDaily->isChecked() || ui->radioButtonErrors->isChecked())
            dateString = QLocale().toString(date, "d. MMMM yyyy");


        ui->labelGraphValues->setText(QString("%1:\n%2").arg(dateString).arg(valueString));
    }
    else
    {
        ui->labelGraphValues->setText("");
    }
}


void MainWindow::clearGraphsAndResultSummary()
{
    plotter_->clearPlots();
    plotter_->clearCache();
    updateGraphToolTip(nullptr);
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

