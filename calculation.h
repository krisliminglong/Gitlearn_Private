#ifndef CALCULATION_H
#define CALCULATION_H
#include<QObject>
#include <Eigen/Dense>
#include<QThread>
#include<QDebug>
#include<QElapsedTimer>
#include <vector>
#include <cmath>
#include <algorithm>
#include <random>

class DataCalculation: public QObject
{
     Q_OBJECT
public:
    explicit DataCalculation(QObject *parent = nullptr);
    std::pair<Eigen::MatrixXd, Eigen::MatrixXd> createMatrix(int num_blade, int order) ;
    std::tuple<double, Eigen::VectorXd, Eigen::VectorXd> fit(
                               double sampling,double radius,
                               const Eigen::MatrixXd& mat_C,
                               const Eigen::MatrixXd& mat_B,
                               const std::vector<uint64_t>& dataConst,
                                 int num_blade);
    //参数识别
    void VibParaIdentify(std::vector<uint64_t> data_useful, int blade_num, int sampling, double Radius);
public slots:
    void ProcessAllChannelsData(const std::vector<std::vector<uint64_t>>& allData);
    void ProcessAllRiseAndFallData(const std::vector<std::vector<uint64_t>>& RiseData,const std::vector<std::vector<uint64_t>>& FallData);
    void SetParameter(int num_blade1,double Radius1,int SampleRate1);
    void SetIndexZero();

public:
    int FirstFitOrder;//SG滤波的拟合阶数，这个值为1，用于监测使用
    Eigen::MatrixXd mat_C;//一阶拟合，计算转速使用
    Eigen::MatrixXd mat_B;//一阶拟合下的计算理论时间，有误差

    int SeconedFitOrder;
    Eigen::MatrixXd SeconedFit_Speed_matrix;//二阶拟合下的转速计算
    Eigen::MatrixXd Theory_Time_matrix;//二阶拟合，计算理论时间使用

    QString matrixToQString(const Eigen::MatrixXd& matrix);//打印矩阵用的

    std::vector<std::vector<uint64_t>> ChannelDataAccumulator; // 每个通道的数据累积器（）
    QVector<QVector<QVector<double>>> vibrationData;
    QVector<QVector<double>> speedData;
    QVector<QVector<double>> cycleData;
    QVector<int> cycleCounter;

    //降噪用的变量
    std::vector<std::vector<uint64_t>> riseDataBuffer;//上升沿的数据积累器
    std::vector<std::vector<uint64_t>> fallDataBuffer;//下降沿的数据积累器
    std::vector<bool> InstableDataTrace; // 用于跟踪每个通道是否已移除前2000个不稳定数据
    std::vector<std::vector<uint64_t>> DeleteNoiseRiseData;//存储去除毛刺后的上升沿数据
    std::vector<std::vector<uint64_t>> DeleteNoiseFallData;//存储去除毛刺后的下降沿数据
    std::vector<std::vector<uint64_t>> FallData_Finall;//用于计算的下降沿信号

    std::tuple<std::vector<uint64_t>, std::vector<uint64_t>> DeleteNoiseData//利用叶片厚度来降噪
                               (const std::vector<uint64_t>& riseTimes,
                                const std::vector<uint64_t>& fallTimes);

    void remove_large_gap_pulses(std::vector<uint64_t> &data,//利用叶片之间的间距来降噪
                                                 std::vector<uint64_t> &del_noise_data,
                                                 size_t window_size = 40,
                                                 double threshold_min = 0.8,
                                                 double threshold_max = 2,
                                                 double interval_threshold_factor = 2.5);

    void filterVibrationData(QVector<QVector<double>>& vibrationData);//去除不对的振动位移


    QElapsedTimer elapsedTimer;

    int num_blade;
    int SampleRate;
    double Radius;

signals:
    void dataReady(const QVector<QVector<QVector<double>>>& vibrationData,
                   const QVector<QVector<double>>& speedData,
                   const QVector<QVector<double>>& cycleData);

    void SendVibData(
               const QVector<QVector<double>> &vibration1,
               const QVector<QVector<double>> &Speed_Frenquence,
               const QVector<QVector<double>> &Roll_Cycle_Number,
               const QVector<QVector<double>> &Remove_Constant_Bias_vib,
               const QVector<QVector<double>> &Remove_Bias_origin_vib);

};

#endif // CALCULATION_H
