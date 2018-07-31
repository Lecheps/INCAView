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

    //TODO: This should not be hard coded, at least not to this path:
    keyPath_ = "C:\\testkeys\\magnusKey";

    ui->lineEditIP->setText("35.189.120.237");
    ui->lineEditUsername->setText("magnus");

    treeParameters_ = 0;
    treeResults_ = 0;
    parameterModel_ = 0;

    ui->radioButtonDaily->click();

    toggleWeExpectToBeConnected(false);

    QObject::connect(ui->radioButtonDaily, &QRadioButton::clicked, this, &MainWindow::updateGraphsAndResultSummary);
    QObject::connect(ui->radioButtonMonthlyAverages, &QRadioButton::clicked, this, &MainWindow::updateGraphsAndResultSummary);
    QObject::connect(ui->radioButtonYearlyAverages, &QRadioButton::clicked, this, &MainWindow::updateGraphsAndResultSummary);

    //NOTE: the lineeditdelegate is used by the tableviewparameters to provide an input widget when editing parameter values.
    lineEditDelegate = new ParameterEditDelegate();
    ui->tableViewParameters->setItemDelegateForColumn(1, lineEditDelegate);
    ui->tableViewParameters->verticalHeader()->hide();
    ui->tableViewParameters->setEditTriggers(QAbstractItemView::AllEditTriggers);

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
    //NOTE: If we want a rectangle zoom for the plot, look at http://www.qcustomplot.com/index.php/support/forum/227

    QObject::connect(ui->widgetPlotResults, QCustomPlot::mouseMove, this, &MainWindow::updateGraphToolTip);

    QObject::connect(&sshInterface_, &SSHInterface::log, this, &MainWindow::log);
    QObject::connect(&sshInterface_, &SSHInterface::logError, this, &MainWindow::logSSHError);
    QObject::connect(&sshInterface_, &SSHInterface::runINCAFinished, this, &MainWindow::onRunINCAFinished);
    QObject::connect(&sshInterface_, &SSHInterface::runINCAError, this, &MainWindow::handleRunINCAError);

    ui->progressBarRunInca->setVisible(false);

    ui->comboBoxSelectProject->addItem(tr("<None>"));

    QObject::connect(ui->comboBoxSelectProject, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::handleModelSelect);
}

MainWindow::~MainWindow()
{
    delete ui;
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
    if(weExpectToBeConnected_ && !sshInterface_.isSessionConnected())
    {
        const char *disconnectionMessage = sshInterface_.getDisconnectionMessage();
        logError(QString("SSH Disconnected:") + disconnectionMessage);
        handleInvoluntarySSHDisconnect();
    }
}

void MainWindow::resetWindowTitle()
{ 
    if(weExpectToBeConnected_)
    {
        QString dbpath = "";
        if(currentSelectedProject_ >= 0) dbpath = availableProjects_[currentSelectedProject_].databaseName;
        QString titlepath = QString("%1@%2:~/%3").arg(ui->lineEditUsername->text()).arg(ui->lineEditIP->text()).arg(dbpath);
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
    ui->lineEditIP->setEnabled(false);

    char namebuf[256];
    strcpy(namebuf, ui->lineEditUsername->text().toLatin1().data()); //Are there really no better ways to copy QString to char *?
    char ipbuf[256];
    strcpy(ipbuf, ui->lineEditIP->text().toLatin1().data());

    log(QString("Attempting to connect to ") + namebuf + "@" + ipbuf + " ...");

    bool success = sshInterface_.connectSession(namebuf, ipbuf, keyPath_);

    if(success)
    {
        log("Connection successful. Select a project.");

        //TODO: Load the list of projects from the server. Each project should be tied to a specific user. One should be able to create new projects from a list of models.
        if(!availableProjects_.count())
        {
            sshInterface_.getProjectList("projects.db", namebuf, availableProjects_);
        }

        ui->comboBoxSelectProject->clear();
        ui->comboBoxSelectProject->addItem(tr("<None>"));
        for(ProjectSpec &project : availableProjects_)
        {
            ui->comboBoxSelectProject->addItem(project.name);
        }

        toggleWeExpectToBeConnected(true);
    }
    else
    {
        //NOTE: SSH errors are reported to the log elsewhere.

        toggleWeExpectToBeConnected(false);
    }
}

void MainWindow::handleModelSelect(int index)
{
    if(weExpectToBeConnected_) //NOTE: The combo box should be disabled if we are not expecting to be connected, so this test is just to be safe.
    {
        if(sshInterface_.isSessionConnected())
        {
            if(index >= 0 && index <= availableProjects_.size())
            {
                //TODO: If we have edited parameters, we should query the user if they want to change model without saving parameters.

                //TODO: If we previously had an involuntary disconnect, we may want to keep the state of the models and allow the user to reconnect without reloading the state.
                if(treeParameters_) delete treeParameters_;
                if(treeResults_) delete treeResults_;
                if(parameterModel_) delete parameterModel_;
                treeParameters_ = 0;
                treeResults_ = 0;
                parameterModel_ = 0;

                toggleParametersHaveBeenEditedSinceLastSave(false);
                clearGraphsAndResultSummary();
                editUndoStack_.clear();

                currentSelectedProject_ = index-1; //NOTE: index=0 is the <None> option. So index=1 refers to element 0 of our model list.
                if(currentSelectedProject_ >= 0 && currentSelectedProject_ < availableProjects_.size())
                {
                    log("Loading project: " + availableProjects_[currentSelectedProject_].name);
                    loadModelData();

                    ui->treeViewParameters->expandToDepth(3);
                    ui->treeViewParameters->resizeColumnToContents(0);
                    ui->treeViewResults->expandToDepth(1);
                    ui->treeViewResults->resizeColumnToContents(0);
                    ui->treeViewParameters->setColumnHidden(1, true);
                    ui->treeViewResults->setColumnHidden(1, true);

                    ui->pushRun->setEnabled(true);

                    ui->radioButtonDaily->setEnabled(true);
                    ui->radioButtonMonthlyAverages->setEnabled(true);
                    ui->radioButtonYearlyAverages->setEnabled(true);
                }
                else
                {
                    //NOTE: We have selected the "<None>" option;
                    ui->pushRun->setEnabled(false);
                    ui->pushSaveParameters->setEnabled(false);

                    ui->radioButtonDaily->setEnabled(false);
                    ui->radioButtonMonthlyAverages->setEnabled(false);
                    ui->radioButtonYearlyAverages->setEnabled(false);
                }

                resetWindowTitle();
            }
        }
        else
        {
            handleInvoluntarySSHDisconnect();
        }
    }
}

void MainWindow::loadModelData()
{
    log("Loading parameter and results structures...");

    parameterModel_ = new ParameterModel();
    treeParameters_ = new TreeModel("Parameter Structure");
    treeResults_ = new TreeModel("Results Structure");

    //TODO: Just in case something goes wrong when loading from the remote database here, we should really do some more error handling.
    char dbpath[256];
    strcpy(dbpath, availableProjects_[currentSelectedProject_].databaseName.toLatin1().data());

    //NOTE: Loading in the results structure tree:
    QVector<TreeData> resultstreedata;
    sshInterface_.getResultsStructure(dbpath, resultstreedata);
    for(TreeData& item : resultstreedata)
    {
        treeResults_->addItem(item);
    }

    //NOTE: loading in the parameter structure tree and the parameter value data
    std::map<uint32_t, parameter_min_max_val_serial_entry> IDtoParam;
    sshInterface_.getParameterValuesMinMax(dbpath, IDtoParam);

    QVector<TreeData> structuredata;
    sshInterface_.getParameterStructure(dbpath, structuredata);
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
    ui->treeViewResults->setModel(treeResults_);

    QObject::connect(parameterModel_, &ParameterModel::parameterWasEdited, this, &MainWindow::parameterWasEdited);
    QObject::connect(ui->treeViewParameters->selectionModel(), &QItemSelectionModel::selectionChanged, this, &MainWindow::updateParameterView);
    QObject::connect(ui->treeViewResults->selectionModel(), &QItemSelectionModel::selectionChanged, this, &MainWindow::updateGraphsAndResultSummary);
    QObject::connect(ui->tableViewParameters, &QTableView::clicked, parameterModel_, &ParameterModel::handleClick);

    log("Loading complete");
}

void MainWindow::toggleWeExpectToBeConnected(bool connected)
{
    weExpectToBeConnected_ = connected;

    if(connected)
    {
        ui->pushConnect->setEnabled(false);
        ui->lineEditUsername->setEnabled(false);
        ui->lineEditIP->setEnabled(false);
        ui->pushDisconnect->setEnabled(true);

        ui->comboBoxSelectProject->setEnabled(true);
    }
    else
    {
        ui->pushConnect->setEnabled(true);
        ui->lineEditUsername->setEnabled(true);
        ui->lineEditIP->setEnabled(true);
        ui->pushDisconnect->setEnabled(false);
        ui->pushRun->setEnabled(false);
        ui->comboBoxSelectProject->setEnabled(false);
        ui->pushSaveParameters->setEnabled(false);
        ui->radioButtonDaily->setEnabled(false);
        ui->radioButtonMonthlyAverages->setEnabled(false);
        ui->radioButtonYearlyAverages->setEnabled(false);
    }
    resetWindowTitle();
}


void MainWindow::on_pushDisconnect_clicked()
{
    //TODO: If stuffHasBeenEditedSinceLastSave, query the user if they really want to disconnect without saving?

    sshInterface_.disconnectSession();

    toggleWeExpectToBeConnected(false);
    toggleParametersHaveBeenEditedSinceLastSave(false);

    if(treeParameters_) delete treeParameters_; //NOTE: The destructor of the QAbstractItemModel automatically disconnects it from it's view.
    if(treeResults_) delete treeResults_;
    if(parameterModel_) delete parameterModel_;
    treeParameters_ = 0;
    treeResults_ = 0;
    parameterModel_ = 0;

    clearGraphsAndResultSummary();
}


void MainWindow::handleInvoluntarySSHDisconnect()
{
    toggleWeExpectToBeConnected(false);

    logError("We were disconnected from the SSH connection.");

    //TODO: We need to decide what we want to to the state of the parameter model and tree models.
    // Note however that the involuntary disconnect is now less likely since we fixed the timeout bug, so this may not be a priority.
}


void MainWindow::on_pushSaveParameters_clicked()
{
    if(parametersHaveBeenEditedSinceLastSave_)
    {
        log("Saving parameters...");

        //NOTE: Serialize parameter values and send them to the remote database
        QVector<parameter_serial_entry> parameterdata;
        parameterModel_->serializeParameterData(parameterdata); //NOTE: we should probably only save the parameters that have been changed instead of all of them..
        //TODO: We should probably do some error handling here.
        char dbpath[256];
        strcpy(dbpath, availableProjects_[currentSelectedProject_].databaseName.toLatin1().data());
        sshInterface_.writeParameterValues(dbpath, parameterdata);
        toggleParametersHaveBeenEditedSinceLastSave(false);

        editUndoStack_.clear();

        log("Saving parameters complete."); //NOTE: Will look silly if there is an error in between...
    }
}


void MainWindow::on_pushRun_clicked()
{
    if(weExpectToBeConnected_) //NOTE: The button should also be disabled in this case. This check is just for safety
    {
        if(sshInterface_.isSessionConnected())
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
    ui->pushRun->setEnabled(false);

    log("Attempting to run INCA...");

    on_pushSaveParameters_clicked(); //NOTE: Save the parameters to the database.

    if(parameterModel_->timestepsLoaded_)
    {
        //TODO: Find out if the parameter called "Timesteps" is actually used to set up the number of timesteps when the model is run!
        //   Otherwise we need to get the correct number of timesteps in a different way.
        //TODO: We should retrieve the latest edited value for timesteps here (or update it in the parametermodel when it receives an edit).
        ui->progressBarRunInca->setMaximum(parameterModel_->timesteps_);
    }
    else
    {
        //TODO: What do we do?
    }

    char namebuf[256];
    strcpy(namebuf, ui->lineEditUsername->text().toLatin1().data()); //Are there really no better ways to convert QString to char *?
    char ipbuf[256];
    strcpy(ipbuf, ui->lineEditIP->text().toLatin1().data());
    char exebuf[256];
    if(currentSelectedProject_ >= 0 && currentSelectedProject_ < availableProjects_.size())
    {
        strcpy(exebuf, availableProjects_[currentSelectedProject_].exeName.toLatin1().data());
        sshInterface_.runModel(namebuf, ipbuf, keyPath_, exebuf, ui->progressBarRunInca);
    }
    else
    {
        logError("We got a command to run the model, even though no model was selected"); //NOTE: The way the ui is set up should prevent this from ever happening
    }
}

void MainWindow::onRunINCAFinished()
{
    //NOTE: this is a slot that receives a signal from the sshInterface.

    log("INCA run finished.");
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
        if(sshInterface_.isSessionConnected())
        {
            //NOTE: Right now we just throw out all previous graphs and re-create everything. We could keep track of which ID corresponds to which graph (in which plot mode)
            // and then only update/create the graphs that have changed. However, this should only be necessary if this routine runs very slowly on some user machines.
            ui->widgetPlotResults->clearGraphs();
            ui->widgetPlotResults->yAxis->setRange(0, 2.0*QCPRange::minRange); //NOTE: this is our way of "clearing" the range so that it is correctly set later. Could maybe find a better way.
            ui->textResultsInfo->clear();

            QModelIndexList indexes = ui->treeViewResults->selectionModel()->selectedIndexes();

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
                }
            }

            int firstunassignedcolor = 0;
            if(IDs.count())
            {
                QVector<QVector<double>> resultsets;
                char dbpath[256];
                strcpy(dbpath, availableProjects_[currentSelectedProject_].databaseName.toLatin1().data());
                bool success = sshInterface_.getResultSets(dbpath, IDs, resultsets);

                if(success)
                {
                    //qDebug() << "got data";
                    ui->widgetPlotResults->xAxis->setRange(0, 0); //NOTE: If we don't do this we may keep the max range of previous plots that are now unselected.

                    for(int i = 0; i < IDs.count(); ++i)
                    {
                        int ID = IDs[i];
                        QVector<double>& yval = resultsets[i];
                        int cnt = yval.count();
                        QVector<double> xval(cnt);

                        qDebug() << "data count: " << cnt;

                        int64_t starttime;
                        if(parameterModel_->startDateLoaded_)
                        {
                            //TODO: This is not reliable! A different start date could have been saved as a parameter since the last run.
                            // instead we should have date data in the result set.
                            starttime = parameterModel_->startDate_;
                        }
                        else
                        {
                            //NOTE: Do we really want to use the current time here, or should we do something else to the x axis?
                            QDateTime startdate = QDateTime::currentDateTime();
                            startdate = QDateTime(startdate.date()); //Remove hours, minutes, seconds
                            starttime = startdate.toSecsSinceEpoch();
                        }

                        double min = std::numeric_limits<double>::max();
                        double max = std::numeric_limits<double>::min();

                        for(int j = 0; j < cnt; ++j)
                        {
                            double value = yval[j];
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
                                    "%1 (%2) <font color=%3>&#9608;&#9608;</font><br/>" //NOTE: this is reliant on the font having the character &#9608;
                                    "min: %4<br/>"
                                    "max: %5<br/>"
                                    "average: %6<br/>"
                                    "standard deviation: %7<br/>"
                                    "<br/>"
                                  ).arg(name, parentName, color.name())
                                   .arg(min, 0, 'g', 5)
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

                                graph->setData(displayedx, displayedy, true);
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

                                graph->setData(displayedx, displayedy, true);
                            }
                            else
                            {
                                QSharedPointer<QCPAxisTickerDateTime> dateTicker(new QCPAxisTickerDateTime);
                                dateTicker->setDateTimeFormat("d. MMMM\nyyyy");
                                ui->widgetPlotResults->xAxis->setTicker(dateTicker);

                                graph->setData(xval, yval, true);
                            }

                            QCPRange existingYRange = ui->widgetPlotResults->yAxis->range();
                            double newmax = existingYRange.upper < max ? max : existingYRange.upper;
                            double newmin = existingYRange.lower > min ? min : existingYRange.lower;
                            if(newmax - newmin < QCPRange::minRange)
                            {
                                newmax = newmin + 2.0*QCPRange::minRange;
                            }
                            ui->widgetPlotResults->yAxis->setRange(newmin, newmax);
                            ui->widgetPlotResults->xAxis->setRange(xval.first(), xval.last());
                        }
                    }
                }
            }

            ui->widgetPlotResults->replot();
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
            valueString.append(treeResults_->getName(ID)).append(" (").append(treeResults_->getParentName(ID)).append("): ");

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

