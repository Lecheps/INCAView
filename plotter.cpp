#include "plotter.h"

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/min.hpp>
#include <boost/accumulators/statistics/max.hpp>
#include <boost/accumulators/statistics/variance.hpp>
#include <boost/accumulators/statistics/covariance.hpp>
#include <boost/accumulators/statistics/variates/covariate.hpp>
#include <limits>

double NormalCDFInverse(double p);
double NormalCDF(double x);


void Plotter::filterUncachedIDs(const QVector<int>& IDs, QVector<int>& uncachedOut)
{
    for(int ID : IDs)
    {
        auto find = cache_.find(ID);
        if(find == cache_.end()) uncachedOut.push_back(ID);
    }
}

void Plotter::addToCache(const QVector<int>& newIDs, const QVector<QVector<double>>& newResultsets, const QVector<int64_t>& startDates)
{
    for(int i = 0; i < newResultsets.count(); ++i)
    {
        int ID = newIDs[i];
        cache_[ID] = newResultsets[i]; //NOTE: Vector copy
        startDateCache_[ID] = startDates[i];
    }
}

void Plotter::clearPlots()
{
    plot_->clearPlottables();
    plot_->replot();
    resultsInfo_->clear();
}

void Plotter::plotGraphs(const QVector<int>& IDs, const QVector<QString>& resultnames, PlotMode mode, QVector<bool> &scatter, bool logarithmicY)
{
    //NOTE: Right now we just throw out all previous graphs and re-create everything. We could keep track of which ID corresponds to which graph (in which plot mode)
    // and then only update/create the graphs that have changed. However, this should only be necessary if this routine runs very slowly on some user machines.
    plot_->clearPlottables();
    resultsInfo_->clear();

    currentPlottedIDs_ = IDs;

    if(logarithmicY &&
        (mode == PlotMode_Daily || mode == PlotMode_MonthlyAverages || mode == PlotMode_YearlyAverages || mode == PlotMode_DailyNormalized || mode == PlotMode_Error)
    )
    {
        plot_->yAxis->setScaleType(QCPAxis::stLogarithmic);
    }
    else
    {
        plot_->yAxis->setScaleType(QCPAxis::stLinear);
    }

    if(mode == PlotMode_Daily || mode == PlotMode_MonthlyAverages || mode == PlotMode_YearlyAverages || mode == PlotMode_DailyNormalized)
    {
        double minyrange = 0.0;
        double maxyrange = 0.0;

        int firstunassignedcolor = 0;

        for(int i = 0; i < IDs.count(); ++i)
        {

            int ID = IDs[i];

            const QVector<double>& yval = cache_[ID];

            int64_t startDate = startDateCache_[ID];

            if(yval.empty()) continue; //TODO: Log warning?

            int cnt = yval.count();

            if(cnt != 0)
            {

                QColor& color = graphColors_[firstunassignedcolor++];
                if(firstunassignedcolor == graphColors_.count()) firstunassignedcolor = 0; // Cycle the colors


                using namespace boost::accumulators;
                accumulator_set<double, stats<tag::min, tag::max, tag::mean, tag::variance>> acc;
                for(double d : yval)
                {
                    if(!std::isnan(d))
                        acc(d);
                }

                resultsInfo_->append(QString(
                        "%1 <font color=%2>&#9608;&#9608;</font><br/>"
                        "min: %3<br/>"
                        "max: %4<br/>"
                        "average: %5<br/>"
                        "standard deviation: %6<br/>"
                        "<br/>"
                      ).arg(resultnames[i], color.name())
                       .arg(min(acc), 0, 'g', 5)
                       .arg(max(acc), 0, 'g', 5)
                       .arg(mean(acc), 0, 'g', 5)
                       .arg(std::sqrt(variance(acc)), 0, 'g', 5)
                );

                QCPGraph* graph = plot_->addGraph();
                graph->setPen(QPen(color));

                double graphmin = std::numeric_limits<double>::max();;
                double graphmax = std::numeric_limits<double>::min();

                if(mode == PlotMode_YearlyAverages)
                {
                    QDateTime workingdate = QDateTime::fromSecsSinceEpoch(startDate, Qt::OffsetFromUTC, 0);
                    QVector<double> displayedx, displayedy;

                    int prevyear = workingdate.date().year();

                    int dayscnt = 0;
                    accumulator_set<double, stats<tag::mean>> localAcc;

                    for(int j = 0; j < cnt; ++j)
                    {
                        localAcc(yval[j]);
                        dayscnt++;
                        int curyear = workingdate.date().year();
                        if((curyear != prevyear || (j == cnt-1)) && dayscnt > 0)
                        {
                            double value = mean(localAcc);
                            displayedy.push_back(value);
                            displayedx.push_back(QDateTime(QDate(prevyear, 1, 1), QTime(), Qt::OffsetFromUTC, 0).toSecsSinceEpoch());
                            graphmin = std::min(graphmin, value);
                            graphmax = std::max(graphmax, value);

                            localAcc = accumulator_set<double, stats<tag::mean>>(); //NOTE: Reset it.
                            dayscnt = 0;
                            prevyear = curyear;
                        }
                        workingdate = workingdate.addDays(1);
                    }
                    QSharedPointer<QCPAxisTickerDateTime> dateTicker(new QCPAxisTickerDateTime);
                    dateTicker->setDateTimeFormat("yyyy");
                    plot_->xAxis->setTicker(dateTicker);

                    graph->setData(displayedx, displayedy, true);
                }
                else if(mode == PlotMode_MonthlyAverages)
                {
                    QVector<double> displayedx, displayedy;

                    QDateTime workingdate = QDateTime::fromSecsSinceEpoch(startDate, Qt::OffsetFromUTC, 0);
                    int prevmonth = workingdate.date().month();
                    int prevyear = workingdate.date().year();

                    accumulator_set<double, stats<tag::mean>> localAcc;
                    //double sum = 0;
                    int dayscnt = 0;
                    for(int j = 0; j < cnt; ++j)
                    {
                        localAcc(yval[j]);
                        //sum += yval[j];
                        dayscnt++;
                        int curmonth = workingdate.date().month();
                        if((curmonth != prevmonth || (j == cnt-1)) && dayscnt > 0)
                        {
                            double value = mean(localAcc);
                            displayedy.push_back(value);
                            displayedx.push_back(QDateTime(QDate(prevyear, prevmonth, 1), QTime(), Qt::OffsetFromUTC, 0).toSecsSinceEpoch());
                            graphmin = std::min(graphmin, value);
                            graphmax = std::max(graphmax, value);

                            localAcc = accumulator_set<double, stats<tag::mean>>(); //NOTE: Reset it.
                            dayscnt = 0;
                            prevmonth = curmonth;
                            prevyear = workingdate.date().year();
                        }
                        workingdate = workingdate.addDays(1);
                    }
                    QSharedPointer<QCPAxisTickerDateTime> dateTicker(new QCPAxisTickerDateTime);
                    dateTicker->setDateTimeFormat("MMMM\nyyyy");
                    plot_->xAxis->setTicker(dateTicker);

                    graph->setData(displayedx, displayedy, true);
                }
                else if ( mode == PlotMode_DailyNormalized)
                {
//                    double currentMean = mean(acc);
                    double range = max(acc) - min(acc);
                    graphmin = 0.;
                    graphmax = 1.;
                    QVector<double> displayedx(cnt);
                    QVector<double> displayedy(cnt);
                    for(int j = 0; j < cnt; ++j)
                    {
                        displayedy[j] = (yval[j] - min(acc)) / range ;
                        displayedx[j] = (double)(startDate + 24*3600*j);
                    }
                    QSharedPointer<QCPAxisTickerDateTime> dateTicker(new QCPAxisTickerDateTime);
                    dateTicker->setDateTimeFormat("d. MMMM\nyyyy");
                    plot_->xAxis->setTicker(dateTicker);

                    if(scatter[i])
                    {
                        graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, QPen(Qt::black, 1.5), QBrush(Qt::white), 9));
                    }

                    graph->setData(displayedx, displayedy, true);

                }
                else //mode == PlotMode_Daily
                {
                    graphmin = min(acc);
                    graphmax = max(acc);

                    QVector<double> displayedx(cnt);
                    for(int j = 0; j < cnt; ++j)
                    {
                        double value = yval[j];
                        displayedx[j] = (double)(startDate + 24*3600*j);
                    }

                    QSharedPointer<QCPAxisTickerDateTime> dateTicker(new QCPAxisTickerDateTime);
                    dateTicker->setDateTimeFormat("d. MMMM\nyyyy");
                    plot_->xAxis->setTicker(dateTicker);

                    if(scatter[i])
                    {
                        graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, QPen(Qt::black, 1.5), QBrush(Qt::white), 9));
                    }

                    graph->setData(displayedx, yval, true);
                }


                maxyrange = std::max(graphmax, maxyrange);
                minyrange = std::min(graphmin, minyrange);
                if(maxyrange - minyrange < QCPRange::minRange)
                {
                    maxyrange = minyrange + 2.0*QCPRange::minRange;
                }
                plot_->yAxis->setRange(minyrange, maxyrange);

                if (!isSetXrange_)
                { 
                    bool foundrange;
                    QCPRange range = graph->data()->keyRange(foundrange);
                    plot_->xAxis->setRange(range);
                    xrange_ = range;
                    isSetXrange_ = true;
                }
                else
                {
                    plot_->xAxis->setRange(xrange_);
                }
            }
        }
    }
    else //mode == PlotMode_Error || mode == PlotMode_ErrorNormalProbability || mode == PlotMode_ErrorHistogram
    {
        if(IDs.count() == 2)
        {
            int ID0 = IDs[0];
            int ID1 = IDs[1];

            int64_t startDatemod = startDateCache_[ID0];
            int64_t startDateobs = startDateCache_[ID1];
            int64_t startDate = std::max(startDatemod, startDateobs);
            int64_t alignmod = 0;
            int64_t alignobs = 0;
            if(startDatemod < startDateobs)
            {
                alignmod = (startDateobs-startDatemod)/86400; //Again, this assumes one-day-timesteps...
            }
            else if(startDateobs < startDatemod)
            {
                alignobs = (startDatemod-startDateobs)/86400; //Again, this assumes one-day-timesteps...
            }

            const QVector<double>& modeled = cache_[ID0];
            const QVector<double>& observed = cache_[ID1];


            //TODO: we actually need to compute an offset here to align them!

            QString modeledName = resultnames[0];
            QString observedName = resultnames[1];

            if(observed.empty() || modeled.empty()) return;

            //TODO: Should we give a warning if the count of the two sets are not equal?
            int count = std::min((int64_t)observed.count()-alignobs, modeled.count()-alignmod);

            QVector<double> residuals(count);
            QVector<double> xval(count);

            using namespace boost::accumulators;

            //TODO: Do we really need separate accumulators for residual, absolute residual and squared residual? Could they be handled by one accumulator?
            accumulator_set<double, stats<tag::variance>> obsacc;
            accumulator_set<double, stats<tag::mean, tag::min, tag::max>>  residualacc;
            accumulator_set<double, stats<tag::mean>> residualabsacc;
            accumulator_set<double, stats<tag::mean>> residualsquareacc;
            accumulator_set<double, stats<tag::mean, tag::variance, tag::covariance<double, tag::covariate1>>> xacc;

            for(int i = 0; i < count; ++i)
            {
                xval[i] = (double)(startDate + 24*3600*i);
                double mod = modeled[i+alignmod];
                double obs = observed[i+alignobs];

                if(!std::isnan(mod) && !std::isnan(obs))
                {
                    double residual = obs - mod;
                    residuals[i] = residual;

                    obsacc(obs);
                    residualacc(residual);
                    residualabsacc(std::abs(residual));
                    residualsquareacc(residual*residual);

                    xacc(xval[i], covariate1=residual);
                }
                else
                {
                    residuals[i] = std::numeric_limits<double>::quiet_NaN();
                }
            }

            double observedvariance = variance(obsacc);

            double meanx        = mean(xacc);
            double xvariance    = variance(xacc);
            double xycovariance = covariance(xacc);

            double minres = min(residualacc);
            double maxres = max(residualacc);

            double meanerror         = mean(residualacc);
            double meanabsoluteerror = mean(residualabsacc);
            double meansquarederror  = mean(residualsquareacc);

            //TODO: There looks like there is something wrong with the Nash-Sutcliffe coefficient, but I don't know what!

            double nashsutcliffe = 1.0 - meansquarederror / observedvariance;

            resultsInfo_->append(QString(
                    "Observed: %1, vs Modeled: %2<br/>"
                    "mean error (bias): %3<br/>"
                    "mean absolute error: %4<br/>"
                    "mean squared error: %5<br/>"
                    "Nash-Sutcliffe: %6<br/>"
                  ).arg(observedName, modeledName)
                   .arg(meanerror, 0, 'g', 5)
                   .arg(meanabsoluteerror, 0, 'g', 5)
                   .arg(meansquarederror, 0, 'g', 5)
                   .arg(nashsutcliffe, 0, 'g', 5)
            );

            //TODO: Make all the error plots nicer when the error is very small.
            double epsilon = 1e-6; //TODO: Find a less arbitrary epsilon.

            if(mode == PlotMode_Error)
            {
                QSharedPointer<QCPAxisTickerDateTime> dateTicker(new QCPAxisTickerDateTime);
                dateTicker->setDateTimeFormat("d. MMMM\nyyyy");
                plot_->xAxis->setTicker(dateTicker);

                QCPGraph* graph = plot_->addGraph();
                if(maxres - minres > epsilon)
                    plot_->yAxis->setRange(minres, maxres);
                else
                    plot_->yAxis->setRange(minres - epsilon, minres + epsilon);
                plot_->xAxis->setRange(xval.first(), xval.last());
                graph->setData(xval, residuals, true);


                //TODO: The linear regression may not be correct if there is a lot of missing observed data.
                double beta = xycovariance / xvariance;
                double alpha = meanerror - beta * meanx;
                QVector<double> linearError(count);
                for(int i = 0; i < count; ++i)
                {
                    linearError[i] = alpha + beta*xval[i];
                }

                QCPGraph* regressiongraph = plot_->addGraph();
                QPen redDotPen;
                redDotPen.setColor(Qt::red);
                redDotPen.setStyle(Qt::DotLine);
                redDotPen.setWidthF(2);
                regressiongraph->setPen(redDotPen);

                regressiongraph->setData(xval, linearError, true);
            }
            else if(mode == PlotMode_ErrorNormalProbability)
            {
                QVector<double> sortederrors;
                sortederrors.reserve(residuals.size());
                for(double D : residuals)
                {
                    if(!std::isnan(D)) sortederrors.push_back(D);
                }
                qSort(sortederrors.begin(), sortederrors.end());
                QVector<double> expected(sortederrors.size());

                double a = sortederrors.size() <= 10 ? 3.0/8.0 : 0.5;
                for(int i = 0; i < sortederrors.size(); ++i)
                {
                    expected[i] = NormalCDFInverse(((double)(i+1) - a)/((double)sortederrors.size() + 1 - 2.0*a));
                    //sortederrors[i] = (sortederrors[i] - meanerror) / sigma;
                }

                QCPGraph* graph = plot_->addGraph();

                graph->setPen(QPen(Qt::blue));
                graph->setLineStyle(QCPGraph::lsNone);
                graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, 4));

                if(sortederrors.last() - sortederrors.first() > epsilon)
                {
                    plot_->yAxis->setRange(sortederrors.first(), sortederrors.last());
                }
                else
                {
                    plot_->yAxis->setRange(sortederrors.first(), sortederrors.first() + epsilon);
                }

                plot_->xAxis->setRange(expected.first(), expected.last());
                QSharedPointer<QCPAxisTickerFixed> ticker(new QCPAxisTickerFixed);
                plot_->xAxis->setTicker(ticker); //NOTE: We have to reset the ticker in case it was set to a date ticker previously

                graph->setData(expected, sortederrors, true);

            }
            else if(mode == PlotMode_ErrorHistogram)
            {
                int numbins = 20; //TODO: How to determine a good number of bins?
                QVector<double> valuebins(numbins);
                QVector<double> values(numbins);
                double range = (maxres - minres) / (double) numbins;

                for(int i = 0; i < numbins; ++i)
                {
                    values[i] = minres + range*((double)i + 0.5);
                }

                double binmax = 0.0;
                double invcount = 1.0 / (double)count;
                for(int i = 0; i < count; ++i)
                {
                    int bin = 0;

                    if(std::isnan(residuals[i])) continue;

                    if(range > epsilon) bin = (int)floor((residuals[i]-minres)/range);
                    if(bin >= numbins) bin = numbins - 1; //NOTE: will happen with the max value;
                    if(bin < 0)
                    {
                        qDebug() << "unexpected bin for a number in histogram plot";
                        bin = 0; //NOTE: SHOULD not happen.
                    }
                    valuebins[bin] += invcount;
                    binmax = binmax < valuebins[bin] ? valuebins[bin] : binmax;
                }

                plot_->yAxis->setRange(0.0, binmax);
                if(maxres - minres > epsilon)
                    plot_->xAxis->setRange(minres, maxres);
                else
                    plot_->xAxis->setRange(minres, minres + epsilon);

                QSharedPointer<QCPAxisTickerFixed> ticker(new QCPAxisTickerFixed);
                int tickcount = 10;
                ticker->setTickCount(tickcount);
                ticker->setTickStep(ceil((maxres - minres)/(double)tickcount));
                plot_->xAxis->setTicker(ticker); //NOTE: We have to reset the ticker in case it was set to a date ticker previously

                QCPBars *bars = new QCPBars(plot_->xAxis, plot_->yAxis);
                bars->setWidth(range);
                bars->setData(values, valuebins);
            }
        }
    }
    
    plot_->replot();
}

void Plotter::setXrange(QCPRange xrange)
{
    xrange_ = xrange;
}


//NOTE: inverse normal taken from here: https://www.johndcook.com/blog/cpp_phi_inverse/ (it says that it has a do-whatever-you-want-with-it licence)

double RationalApproximation(double t)
{
    // Abramowitz and Stegun formula 26.2.23.
    // The absolute value of the error should be less than 4.5 e-4.
    double c[] = {2.515517, 0.802853, 0.010328};
    double d[] = {1.432788, 0.189269, 0.001308};
    return t - ((c[2]*t + c[1])*t + c[0]) /
               (((d[2]*t + d[1])*t + d[0])*t + 1.0);
}

double NormalCDFInverse(double p)
{
    if (p <= 0.0 || p >= 1.0)
    {
        return 0.0;
    }

    if (p < 0.5)
    {
        // F^-1(p) = - G^-1(p)
        return -RationalApproximation( sqrt(-2.0*log(p)) );
    }
    else
    {
        // F^-1(p) = G^-1(1-p)
        return RationalApproximation( sqrt(-2.0*log(1-p)) );
    }
}


//NOTE: normal taken from here: https://www.johndcook.com/blog/cpp_phi/ (open domain)
double NormalCDF(double x)
{
    // constants
    double a1 =  0.254829592;
    double a2 = -0.284496736;
    double a3 =  1.421413741;
    double a4 = -1.453152027;
    double a5 =  1.061405429;
    double p  =  0.3275911;

    // Save the sign of x
    int sign = 1;
    if (x < 0)
        sign = -1;
    x = fabs(x)/sqrt(2.0);

    // A&S formula 7.1.26
    double t = 1.0/(1.0 + p*x);
    double y = 1.0 - (((((a5*t + a4)*t) + a3)*t + a2)*t + a1)*t*exp(-x*x);

    return 0.5*(1.0 + sign*y);
}

