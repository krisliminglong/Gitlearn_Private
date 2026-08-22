#ifndef SENSORFITTING_H
#define SENSORFITTING_H

#include <Eigen/Dense>
#include <qcustomplot.h>
#include <QVector>

class SensorFitting {
public:
    SensorFitting();
    std::tuple<Eigen::MatrixXd, QVector<double>,
               QVector<double>, QVector<double>>
    fitProbe(int downlimit, int uplimit,
             const QVector<double>& sensorSpeed,
             const QVector<double>& sensorBias,
             int Q_value);

private:
    void prepareData(int downlimit, int uplimit,
                      const QVector<double>& sensorSpeed,
                      const QVector<double>& sensorBias,
                      Eigen::MatrixXd& rotatingspeed,
                      Eigen::MatrixXd& displacement,
                      Eigen::MatrixXd& fn);

    void plotFitting(const Eigen::MatrixXd& data_input,
                        const Eigen::MatrixXd& data_output,
                        const Eigen::MatrixXd& params,
                        QVector<double>& x, QVector<double>& y,
                        QVector<double>& y_fit);


    double myFunc(const Eigen::MatrixXd& params, double input_data);

    double calDeriv(const Eigen::MatrixXd& params, double input_data, int param_index);

    Eigen::MatrixXd calJacobian(const Eigen::MatrixXd& params, const Eigen::MatrixXd& input_data);

    Eigen::MatrixXd calResidual(const Eigen::MatrixXd& params,
                                const Eigen::MatrixXd& input_data,
                                const Eigen::MatrixXd& output_data);

    double getInitU(const Eigen::MatrixXd& A, double tao);
    Eigen::MatrixXd LM(int num_iter, Eigen::MatrixXd params,
                       const Eigen::MatrixXd& input_data,
                       const Eigen::MatrixXd& output_data);
};

#endif // SENSORFITTING_H
