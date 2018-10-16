#include "plotter.h"

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/min.hpp>
#include <boost/accumulators/statistics/max.hpp>
#include <boost/accumulators/statistics/variance.hpp>
#include <boost/accumulators/statistics/covariance.hpp>
#include <boost/accumulators/statistics/variates/covariate.hpp>

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

void Plotter::addToCache(const QVector<int>& newIDs, const QVector<QVector<double>>& newResultsets, int64_t startDate)
{
    for(int i = 0; i < newResultsets.count(); ++i)
    {
        int ID = newIDs[i];
        cache_[ID] = newResultsets[i]; //NOTE: Vector copy
    }

    startDate_ = startDate;
}

void Plotter::clearPlots()
{
    plot_->clearPlottables();
    plot_->replot();
    resultsInfo_->clear();
}

void Plotter::plotGraphs(const QVector<int>& IDs, const QVector<QString>& resultnames, PlotMode mode)
{
    //NOTE: Right now we just throw out all previous graphs and re-create everything. We could keep track of which ID corresponds to which graph (in which plot mode)
    // and then only update/create the graphs that have changed. However, this should only be necessary if this routine runs very slowly on some user machines.
    plot_->clearPlottables();
    resultsInfo_->clear();

    currentPlottedIDs_ = IDs;

    if(mode == PlotMode_Daily || mode == PlotMode_MonthlyAverages || mode == PlotMode_YearlyAverages)
    {
        double minyrange = 0.0;
        double maxyrange = 0.0;

        int firstunassignedcolor = 0;

        for(int i = 0; i < IDs.count(); ++i)
        {

            int ID = IDs[i];

            const QVector<double>& yval = cache_[ID];

            if(yval.empty()) continue; //TODO: Log warning?

            int cnt = yval.count();

            if(cnt != 0)
            {

                QColor& color = graphColors_[firstunassignedcolor++];
                if(firstunassignedcolor == graphColors_.count()) firstunassignedcolor = 0; // Cycle the colors


                using namespace boost::accumulators;
                accumulator_set<double, features<tag::min, tag::max, tag::mean, tag::variance>> acc;
                for(double d : yval) acc(d);

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
                    QDateTime workingdate = QDateTime::fromSecsSinceEpoch(startDate_, Qt::OffsetFromUTC, 0);
                    QVector<double> displayedx, displayedy;

                    int prevyear = workingdate.date().year();

                    int dayscnt = 0;
                    accumulator_set<double, features<tag::mean>> localAcc;

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

                            localAcc = accumulator_set<double, features<tag::mean>>(); //NOTE: Reset it.
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

                    QDateTime workingdate = QDateTime::fromSecsSinceEpoch(startDate_, Qt::OffsetFromUTC, 0);
                    int prevmonth = workingdate.date().month();
                    int prevyear = workingdate.date().year();

                    accumulator_set<double, features<tag::mean>> localAcc;
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

                            localAcc = accumulator_set<double, features<tag::mean>>(); //NOTE: Reset it.
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
                else //mode == PlotMode_Daily
                {
                    graphmin = min(acc);
                    graphmax = max(acc);

                    QVector<double> displayedx(cnt);
                    for(int j = 0; j < cnt; ++j)
                    {
                        double value = yval[j];
                        displayedx[j] = (double)(startDate_ + 24*3600*j);
                    }

                    QSharedPointer<QCPAxisTickerDateTime> dateTicker(new QCPAxisTickerDateTime);
                    dateTicker->setDateTimeFormat("d. MMMM\nyyyy");
                    plot_->xAxis->setTicker(dateTicker);

                    graph->setData(displayedx, yval, true);
                }

                maxyrange = std::max(graphmax, maxyrange);
                minyrange = std::min(graphmin, minyrange);
                if(maxyrange - minyrange < QCPRange::minRange)
                {
                    maxyrange = minyrange + 2.0*QCPRange::minRange;
                }
                plot_->yAxis->setRange(minyrange, maxyrange);
                bool foundrange;
                plot_->xAxis->setRange(graph->data()->keyRange(foundrange));
            }
        }
    }
    else //mode == PlotMode_Error || mode == PlotMode_ErrorNormalProbability || mode == PlotMode_ErrorHistogram
    {
        if(IDs.count() == 2)
        {
            int ID0 = IDs[0];
            int ID1 = IDs[1];

            const QVector<double>& modeled = cache_[ID0];
            const QVector<double>& observed = cache_[ID1];

            QString modeledName = resultnames[0];
            QString observedName = resultnames[1];

            if(observed.empty() || modeled.empty()) return;

            //TODO: Should we give a warning if the count of the two sets are not equal?
            int count = std::min(observed.count(), modeled.count());

            QVector<double> residuals(count);
            QVector<double> xval(count);

            //TODO: We should expect the observed series to have potential NaN values (missing data)..
            // That should be handled properly.

            using namespace boost::accumulators;

            //TODO: Do we really need separate accumulators for residual, absolute residual and squared residual? Could they be handled by one accumulator?
            accumulator_set<double, features<tag::variance>> obsacc;
            accumulator_set<double, features<tag::mean, tag::min, tag::max>>  residualacc;
            accumulator_set<double, features<tag::mean>> residualabsacc;
            accumulator_set<double, features<tag::sum, tag::mean>> residualsquareacc;
            accumulator_set<double, features<tag::mean, tag::variance, tag::covariance<double, tag::covariate1>>> xacc;

            for(int i = 0; i < count; ++i)
            {
                obsacc(observed[i]);

                double residual = observed[i] - modeled[i];
                residuals[i] = residual;

                residualacc(residual);
                residualabsacc(std::abs(residual));
                residualsquareacc(residual*residual);

                xval[i] = (double)(startDate_ + 24*3600*i);
                xacc(xval[i], covariate1=residual);
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
            double sumsquarederror   = sum(residualsquareacc);

            double nashsutcliffe = 1.0 - sumsquarederror / observedvariance; //TODO: check for observedvariance==0 ?

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
                QVector<double> sortederrors = residuals;
                qSort(sortederrors.begin(), sortederrors.end());
                QVector<double> expected(count);

                double a = count <= 10 ? 3.0/8.0 : 0.5;
                for(int i = 0; i < count; ++i)
                {
                    expected[i] = NormalCDFInverse(((double)(i+1) - a)/((double)count + 1 - 2.0*a));
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

                    if(range > epsilon) bin = (int)floor((residuals[i]-minres)/range);
                    if(bin >= numbins) bin = numbins - 1; //NOTE: will happen with the max value;
                    if(bin < 0) bin = 0; //NOTE: SHOULD not happen.
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

