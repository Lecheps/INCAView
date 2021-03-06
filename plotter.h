#ifndef PLOTTER_H
#define PLOTTER_H

#include "qcustomplot.h"
#include <unordered_map>

enum PlotMode
{
    PlotMode_Daily,
    PlotMode_DailyNormalized,
    PlotMode_MonthlyAverages,
    PlotMode_YearlyAverages,
    PlotMode_Error,
    PlotMode_ErrorHistogram,
    PlotMode_ErrorNormalProbability,
};

class Plotter
{

protected:
    QCPRange xrange_;
    bool isSetXrange_;
public:
    Plotter(QCustomPlot *plot, QTextBrowser *resultsInfo)
        : plot_(plot), resultsInfo_(resultsInfo),

          graphColors_({{0, 130, 200}, {230, 25, 75}, {60, 180, 75}, {245, 130, 48}, {145, 30, 180},
                        {70, 240, 240}, {240, 50, 230}, {210, 245, 60}, {250, 190, 190}, {0, 128, 128}, {230, 190, 255},
                        {170, 110, 40}, {128, 0, 0}, {170, 255, 195}, {128, 128, 0}, {255, 215, 180}, {0, 0, 128}, {255, 225, 25}})
    {
        xrange_.lower = 0.;
        xrange_.upper = 1.;
        plot_->xAxis->setRange(0.,1.);
        plot_->yAxis->setRange(0.,1.);
        isSetXrange_ = false;
    }


    void filterUncachedIDs(const QVector<int>& IDs, QVector<int>& uncachedOut);
    void plotGraphs(const QVector<int>& IDs, const QVector<QString>& resultnames, PlotMode mode, QVector<bool> &scatter, bool logarithmicY);

    void addToCache(const QVector<int>& newIDs, const QVector<QVector<double>>& newResultsets, const QVector<int64_t>& startDates);
    void clearCache() { cache_.clear(); startDateCache_.clear(); }
    void clearPlots();

    void setXrange(QCPRange);

    QVector<int> currentPlottedIDs_;

    std::unordered_map<int, QVector<double>> cache_; //NOTE: We want to be able to access this from the mainwindow, and I can't be bothered to write accessors for it.
    std::unordered_map<int, int64_t> startDateCache_; //NOTE: For now we just store the start date for the plots and assume daily values. This should probably be improved eventually.
private:
    QCustomPlot *plot_;
    QTextBrowser *resultsInfo_;

    QVector<QColor> graphColors_;
};

#endif // PLOTTER_H
