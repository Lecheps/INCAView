#include "plotter.h"


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

void Plotter::addToCache(const QVector<int>& newIDs, const QVector<QVector<double>>& newResultsets)
{
    for(int i = 0; i < newResultsets.count(); ++i)
    {
        int ID = newIDs[i];
        cache_[ID] = newResultsets[i]; //NOTE: Vector copy
    }
}

void Plotter::clearPlots()
{
    plot_->clearPlottables();
    plot_->replot();
    resultsInfo_->clear();
}

void Plotter::plotGraphs(const QVector<int>& IDs, const QVector<QString>& resultnames, PlotMode mode, QDateTime date)
{
    //NOTE: Right now we just throw out all previous graphs and re-create everything. We could keep track of which ID corresponds to which graph (in which plot mode)
    // and then only update/create the graphs that have changed. However, this should only be necessary if this routine runs very slowly on some user machines.
    plot_->clearPlottables();
    resultsInfo_->clear();

    int64_t starttime = date.toSecsSinceEpoch();

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

                double mean = 0;
                double min = std::numeric_limits<double>::max();
                double max = std::numeric_limits<double>::min();
                for(double d : yval)
                {
                    min = std::min(min, d);
                    max = std::max(max, d);
                    mean += d;
                }
                mean /= (double)cnt;
                double stddev = 0;
                for(double d : yval) stddev += (d - mean)*(d - mean);
                stddev = std::sqrt(stddev / (double) cnt);

                resultsInfo_->append(QString(
                        "%1 <font color=%2>&#9608;&#9608;</font><br/>"
                        "min: %3<br/>"
                        "max: %4<br/>"
                        "average: %5<br/>"
                        "standard deviation: %6<br/>"
                        "<br/>"
                      ).arg(resultnames[i], color.name())
                       .arg(min, 0, 'g', 5)
                       .arg(max, 0, 'g', 5)
                       .arg(mean, 0, 'g', 5)
                       .arg(stddev, 0, 'g', 5)
                );

                QCPGraph* graph = plot_->addGraph();
                graph->setPen(QPen(color));

                min = std::numeric_limits<double>::max();
                max = std::numeric_limits<double>::min();

                if(mode == PlotMode_YearlyAverages)
                {
                    QDateTime workingdate = date;
                    QVector<double> displayedx, displayedy;
                    min = std::numeric_limits<double>::max();
                    max = std::numeric_limits<double>::min();

                    int prevyear = workingdate.date().year();
                    double sum = 0;
                    int dayscnt = 0;
                    for(int j = 0; j < cnt; ++j)
                    {
                        sum += yval[j];
                        dayscnt++;
                        int curyear = workingdate.date().year();
                        if((curyear != prevyear || (j == cnt-1)) && dayscnt > 0)
                        {
                            double value = sum / (double) dayscnt;
                            displayedy.push_back(value);
                            displayedx.push_back(QDateTime(QDate(prevyear, 1, 1)).toSecsSinceEpoch());
                            min = std::min(min, value);
                            max = std::max(max, value);

                            sum = 0;
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
                    min = std::numeric_limits<double>::max();
                    max = std::numeric_limits<double>::min();

                    QDateTime workingdate = date;
                    int prevmonth = workingdate.date().month();
                    int prevyear = workingdate.date().year();
                    double sum = 0;
                    int dayscnt = 0;
                    for(int j = 0; j < cnt; ++j)
                    {
                        sum += yval[j];
                        dayscnt++;
                        int curmonth = workingdate.date().month();
                        if((curmonth != prevmonth || (j == cnt-1)) && dayscnt > 0)
                        {
                            double value = sum / (double) dayscnt;
                            displayedy.push_back(value);
                            displayedx.push_back(QDateTime(QDate(prevyear, prevmonth, 1)).toSecsSinceEpoch());
                            min = std::min(min, value);
                            max = std::max(max, value);

                            sum = 0;
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
                    QVector<double> displayedx(cnt);
                    for(int j = 0; j < cnt; ++j)
                    {
                        double value = yval[j];
                        displayedx[j] = (double)(starttime + 24*3600*j);
                        min = std::min(min, value);
                        max = std::max(max, value);
                    }

                    QSharedPointer<QCPAxisTickerDateTime> dateTicker(new QCPAxisTickerDateTime);
                    dateTicker->setDateTimeFormat("d. MMMM\nyyyy");
                    plot_->xAxis->setTicker(dateTicker);

                    graph->setData(displayedx, yval, true);
                }

                maxyrange = std::max(max, maxyrange);
                minyrange = std::min(min, minyrange);
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

            const QVector<double>& observed = cache_[ID0];
            const QVector<double>& modeled = cache_[ID1];
            QString observedName = resultnames[0];
            QString modeledName = resultnames[1];

            if(observed.empty() || modeled.empty()) return;

            //TODO: Should we give a warning if the count of the two sets are not equal?
            int count = std::min(observed.count(), modeled.count());

            QVector<double> errors(count);
            QVector<double> xval(count);
            QVector<double> abserrors(count);

            double meanobserved = 0.0;
            double meanx = 0.0;

            for(int i = 0; i < count; ++i)
            {
                xval[i] = (double)(starttime + 24*3600*i);
                meanobserved += observed[i];
                meanx += xval[i];
            }
            meanobserved /= (double)count;
            meanx /= (double)count;

            double min = std::numeric_limits<double>::max();
            double max = std::numeric_limits<double>::min();

            double sumerror = 0.0;
            double sumabsoluteerror = 0.0;
            double sumsquarederror = 0.0;
            double observedvariance = 0.0;
            for(int i = 0; i < count; ++i)
            {
                errors[i] = observed[i] - modeled[i];
                sumerror += errors[i];
                sumabsoluteerror += std::abs(errors[i]);
                sumsquarederror += errors[i]*errors[i];
                observedvariance += (observed[i] - meanobserved)*(observed[i] - meanobserved);

                min = errors[i] < min ? errors[i] : min;
                max = errors[i] > max ? errors[i] : max;
            }
            double meanerror = sumerror / (double)count;
            double meanabsoluteerror = sumabsoluteerror / (double)count;
            double meansquarederror = sumsquarederror / (double)count;

            double nashsutcliffe = 1.0 - sumsquarederror / observedvariance; //TODO: check for observedvariance==0

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

            double xvariance = 0.0;
            double errorvariance = 0.0;
            double xycovariance = 0.0;
            for(int i = 0; i < count; ++i)
            {
                xvariance += (xval[i] - meanx)*(xval[i] - meanx);
                errorvariance += (errors[i] - meanerror)*(errors[i] - meanerror);
                xycovariance += (xval[i] - meanx)*(errors[i] - meanerror);
            }
            //double sigma = std::sqrt(errorvariance);

            //TODO: Make all the error plots nicer when the error is very small.
            double epsilon = 1e-6; //TODO: Find a less arbitrary epsilon.

            if(mode == PlotMode_Error)
            {
                QSharedPointer<QCPAxisTickerDateTime> dateTicker(new QCPAxisTickerDateTime);
                dateTicker->setDateTimeFormat("d. MMMM\nyyyy");
                plot_->xAxis->setTicker(dateTicker);

                QCPGraph* graph = plot_->addGraph();
                if(max - min > epsilon)
                    plot_->yAxis->setRange(min, max);
                else
                    plot_->yAxis->setRange(min - epsilon, min + epsilon);
                plot_->xAxis->setRange(xval.first(), xval.last());
                graph->setData(xval, errors, true);


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
                QVector<double> sortederrors = errors;
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
                double range = (max - min) / (double) numbins;

                for(int i = 0; i < numbins; ++i)
                {
                    values[i] = min + range*((double)i + 0.5);
                }

                double binmax = 0.0;
                double invcount = 1.0 / (double)count;
                for(int i = 0; i < count; ++i)
                {
                    int bin = 0;

                    if(range > epsilon) bin = (int)floor((errors[i]-min)/range);
                    if(bin >= numbins) bin = numbins - 1; //NOTE: will happen with the max value;
                    if(bin < 0) bin = 0; //NOTE: SHOULD not happen.
                    valuebins[bin] += invcount;
                    binmax = binmax < valuebins[bin] ? valuebins[bin] : binmax;
                }

                plot_->yAxis->setRange(0.0, binmax);
                if(max - min > epsilon)
                    plot_->xAxis->setRange(min, max);
                else
                    plot_->xAxis->setRange(min, min + epsilon);

                QSharedPointer<QCPAxisTickerFixed> ticker(new QCPAxisTickerFixed);
                int tickcount = 10;
                ticker->setTickCount(tickcount);
                ticker->setTickStep(ceil((max - min)/(double)tickcount));
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

