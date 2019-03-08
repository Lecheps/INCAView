#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <functional>
#include "sshInterface.h"
#include "sqlhandler/serialization.h"
#include <fstream>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    std::ifstream usernamefile;
    usernamefile.open("lastusername.txt");
    if(usernamefile)
    {
        std::string username;
        usernamefile >> username;
        ui->lineEditUsername->setText(QString::fromStdString(username));
        usernamefile.close();
    }
    else
    {
        ui->lineEditUsername->setText("username");
    }

    treeParameters_ = nullptr;
    treeResults_ = nullptr;
    treeInputs_  = nullptr;
    parameterModel_ = nullptr;

    ui->radioButtonDaily->click();

    setWeExpectToBeConnected(false);

    QObject::connect(ui->radioButtonDaily, &QRadioButton::clicked, this, &MainWindow::updateGraphsAndResultSummary);
    QObject::connect(ui->radioButtonDailyNormalized, &QRadioButton::clicked, this, &MainWindow::updateGraphsAndResultSummary);
    QObject::connect(ui->radioButtonMonthlyAverages, &QRadioButton::clicked, this, &MainWindow::updateGraphsAndResultSummary);
    QObject::connect(ui->radioButtonYearlyAverages, &QRadioButton::clicked, this, &MainWindow::updateGraphsAndResultSummary);
    QObject::connect(ui->radioButtonErrors, &QRadioButton::clicked, this, &MainWindow::updateGraphsAndResultSummary);
    QObject::connect(ui->radioButtonErrorHistogram, &QRadioButton::clicked, this, &MainWindow::updateGraphsAndResultSummary);
    QObject::connect(ui->radioButtonErrorNormalProbability, &QRadioButton::clicked, this, &MainWindow::updateGraphsAndResultSummary);

    QObject::connect(ui->checkBoxScatterInputs, &QCheckBox::clicked, this, &MainWindow::updateGraphsAndResultSummary);

    //NOTE: the lineeditdelegate is used by the tableviewparameters to provide an input widget when editing parameter values.
    lineEditDelegate = new ParameterEditDelegate();
    ui->tableViewParameters->setItemDelegateForColumn(1, lineEditDelegate);
    ui->tableViewParameters->verticalHeader()->hide();
    ui->tableViewParameters->setEditTriggers(QAbstractItemView::AllEditTriggers);

    ui->pushSaveParameters->setEnabled(false);
    ui->pushExportParameters->setEnabled(false);
    //ui->pushCreateDatabase->setEnabled(false);
    //ui->pushUploadInputs->setEnabled(false);

    ui->treeViewResults->setSelectionMode(QTreeView::ExtendedSelection); // Allows to ctrl-select multiple items.
    ui->treeViewInputs->setSelectionMode(QTreeView::ExtendedSelection); // Allows to ctrl-select multiple items.

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
    QObject::connect(ui->widgetPlotResults, &QCustomPlot::mouseWheel, this, &MainWindow::getCurrentRange);

    //TODO: We have to think about whether the login info for the hub should be hard coded.
    //NOTE: The hub ssh keys have to be distributed with the exe and be placed in the same folder as the exe.
    sshInterface_ = new SSHInterface("35.198.76.72", "magnus", "hubkey");

    QObject::connect(sshInterface_, &SSHInterface::log, this, &MainWindow::log);
    QObject::connect(sshInterface_, &SSHInterface::logError, this, &MainWindow::logSSHError);

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
    QScrollBar *bar = ui->textLog->verticalScrollBar();
    bool isatbottom = (bar->value() == bar->maximum());

    ui->textLog->append(QTime::currentTime().toString("hh:mm:  ") + Message);

    if(isatbottom) bar->setValue(bar->maximum()); //If it was at the bottom, scroll it down to the new bottom.
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
    QString dbState = selectedParameterDbPath_;
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
    std::ofstream usernamefile;
    usernamefile.open("lastusername.txt");
    if(usernamefile)
    {
        usernamefile.write(username.data(), username.size());
        usernamefile.close();
    }

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
    if(parameterDbWasSelected_) //NOTE: If a database is already selected we have to do some cleanup.
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
        if(treeInputs_) delete treeInputs_;
        treeResults_ = nullptr;
        treeInputs_ = nullptr;

        clearGraphsAndResultSummary();
    }

    bool success = projectDb_.setDatabase(fileName);
    if(success)
    {
        parameterDbWasSelected_ = true;
        selectedParameterDbPath_ = fileName;

        QFileInfo fileinfo(selectedParameterDbPath_);
        projectDirectory_ = fileinfo.absolutePath();

        loadParameterData();

        ui->treeViewParameters->expandToDepth(3);
        ui->treeViewParameters->resizeColumnToContents(0);
        ui->treeViewParameters->setColumnHidden(1, true);
        ui->treeViewParameters->setColumnHidden(2, true);

        ui->pushExportParameters->setEnabled(true);

        updateRunButtonState();

        setParametersHaveBeenEditedSinceLastSave(false);

        resetWindowTitle();
    }
    else
    {
        updateRunButtonState();
        ui->pushExportParameters->setEnabled(false);
        //TODO: additional error handling?
    }
}

void MainWindow::on_pushCreateDatabase_clicked()
{
    //TODO: This entire thing needs to be rethought. Button is removed for now.


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
    const char *remoteParameterDbName   = "parameters.db";

    //TODO: This means that they have to have a parameter database loaded, which is weird since what they are doing here is trying to create one.
    // We should instead query a selection list of models that can be loaded from the server.
    QString exename;
    projectDb_.setDatabase(selectedParameterDbPath_);
    projectDb_.getExenameFromParameterInfo(exename);

    qDebug() << "exe name was: " << exename;

    QByteArray exename2 = exename.toLatin1();

    QByteArray filename2 = fileName.toLatin1();
    bool success = sshInterface_->uploadEntireFile(filename2.data(), "~/", remoteParameterFileName);

    if(!success) return;

    success = sshInterface_->createParameterDatabase(exename2.data(), remoteParameterFileName, remoteParameterDbName);

    if(!success) return;

    QString saveFileName = QFileDialog::getSaveFileName(this,
        tr("Select location to store database file"), "", tr("Database files (*.db)"));

    if(fileName.isEmpty() || fileName.isNull()) //NOTE: In case the user clicked cancel etc.
    {
        return;
    }

    QByteArray saveFileName2 = saveFileName.toLatin1();

    //TODO: Don't hard code the location of the remote parameter database file?
    success = sshInterface_->downloadEntireFile(saveFileName2.data(), remoteParameterDbName);

    if(!success) return;

    loadParameterDatabase(saveFileName);
}

void MainWindow::on_pushExportParameters_clicked()
{
    if(!parameterDbWasSelected_)
    {
        return; //NOTE: Just in case. This should not be possible due to handling of button states
    }

    QString exportParametersPath = QFileDialog::getSaveFileName(this,
                tr("Select file to export parameters"), "", tr("Data files (*.dat)"));
    if(exportParametersPath.isEmpty() || exportParametersPath.isNull()) return; //NOTE: In case the user clicked cancel or closed the dialog.

    on_pushSaveParameters_clicked(); //NOTE: save any changes to the database.

    QString exename;
    projectDb_.setDatabase(selectedParameterDbPath_);
    projectDb_.getExenameFromParameterInfo(exename);

    if(weExpectToBeConnected_)
    {
        if(!sshInterface_->isInstanceConnected())
        {
            handleInvoluntarySSHDisconnect();
            return;
        }

        const char *remoteParameterFileName = "parameters.dat";
        const char *remoteParameterDbName = "parameters.db";

        QByteArray dbfilename2 = selectedParameterDbPath_.toLatin1();
        bool success = sshInterface_->uploadEntireFile(dbfilename2.data(), "~/", remoteParameterDbName);
        if(!success) return;

        QByteArray exename2 = exename.toLatin1();

        success = sshInterface_->exportParameters(exename2.data(), remoteParameterDbName, remoteParameterFileName);

        if(!success) return;

        QByteArray saveFileName2 = exportParametersPath.toLatin1();

        success = sshInterface_->downloadEntireFile(saveFileName2.data(), remoteParameterFileName);
    }
    else
    {
        //For now, assume the exe is in the same directory as the parameter database.
        QString program = projectDirectory_.absoluteFilePath(exename);

        qDebug() << "trying to run program " << program;

        QStringList arguments;
        arguments << "convert_parameters" <<  selectedParameterDbPath_ << exportParametersPath;

        runModelProcessLocally(program, arguments);
    }
}

void MainWindow::on_pushUploadInputs_clicked()
{

    selectedInputFilePath_ = QFileDialog::getOpenFileName(this,
                tr("Select input file"), "", tr("Data files (*.dat)"));  //TODO: should not restrict it to .dat

    if(selectedInputFilePath_.isEmpty() || selectedInputFilePath_.isNull()) //NOTE: In case the user clicked cancel etc.
    {
        return;
    }

    inputFileWasSelected_ = true;
    updateRunButtonState();

    if(!weExpectToBeConnected_)
    {

        return;
    }

    if(!sshInterface_->isInstanceConnected())
    {
        handleInvoluntarySSHDisconnect();
        return;
    }

    const char *remoteInputFileName = "uploadedinputs.dat";

    QByteArray filename2 = selectedInputFilePath_.toLatin1();
    bool success = sshInterface_->uploadEntireFile(filename2.data(), "~/", remoteInputFileName);

    if(success) inputFileWasUploaded_ = true;
}

void MainWindow::loadParameterData()
{
    if(parameterDbWasSelected_)
    {
        projectDb_.setDatabase(selectedParameterDbPath_);

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
                parameterModel_->addParameter(data.name, data.unit, data.description, data.ID, data.parentID, par);
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

void MainWindow::loadResultAndInputStructure(const char *ResultDb, const char *InputDb)
{
    log("Attempting to load result and input structure.");

    bool success = false;
    QVector<TreeData> resultstreedata;
    QVector<TreeData> inputtreedata;
    if(weExpectToBeConnected_)
    {
        success = sshInterface_->getStructureData(ResultDb, "ResultsStructure", resultstreedata);
        success = success && sshInterface_->getStructureData(InputDb, "InputsStructure", inputtreedata);
    }
    else
    {
        QString resultdbpath = projectDirectory_.absoluteFilePath(ResultDb);
        projectDb_.setDatabase(resultdbpath);
        success = projectDb_.getResultOrInputStructure(resultstreedata, "ResultsStructure");

        QString inputdbpath = projectDirectory_.absoluteFilePath(InputDb);
        projectDb_.setDatabase(inputdbpath);
        success = success && projectDb_.getResultOrInputStructure(inputtreedata, "InputsStructure");
    }

    if(resultstreedata.empty())
    {
        logError("The result structure is empty. A results database may not have been created, maybe due to an error.");
        return;
    }

    if(!success) return;

    // Setup result structure
    if(treeResults_) delete treeResults_;

    treeResults_ = new TreeModel("Results structure");

    maxresultID_ = 0;
    for(TreeData& item : resultstreedata)
    {
        treeResults_->addItem(item);
        maxresultID_ = item.ID > maxresultID_ ? item.ID : maxresultID_;
    }

    ui->treeViewResults->setModel(treeResults_);

    QObject::connect(ui->treeViewResults->selectionModel(), &QItemSelectionModel::selectionChanged, this, &MainWindow::updateGraphsAndResultSummary);

    // Setup input structure
    if(treeInputs_) delete treeInputs_;

    treeInputs_ = new TreeModel("Input structure");

    for(TreeData &item : inputtreedata)
    {
        //NOTE: We remap the input IDs so that they don't overlap with the result IDs. This makes every timeseries have a unique internal ID in INCAView, and simplifies the Plotter a bit.
        item.ID += maxresultID_;
        if(item.parentID != 0) item.parentID += maxresultID_; //NOTE: parentID=0 just signifies that it does not have a parent, so that should stay 0.
        treeInputs_->addItem(item);
    }

    ui->treeViewInputs->setModel(treeInputs_);

    QObject::connect(ui->treeViewInputs->selectionModel(), &QItemSelectionModel::selectionChanged, this, &MainWindow::updateGraphsAndResultSummary);

    //TODO: The following should be done somewhere else?

    ui->radioButtonDaily->setEnabled(true);
    ui->radioButtonDailyNormalized->setEnabled(true);
    ui->radioButtonMonthlyAverages->setEnabled(true);
    ui->radioButtonYearlyAverages->setEnabled(true);
    ui->radioButtonErrors->setEnabled(true);
    ui->radioButtonErrorHistogram->setEnabled(true);
    ui->radioButtonErrorNormalProbability->setEnabled(true);

    ui->treeViewResults->expandToDepth(3);
    ui->treeViewResults->resizeColumnToContents(0);
    ui->treeViewResults->setColumnHidden(1, true);
    ui->treeViewResults->setColumnHidden(2, true);

    ui->treeViewInputs->expandToDepth(3);
    ui->treeViewInputs->resizeColumnToContents(0);
    ui->treeViewInputs->setColumnHidden(1, true);
    ui->treeViewInputs->setColumnHidden(2, true);

    log("Loading complete.");
}

void MainWindow::updateRunButtonState()
{
    if(parameterDbWasSelected_ &&
       inputFileWasSelected_
      )
    {
        ui->pushRun->setEnabled(true);
        ui->pushRunOptimizer->setEnabled(true);
    }
    else
    {
        ui->pushRun->setEnabled(false);
        ui->pushRunOptimizer->setEnabled(false);
    }

}

void MainWindow::setWeExpectToBeConnected(bool connected)
{
    weExpectToBeConnected_ = connected;

    //updateRunButtonState();

    if(connected)
    {
        ui->pushConnect->setEnabled(false);
        ui->lineEditUsername->setEnabled(false);
        ui->pushDisconnect->setEnabled(true);
        ui->pushCreateDatabase->setEnabled(true);
    }
    else
    {
        ui->pushConnect->setEnabled(true);
        ui->lineEditUsername->setEnabled(true);
        ui->pushDisconnect->setEnabled(false);
        ui->pushRun->setEnabled(false);
        ui->pushRunOptimizer->setEnabled(false);
        ui->pushCreateDatabase->setEnabled(false);
        ui->radioButtonDaily->setEnabled(false);
        ui->radioButtonDailyNormalized->setEnabled(false);
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
    treeResults_ = nullptr;

    if(treeInputs_) delete treeInputs_;
    treeInputs_ = nullptr;

    clearGraphsAndResultSummary();
}


void MainWindow::handleInvoluntarySSHDisconnect()
{
    //TODO: Should we attempt to destroy the compute instance?
    // OR it would probably be better to attempt to reconnect to it?


    setWeExpectToBeConnected(false);

    if(treeResults_) delete treeResults_; //NOTE: The destructor of the QAbstractItemModel automatically disconnects it from it's view.
    treeResults_ = nullptr;

    if(treeInputs_) delete treeInputs_;
    treeInputs_ = nullptr;

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

        projectDb_.setDatabase(selectedParameterDbPath_);
        bool success = projectDb_.writeParameterValues(parameterdata);
        if(success)
        {
            setParametersHaveBeenEditedSinceLastSave(false);

            editUndoStack_.clear();

            log("Saving parameters complete.");
        }
    }
}


void MainWindow::on_pushRun_clicked()
{
    QVector<Parameter *> parametersNotInRange;
    if(parameterModel_->areAllParametersInRange(parametersNotInRange))
    {
        runModel();
    }
    else
    {
        QString msg = "Not all parameter values are in the suggested [Min, Max] range:";

        for(Parameter *param : parametersNotInRange)
        {
            msg += "\n" + param->name;
           //TODO: This does not work, I don't know why:
           //int ID = param->ID;
           //qDebug() << "ID of param out of range: " << ID;
           //QString parentName = treeParameters_->getParentName(ID);
           //msg += "\n" + param->name + " (" + parentName + ")";
        }

        msg += "\nThis may cause the model to behave unexpectedly. Run the model anyway?";


        QMessageBox msgBox(QMessageBox::Warning, tr("Invalid parameters"), msg);
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


void MainWindow::on_pushRunOptimizer_clicked()
{
    ui->pushRunOptimizer->setEnabled(false);


    QString setupScriptPath = QFileDialog::getOpenFileName(this,
           tr("Select optimization script"), "", tr("Data files (*.dat)"));

    if(setupScriptPath.isEmpty() || setupScriptPath.isNull())     //NOTE: in case the user clicked cancel.
    {
       return;
    }

    log("Attempting to run optimization...");

    on_pushSaveParameters_clicked(); //NOTE: Save the parameters to the database.

    if(weExpectToBeConnected_ && inputFileWasSelected_ && !inputFileWasUploaded_) //NOTE: If the input file was selected before we connected it has not been uploaded yet, so we have to do it now.
    {
        //TODO: This is repeated code from RunModel.

        const char *remoteInputFileName = "uploadedinputs.dat";

        QByteArray filename2 = selectedInputFilePath_.toLatin1();
        bool success = sshInterface_->uploadEntireFile(filename2.data(), "~/", remoteInputFileName);

        if(success) inputFileWasUploaded_ = true;
    }

    QString exename;
    projectDb_.setDatabase(selectedParameterDbPath_);
    projectDb_.getExenameFromParameterInfo(exename);

    qDebug() << "exe name was: " << exename;

    if(weExpectToBeConnected_)
    {
        //TODO: Not implemented
    }
    else
    {
        //For now, assume the exe is in the same directory as the parameter database.
        QString program = projectDirectory_.absoluteFilePath(exename);

        qDebug() << "trying to run program with optimization " << program;

        QStringList arguments;
        arguments << "run_optimizer" << selectedInputFilePath_ << selectedParameterDbPath_ << setupScriptPath << "optimized_parameters.db";

        runModelProcessLocally(program, arguments);
    }

    log("Optimizer process completed.");

    //NOTE: Load in the optimized parameters and run the model one more time to see the results.
    QString dbpath = projectDirectory_.filePath("optimized_parameters.db");

    loadParameterDatabase(dbpath);
    runModel();

    ui->pushRunOptimizer->setEnabled(true);
}


bool MainWindow::runModelProcessLocally(const QString& program, const QStringList& arguments)
{
    QProcess modelrun;
    modelrun.setWorkingDirectory(projectDirectory_.path());
    modelrun.start(program, arguments);
    if(!modelrun.waitForStarted())
    {
        logError("Model exe process did not start.");
        ui->pushRun->setEnabled(true);
        return false;
    }

    bool correct = true;

    connect(&modelrun, &QProcess::readyReadStandardOutput, [&](){log(modelrun.readAllStandardOutput());});
    connect(&modelrun, &QProcess::readyReadStandardError, [&](){logError(modelrun.readAllStandardError()); correct=false;});
    connect(&modelrun, &QProcess::errorOccurred, [&]()
    {
        logError("An error occurred while running the model exe.");
        correct = false;
    }
    );

    if(!modelrun.waitForFinished(-1)) //TODO: We could maybe have a timeout, but it is hard to predict what it should be (some models could potentially take a minute or two to run?).
    {
        logError("Model exe process finished incorrectly.");
        ui->pushRun->setEnabled(true);
        return false;
    }

    return correct;
}

void MainWindow::runModel()
{
    if(!parameterDbWasSelected_)
    {
        //it should not be possible to reach this state since the button should be disabled
        return;
    }

    ui->pushRun->setEnabled(false);

    log("Attempting to run Model...");

    on_pushSaveParameters_clicked(); //NOTE: Save the parameters to the database.

    if(weExpectToBeConnected_ && inputFileWasSelected_ && !inputFileWasUploaded_) //NOTE: If the input file was selected before we connected it has not been uploaded yet, so we have to do it now.
    {
        const char *remoteInputFileName = "uploadedinputs.dat";

        QByteArray filename2 = selectedInputFilePath_.toLatin1();
        bool success = sshInterface_->uploadEntireFile(filename2.data(), "~/", remoteInputFileName);

        if(success) inputFileWasUploaded_ = true;
    }

    const char *ResultDb = "results.db";
    const char *InputDb  = "inputs.db";

    QString exename;
    projectDb_.setDatabase(selectedParameterDbPath_);
    projectDb_.getExenameFromParameterInfo(exename);

    qDebug() << "exe name was: " << exename;

    bool success = true;

    if(weExpectToBeConnected_)
    {
        if(!sshInterface_->isInstanceConnected())
        {
            handleInvoluntarySSHDisconnect();
            return;
        }

        //TODO: Upload the entire parameter database to the instance!
        const char *remoteParameterDbName = "parameters.db";

        QByteArray dbpath = selectedParameterDbPath_.toLatin1();
        sshInterface_->uploadEntireFile(dbpath.data(), "~/", remoteParameterDbName);


        QByteArray exename2 = exename.toLatin1();

        const char *remoteInputFile = "uploadedinputs.dat";
        sshInterface_->runModel(exename2.data(), remoteInputFile, remoteParameterDbName); //TODO: This one should also report success/error?
    }
    else
    {
        //For now, assume the exe is in the same directory as the parameter database.
        QString program = projectDirectory_.absoluteFilePath(exename);

        //TODO: Deleting the previous inputs and results db may not be that clean, but we don't have any system for managing it properly yet, so not deleting them causes errors.
        QString resultpath = projectDirectory_.absoluteFilePath(ResultDb);
        QFile::remove(resultpath);
        QString inputpath = projectDirectory_.absoluteFilePath(InputDb);
        QFile::remove(inputpath);

        qDebug() << "trying to run program " << program;

        QStringList arguments;
        arguments << "run" << selectedInputFilePath_ << selectedParameterDbPath_;

        success = runModelProcessLocally(program, arguments);
    }

    //log("Model run process completed."); //NOTE: This one was just confusing, since it was also printed if there was an error.

    if(success)
    {
        //TODO: We should do a more rigorous check here. If e.g. the user has switched out the input file between runs then the tree structure may no longer be valid and should be recreated.
        if(!treeResults_)
            loadResultAndInputStructure(ResultDb, InputDb);

        plotter_->clearCache();

        updateGraphsAndResultSummary(); //In case somebody had a graph selected, it is updated with a plot of the data generated from the last run.
    }

    ui->pushRun->setEnabled(true);
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

bool MainWindow::getDataSets(const char *dbname, const QVector<int> &IDs, const char *table, QVector<QVector<double>> &seriesout, QVector<int64_t> &startdatesout)
{
    if(weExpectToBeConnected_)
    {
        if(!sshInterface_->isInstanceConnected())
        {
            handleInvoluntarySSHDisconnect();
            return false;
        }

        return sshInterface_->getDataSets(dbname, IDs, table, seriesout, startdatesout);
    }
    else
    {
        QString dbpath = projectDirectory_.absoluteFilePath(dbname);
        projectDb_.setDatabase(dbpath);
        return projectDb_.getResultOrInputValues(table, IDs, seriesout, startdatesout);
    }
}


void MainWindow::updateGraphsAndResultSummary()
{
    if(!treeResults_ || !treeInputs_)
    {
        clearGraphsAndResultSummary();
        return;
    }

    QModelIndexList resultindexes = ui->treeViewResults->selectionModel()->selectedIndexes();

    QVector<QString> names;

    QVector<int> resultIDs;
    for(auto index : resultindexes)
    {
        if(index.column() == 0)
        {
            auto idx = index.model()->index(index.row(),index.column() + 1, index.parent());
            int ID = (treeResults_->itemData(idx))[0].toInt();
            if( ID != 0 && treeResults_->childCount(ID) == 0) //NOTE: If it has children in the tree, it is an indexer or index, not a result series.
            {
                resultIDs.push_back(ID);
                QString name = treeResults_->getName(ID);
                QString parentName = treeResults_->getParentName(ID);
                QString unit = treeResults_->getUnit(ID);
                names.push_back(name + " (" + parentName + ") " + unit);
            }  
        }
    }

    QModelIndexList inputindexes = ui->treeViewInputs->selectionModel()->selectedIndexes();

    QVector<int> inputIDs;
    for(auto index : inputindexes)
    {
        auto idx = index.model()->index(index.row(), index.column() + 1, index.parent());
        int ID = (treeInputs_->itemData(idx))[0].toInt();
        if(ID != 0 && treeInputs_->childCount(ID) == 0)
        {
            inputIDs.push_back(ID);
            QString name = treeInputs_->getName(ID);
            QString parentName = treeInputs_->getParentName(ID);
            QString unit = treeInputs_->getUnit(ID);
            names.push_back(name + " (" + parentName + ") " + unit);
        }
    }

    if(!resultIDs.empty() || !inputIDs.empty())
    {
        QVector<int> uncachedResultIDs;
        plotter_->filterUncachedIDs(resultIDs, uncachedResultIDs);

        bool success = true;
        if(!uncachedResultIDs.empty())
        {
            //TODO: Formalize the paths to the databases in some way so that they are not just scattered around in the code.
            const char *resultdb = "results.db";
            QVector<QVector<double>> resultsets;
            QVector<int64_t> startDates;
            success = getDataSets(resultdb, uncachedResultIDs, "Results", resultsets, startDates);
            plotter_->addToCache(uncachedResultIDs, resultsets, startDates);
        }

        QVector<int> uncachedInputIDs;
        plotter_->filterUncachedIDs(inputIDs, uncachedInputIDs);

        if(!uncachedInputIDs.empty())
        {
            for(int &ID : uncachedInputIDs) ID -= maxresultID_; //NOTE: remap the input IDs back so that we can use them to request from the database.

            //TODO: Formalize the paths to the remote databases in some way so that they are not just scattered around in the code.
            const char *inputdb = "inputs.db";
            QVector<QVector<double>> inputsets;
            QVector<int64_t> startDates;
            success = getDataSets(inputdb, uncachedInputIDs, "Inputs", inputsets, startDates);

            for(int &ID : uncachedInputIDs) ID += maxresultID_; //NOTE: map them back AGAIN because we now talk to the internal system.

            plotter_->addToCache(uncachedInputIDs, inputsets, startDates);
        }

        //NOTE: For now we don't have a individual start date for each time series. Instead, we just get one. And we assume that the start date for the result data
        // is the same as the one for the input data. That is how the models work currently too (as of 24.09.2018).

        if(success)
        {
            PlotMode mode = PlotMode_Daily;
            if(ui->radioButtonMonthlyAverages->isChecked()) mode = PlotMode_MonthlyAverages;
            else if(ui->radioButtonDailyNormalized->isChecked()) mode = PlotMode_DailyNormalized;
            else if(ui->radioButtonYearlyAverages->isChecked()) mode = PlotMode_YearlyAverages;
            else if(ui->radioButtonErrors->isChecked()) mode = PlotMode_Error;
            else if(ui->radioButtonErrorHistogram->isChecked()) mode = PlotMode_ErrorHistogram;
            else if(ui->radioButtonErrorNormalProbability->isChecked()) mode = PlotMode_ErrorNormalProbability;

            QVector<int> IDs;
            IDs.append(resultIDs);
            IDs.append(inputIDs);


            QVector<bool> scatter;
            for(int i = 0; i < resultIDs.size(); ++i) scatter << false;
            for(int i = 0; i < inputIDs.size(); ++i) scatter << ui->checkBoxScatterInputs->isChecked();

            plotter_->plotGraphs(IDs, names, mode, scatter);
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

        QString valueString= "";
        bool first = true;
        for(int i = 0; i < ui->widgetPlotResults->graphCount(); ++i)
        {
            int ID = plotter_->currentPlottedIDs_[i];

            if(first) first = false;
            else valueString.append("\n");

            QCPGraph *graph = ui->widgetPlotResults->graph(i);
            double value = graph->data()->findBegin(x)->value;

            if(ui->radioButtonErrors->isChecked())
            {
                if(i == 0) valueString.append("Error: ");
                else valueString.append("Linearly regressed error: ");
            }
            else if(ui->radioButtonDaily->isChecked() || ui->radioButtonMonthlyAverages->isChecked() || ui->radioButtonYearlyAverages->isChecked() || ui->radioButtonDailyNormalized->isChecked())
            {
                //TODO: instead of doing this we should just store a description with each graph in the Plotter and read that here.
                QString name;
                QString parentName;
                QString unit;
                if(ID <= maxresultID_)
                {
                    name = treeResults_->getName(ID);
                    parentName = treeResults_->getParentName(ID);
                    unit = treeResults_->getUnit(ID);

                }
                else
                {
                    name = treeInputs_->getName(ID);
                    parentName = treeInputs_->getParentName(ID);
                    unit = treeInputs_->getUnit(ID);
                }
                //TODO: If there are nested indexes, we should probably also print the parent of the parent and so on...
                valueString.append(name).append(" (").append(parentName).append(") ").append(unit).append(" : ");
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
        else if(ui->radioButtonDailyNormalized->isChecked() )
            dateString = QLocale().toString(date, "d. MMMM yyyy").append("Normalized");


        ui->labelGraphValues->setText(QString("%1:\n%2").arg(dateString).arg(valueString));
    }
    else
    {
        ui->labelGraphValues->setText("");
    }
}



void MainWindow::getCurrentRange(QWheelEvent *event)
{
    //NOTE: This is done to keep the range when new plots are added or the model is run again
    if (event)
    {
        plotter_->setXrange(ui->widgetPlotResults->xAxis->range());
    }
}


void MainWindow::on_pushExportResults_clicked()
{
    if(!treeResults_)
    {
        return;
    }

    QString saveResultsPath = QFileDialog::getSaveFileName(this,
                tr("Select file to save results"), "", tr("Data files (*.csv)"));

    if(saveResultsPath.isEmpty() || saveResultsPath.isNull()) return; //NOTE: In case the user clicked cancel or closed the dialog.

    QModelIndexList resultindexes = ui->treeViewResults->selectionModel()->selectedIndexes();

    QVector<QString> names;

    QVector<int> resultIDs;
    for(auto index : resultindexes)
    {
        if(index.column() == 0)
        {
            auto idx = index.model()->index(index.row(),index.column() + 1, index.parent());
            int ID = (treeResults_->itemData(idx))[0].toInt();
            if( ID != 0 && treeResults_->childCount(ID) == 0) //NOTE: If it has children in the tree, it is an indexer or index, not a result series.
            {
                resultIDs.push_back(ID);
                QString name = treeResults_->getName(ID);
                QString parentName = treeResults_->getParentName(ID);
                names.push_back(name + " (" + parentName + ")"); //TODO: We should get all the indexes here, not just the immediate one.
            }
        }
    }

    if(resultIDs.empty()) return;

    std::ofstream file(saveResultsPath.toLatin1().data());
    if(!file.is_open())
    {
        logError(QString("Unable to open file ") + saveResultsPath);
    }

    //NOTE: Since these IDs have been clicked, we can assume that their timeseries have been loaded into the plotter.
    //NOTE: For result series it is safe to assume that they all have the same length.
    int seriesCount = resultIDs.count();
    QVector<QVector<double>*> resultSeries(seriesCount);
    file << "\"date\",";
    for(size_t idx = 0; idx < seriesCount; ++idx)
    {
        resultSeries[idx] = &plotter_->cache_[resultIDs[idx]];

        file << "\"" << names[idx].toLatin1().data() << "\"";
        if(idx < seriesCount - 1) file << ",";
    }
    file << std::endl;

    int stepcount = resultSeries[0]->count();

    int64_t date = plotter_->startDateCache_[resultIDs[0]]; //NOTE: We are assuming that all result series start at the same date.

    for(int t = 0; t < stepcount; ++t)
    {
        QDateTime workingdate = QDateTime::fromSecsSinceEpoch(date, Qt::OffsetFromUTC, 0);
        file << workingdate.toString("yyyy-MM-dd").toStdString() << ",";
        date += 86400;

        for(int idx = 0; idx < seriesCount; ++idx)
        {
            file << resultSeries[idx]->at(t);
            if(idx < seriesCount - 1) file << ",";
        }
        file << std::endl;
    }

    log("Results exported to " + saveResultsPath);

    file.close();
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

    setParametersHaveBeenEditedSinceLastSave(true);
}


void MainWindow::setParametersHaveBeenEditedSinceLastSave(bool changed)
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


void MainWindow::on_widgetPlotResults_windowTitleChanged(const QString &title)
{

}
