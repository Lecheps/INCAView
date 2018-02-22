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

    ui->radioButtonDaily->click();
    ui->radioButtonDaily->setEnabled(false);
    ui->radioButtonMonthlyAverages->setEnabled(false);
    ui->radioButtonYearlyAverages->setEnabled(false);

    QObject::connect(ui->radioButtonDaily, &QRadioButton::clicked, this, &MainWindow::repaintGraphs);
    QObject::connect(ui->radioButtonMonthlyAverages, &QRadioButton::clicked, this, &MainWindow::repaintGraphs);
    QObject::connect(ui->radioButtonYearlyAverages, &QRadioButton::clicked, this, &MainWindow::repaintGraphs);

    resetWindowTitle();

    lineEditDelegate = new LineEditDelegate();

    QAction *ctrlc = new QAction("copy");
    ctrlc->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_C));
    QObject::connect(ctrlc, &QAction::triggered, this, &MainWindow::copyRequest);
    ui->centralWidget->addAction(ctrlc);

    graphColors_ = {{0, 130, 200}, {230, 25, 75}, {60, 180, 75}, {245, 130, 48}, {145, 30, 180},
                    {70, 240, 240}, {240, 50, 230}, {210, 245, 60}, {250, 190, 190}, {0, 128, 128}, {230, 190, 255},
                    {170, 110, 40}, {128, 0, 0}, {170, 255, 195}, {128, 128, 0}, {255, 215, 180}, {0, 0, 128}, {255, 225, 25}};

    ui->widgetPlotResults->setInteraction(QCP::iRangeDrag, true);
    ui->widgetPlotResults->setInteraction(QCP::iRangeZoom, true);
    ui->widgetPlotResults->axisRect(0)->setRangeDrag(Qt::Horizontal);
    ui->widgetPlotResults->axisRect(0)->setRangeZoom(Qt::Horizontal);

    QObject::connect(ui->widgetPlotResults, QCustomPlot::mouseMove, this, &MainWindow::graphToolTip);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::resetWindowTitle()
{
    if(pathToDBIsSet() && stuffHasBeenEditedSinceLastSave)
    {
        setWindowTitle("*" + loadedDBPath_ + " - INCA View");
        return;
    }
    else if(pathToDBIsSet())
    {
        setWindowTitle(loadedDBPath_ + " - INCA View");
        return;
    }
    setWindowTitle("INCA View");
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
                    delete parameterModel_;

                    ui->widgetPlotResults->clearGraphs();
                    ui->widgetPlotResults->replot();
                }

                stuffHasBeenEditedSinceLastSave = false;

                ui->pushSaveAs->setEnabled(true);
                ui->pushRun->setEnabled(true);

                ui->radioButtonDaily->setEnabled(true);
                ui->radioButtonMonthlyAverages->setEnabled(true);
                ui->radioButtonYearlyAverages->setEnabled(true);


                loadedDBPath_ = pathToDB;

                copyAndOverwriteFile(loadedDBPath_, tempWorkingDBPath);

                setDBPath(tempWorkingDBPath);

                resetWindowTitle();

                parameterModel_ = new ParameterModel();
                populateParameterModel(parameterModel_);
                ui->tableViewParameters->setModel(parameterModel_);
                ui->tableViewParameters->verticalHeader()->hide();
                ui->tableViewParameters->setItemDelegateForColumn(1, lineEditDelegate);
                ui->tableViewParameters->setEditTriggers(QAbstractItemView::CurrentChanged);
                //ui->tableViewParameters->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
                QObject::connect(parameterModel_, &ParameterModel::parameterWasEdited, this, &MainWindow::parameterWasEdited);

                treeParameters_ = new TreeModel("Parameter Structure");
                treeResults_ = new TreeModel("Results Structure");
                populateTreeModel(treeParameters_, "ParameterStructure", true);
                populateTreeModel(treeResults_, "ResultsStructure", false);

                ui->treeViewParameters->setModel(treeParameters_);
                ui->treeViewParameters->expandToDepth(3);
                ui->treeViewParameters->setColumnHidden(1, true);
                ui->treeViewParameters->resizeColumnToContents(0);
                ui->treeViewResults->setModel(treeResults_);
                ui->treeViewResults->expandToDepth(1);
                ui->treeViewResults->resizeColumnToContents(0);
                ui->treeViewResults->setColumnHidden(1, true);
                ui->treeViewResults->setSelectionMode(QTreeView::ExtendedSelection);

                QObject::connect(ui->treeViewParameters->selectionModel(), &QItemSelectionModel::selectionChanged, this, &MainWindow::parameterTreeSelectionChanged);
                QObject::connect(ui->treeViewResults->selectionModel(), &QItemSelectionModel::selectionChanged, this, &MainWindow::repaintGraphs);

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

void MainWindow::on_pushSave_clicked()
{
    if(pathToDBIsSet()) // Just for safety. The button is disabled in this case.
    {
        if(saveCheckParameters()) tryToSave(tempWorkingDBPath, loadedDBPath_);
    }
}

void MainWindow::on_pushSaveAs_clicked()
{
    if(pathToDBIsSet()) // Just for safety. The button is disabled in this case.
    {
        if(saveCheckParameters())
        {
            QString pathToSave = QFileDialog::getSaveFileName(this, tr("Select location to store a backup copy of the database"), "c:/Users/Magnus/Documents/INCAView/", tr("Database files (*.db)"));

            if(!pathToSave.isEmpty()&& !pathToSave.isNull()) //In case the user cancel, or no to the overwrite question.
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
    }
    else
    {
        QMessageBox::warning(this, "Save failed", "The operating system failed to save the file. Check if the file is in use by another program.", QMessageBox::Ok);
    }
    return saveWorked;
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
        qDebug(oldpath.toLatin1().data());
        qDebug(newpath.toLatin1().data());
    }

    return success;
}


void MainWindow::on_pushRun_clicked()
{
    if(pathToDBIsSet()) // Just for safety. The button is disabled in this case.
    {
        if(parameterModel_->areAllParametersInRange())
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
    toggleStuffHasBeenEditedSinceLastSave(true);
}

void MainWindow::parameterTreeSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected)
{
    parameterModel_->clearVisibleParameters();

    QModelIndexList indexes = selected.indexes();
    if(indexes.count() >= 1)
    {
        QModelIndex index = indexes[0];
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
            parameterModel_->setParameterVisible(ID);
        } 

        disconnectDB();
    }
}

void MainWindow::repaintGraphs()
{
    ui->widgetPlotResults->clearGraphs();
    ui->widgetPlotResults->yAxis->setRange(0, 2.0*QCPRange::minRange); //Find a better way to clear it?
    ui->textResultsInfo->clear();

    QModelIndexList indexes = ui->treeViewResults->selectionModel()->selectedIndexes();
    int firstunassignedcolor = 0;

    for(auto index : indexes)
    {
        auto idx = index.model()->index(index.row(),index.column() + 1, index.parent());
        int ID = (treeParameters_->itemData(idx))[0].toInt();
        if(index.column() == 0 && ID != 0)
        {
            connectDB();
            QSqlQuery query;
            query.prepare("SELECT value FROM Results "
                          "WHERE ID=:ID;"
                        );
            query.bindValue(":ID",ID);
            query.exec();


            //TODO: Currently times are just for debugging!!! Should read actual times from database.
            uint starttime = QDateTime::currentDateTime().toTime_t();

            QVector<double> xval, yval;
            int cnt = 0;
            double min = std::numeric_limits<double>::max();
            double max = std::numeric_limits<double>::min();
            while (query.next())
            {
                double value = query.value(0).toDouble();
                xval.append(starttime + 24*3600*(cnt++));
                yval.append(value);
                min = value < min ? value : min;
                max = value > max ? value : max;
            }
            disconnectDB();

            if(cnt != 0) // TODO: We should maybe add in a better check to avoid indexes and indexers.
            {
                QString name = treeResults_->getName(ID);
                QString parentName = treeResults_->getParentName(ID);
                QColor color = graphColors_[firstunassignedcolor++];
                if(firstunassignedcolor == graphColors_.count()) firstunassignedcolor = 0; // Cycle the colors

                double mean = 0;
                for(double d : yval) mean += d;
                mean /= (double)cnt;
                double stddev = 0;
                for(double d : yval) stddev += (d - mean)*(d - mean);
                stddev = std::sqrt(stddev / (double) cnt);

                ui->textResultsInfo->append(name + " (" + parentName + ") <font color=" + color.name() + ">&#9608;&#9608;</font>"); //NOTE: this is somewhat reliant on the font on the local system having the character &#9608; (FULL BLOCK)
                ui->textResultsInfo->append("min: " + QString::number(min, 'g', 5)); //TODO: should this be moved below to display the min of yearly averages when yearly averages are selected?
                ui->textResultsInfo->append("max: " + QString::number(max, 'g', 5));
                ui->textResultsInfo->append("average: " + QString::number(mean, 'g', 5));
                ui->textResultsInfo->append("standard deviation: " + QString::number(stddev, 'g', 5));
                ui->textResultsInfo->append(""); //To get a newline

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
                    for(int i = 0; i < cnt; ++i)
                    {
                        sum += yval[i];
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
                    for(int i = 0; i < cnt; ++i)
                    {
                        sum += yval[i];
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
}


void MainWindow::graphToolTip(QMouseEvent *event)
{
    if(ui->widgetPlotResults->graphCount() > 0)
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

            //NOTE: there is a bug where the value is not completely synched to the x. The value may change even if the displayed x does not.
            //may have to do with how the graph values are stored.

            int ID = indexes[2*i + 1].data().toInt();
            valueString.append(treeResults_->getName(ID)).append(" (").append(treeResults_->getParentName(ID)).append("): ");

            valueString.append(QString::number(value, 'g', 5));
        }

        QDateTime date = QDateTime::fromTime_t(x);
        QString dateString;
        if(ui->radioButtonYearlyAverages->isChecked())
            dateString = date.toString("yyyy").append(" yearly average");
        else if(ui->radioButtonMonthlyAverages->isChecked())
            dateString = date.toString("MMMM yyyy").append(" monthly average");
        else
            dateString = date.toString("d. MMMM yyyy");

        ui->labelGraphValues->setText(QString("%1:\n%2").arg(dateString).arg(valueString));
    }
}


void MainWindow::populateParameterModel(ParameterModel *model)
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
        model->addParameter(
                    query.value(2).toInt(), // ID
                    query.value(0).toString(), // name
                    query.value(1).toString(), // type
                    query.value(5).toString(), // value
                    query.value(3).toString(), // min
                    query.value(4).toString() // max
                    );
    }
    disconnectDB();
}

void MainWindow::populateTreeModel(TreeModel* model, const QString& tableName, bool indexersAndIndexesOnly)
{
    QString onlyIndexersAndIndexes = indexersAndIndexesOnly ? "AND (child.isIndexer is not null OR child.isIndex is not null) " : " ";

    connectDB();
    QSqlQueryModel query;
    query.setQuery("SELECT parent.ID as parentID, child.ID, child.Name "
                          "FROM " + tableName + " as parent, " + tableName + " as child "
                          "WHERE child.lft > parent.lft "
                          "AND child.rgt < parent.rgt "
                          "AND child.dpt = parent.dpt + 1 " + onlyIndexersAndIndexes +
                          "UNION "
                          "SELECT 0 as parentID, child.ID, child.Name " //NOTE: This 0 has to be the same ID as assigned to the tree root node in the TreeModel constructor. This will not work if any of the items in the database have ID=0
                          "FROM " + tableName + " as child "
                          "WHERE child.dpt = 0;"
                          );

    for (int i = 0; i < query.rowCount(); ++i)
    {
        int parentID = query.data(query.index(i, 0)).toInt();
        int childID = query.data(query.index(i, 1)).toInt();
        QString childName = query.data(query.index(i, 2)).toString();
        model->addItem(childName, childID, parentID);
    }
    disconnectDB();
}

void MainWindow::parameterWasEdited(const QString& newValue, int dbID)
{
    connectDB();

    QSqlQuery query;
    query.prepare( "UPDATE "
                   " ParameterValues "
                   " SET "
                   " value = '" + newValue + "'"
                    " WHERE "
                    " ID = :id ;");
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
    resetWindowTitle();
}

void MainWindow::copyRequest(bool checked)
{
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
