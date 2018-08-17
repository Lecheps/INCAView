#include "plotter.h"


double NormalCDFInverse(double p);
double NormalCDF(double x);

void Plotter::plotGraphs(const QVector<QVector<double>>& resultsets, const QVector<QString>& resultnames, PlotMode mode, QDateTime date)
{
    //NOTE: Right now we just throw out all previous graphs and re-create everything. We could keep track of which ID corresponds to which graph (in which plot mode)
    // and then only update/create the graphs that have changed. However, this should only be necessary if this routine runs very slowly on some user machines.
    plot_->clearPlottables();
    resultsInfo_->clear();
    int64_t starttime = date.toSecsSinceEpoch();

    if(mode == PlotMode_Daily || mode == PlotMode_MonthlyAverages || mode == PlotMode_YearlyAverages)
    {
        double minyrange = 0.0;
        double maxyrange = 0.0;

        int firstunassignedcolor = 0;
        for(int i = 0; i < resultsets.count(); ++i)
        {
            const QVector<double>& yval = resultsets[i];
            int cnt = yval.count();
            QVector<double> xval(cnt);

            //qDebug() << "data count: " << cnt;
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

                QColor& color = graphColors_[firstunassignedcolor++];
                if(firstunassignedcolor == graphColors_.count()) firstunassignedcolor = 0; // Cycle the colors

                double mean = 0;
                for(double d : yval) mean += d;
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

                if(mode == PlotMode_YearlyAverages)
                {
                    QVector<double> displayedx, displayedy;
                    min = std::numeric_limits<double>::max();
                    max = std::numeric_limits<double>::min();

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
                    plot_->xAxis->setTicker(dateTicker);

                    graph->setData(displayedx, displayedy, true);
                }
                else if(mode == PlotMode_MonthlyAverages)
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
                    plot_->xAxis->setTicker(dateTicker);

                    graph->setData(displayedx, displayedy, true);
                }
                else
                {
                    QSharedPointer<QCPAxisTickerDateTime> dateTicker(new QCPAxisTickerDateTime);
                    dateTicker->setDateTimeFormat("d. MMMM\nyyyy");
                    plot_->xAxis->setTicker(dateTicker);

                    graph->setData(xval, yval, true);
                }

                maxyrange = maxyrange < max ? max : maxyrange;
                minyrange = minyrange > min ? min : minyrange;
                if(maxyrange - minyrange < QCPRange::minRange)
                {
                    maxyrange = minyrange + 2.0*QCPRange::minRange;
                }
                plot_->yAxis->setRange(minyrange, maxyrange);
                plot_->xAxis->setRange(xval.first(), xval.last());
            }
        }
    }
    else //mode == PlotMode_Error || mode == PlotMode_ErrorNormalProbability || mode == PlotMode_ErrorHistogram
    {
        if(resultsets.count() == 2)
        {
            const QVector<double>& observed = resultsets[0];
            const QVector<double>& modeled = resultsets[1];
            QString observedName = resultnames[0];
            QString modeledName = resultnames[1];

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
            double nashsutcliffe = 1.0 - sumsquarederror / observedvariance;

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
            double sigma = std::sqrt(errorvariance);

            if(mode == PlotMode_Error)
            {
                QSharedPointer<QCPAxisTickerDateTime> dateTicker(new QCPAxisTickerDateTime);
                dateTicker->setDateTimeFormat("d. MMMM\nyyyy");
                plot_->xAxis->setTicker(dateTicker);

                QCPGraph* graph = plot_->addGraph();
                plot_->yAxis->setRange(min, max);
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

                plot_->yAxis->setRange(sortederrors.first(), sortederrors.last());
                plot_->xAxis->setRange(expected.first(), expected.last());
                QSharedPointer<QCPAxisTickerFixed> ticker(new QCPAxisTickerFixed);
                plot_->xAxis->setTicker(ticker); //NOTE: We have to reset the ticker in case it was set to a date ticker previously

                graph->setData(expected, sortederrors, true);

            }
            else if(mode == PlotMode_ErrorHistogram)
            {
                //TODO: Have fewer bins if count is very low?
                int numbins = 20;
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
                    int bin = floor((errors[i]-min)/range);
                    if(bin >= numbins) bin = numbins - 1; //NOTE: will happen with the max value;
                    if(bin < 0) bin = 0; //NOTE: SHOULD not happen.
                    valuebins[bin] += invcount;
                    binmax = binmax < valuebins[bin] ? valuebins[bin] : binmax;
                }

                plot_->yAxis->setRange(0.0, binmax);
                plot_->xAxis->setRange(min, max);

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
        /*
        std::stringstream os;
        os << "Invalid input argument (" << p
           << "); must be larger than 0 but less than 1.";
        throw std::invalid_argument( os.str() );
        */
        return 0.0;
    }

    // See article above for explanation of this section.
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

