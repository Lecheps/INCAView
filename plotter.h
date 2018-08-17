#ifndef PLOTTER_H
#define PLOTTER_H

#include "qcustomplot.h"

enum PlotMode
{
    PlotMode_Daily,
    PlotMode_MonthlyAverages,
    PlotMode_YearlyAverages,
    PlotMode_Error,
    PlotMode_ErrorHistogram,
    PlotMode_ErrorNormalProbability,
};

class Plotter
{
public:
    Plotter(QCustomPlot *plot, QTextBrowser *resultsInfo)
        : plot_(plot), resultsInfo_(resultsInfo),

          graphColors_({{0, 130, 200}, {230, 25, 75}, {60, 180, 75}, {245, 130, 48}, {145, 30, 180},
                        {70, 240, 240}, {240, 50, 230}, {210, 245, 60}, {250, 190, 190}, {0, 128, 128}, {230, 190, 255},
                        {170, 110, 40}, {128, 0, 0}, {170, 255, 195}, {128, 128, 0}, {255, 215, 180}, {0, 0, 128}, {255, 225, 25}})
    {}


    void plotGraphs(const QVector<QVector<double>>& resultsets, const QVector<QString>& resultnames, PlotMode mode, QDateTime date);
private:
    QCustomPlot *plot_;
    QTextBrowser *resultsInfo_;

    QVector<QColor> graphColors_;
};

#endif // PLOTTER_H
