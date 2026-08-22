#ifndef PARAMETER_IDENTIFICATION_H
#define PARAMETER_IDENTIFICATION_H

#include <QMainWindow>
#include<qcustomplot.h>
#include <Eigen/Dense>
#include <calculation.h>
#include <vector>
#include <cstdint>
#include <fstream>
#include <QString>
#include <QMessageBox>
#include <calculation.h>
#include <lmfit.h>
namespace Ui {
class parameter_identification;
}

class parameter_identification : public QMainWindow
{
    Q_OBJECT

public:
    explicit parameter_identification(QWidget *parent = nullptr);
    ~parameter_identification();

    void FirstWidgetInitial(QTabWidget *tabWidget);
    void SecondWidgetInitial(QTabWidget *tabWidget);
    void ThirdWidgetInitial(QTabWidget *tabWidget);

    std::vector<uint64_t> readBinFile(const QString& filePath);

    void plotSingleBlade(int sensorIndex, int bladeIndex);
    void plotAllBlades(int sensorIndex);

    Eigen::MatrixXd PiErXiShu(const Eigen::VectorXd& x, const Eigen::VectorXd& y);

    void exportSingleBlade(int sensorIndex, int bladeIndex);
    void exportAllBlades(int sensorIndex);

public slots:

    void selectSensor1File();
    void selectSensor2File();
    void selectSensor3File();
    void setParameters();
    void calculateVibrationDisplacement(); // 新增计算按钮槽函数
    void BladeVidDataDraw(); // 控制单叶片还是全叶片的绘图
    void clearPlots();
    void CalSensorAngle();
    void exportData();
private:
    Ui::parameter_identification *ui;

    double samplingRate;
    int bladeCount;
    double radius;

    QCustomPlot *HaveBaisPlot;
    QCustomPlot *NoBaisPlot;

    QLineEdit *samplingRateEdit;
    QLineEdit *bladeCountEdit;
    QLineEdit *radiusEdit;

    QLineEdit *sensor1PathEdit;
    QLineEdit *sensor2PathEdit;
    QLineEdit *sensor3PathEdit;

    QString sensor1FilePath;
    QString sensor2FilePath;
    QString sensor3FilePath;

    std::vector<uint64_t> sensor1Data;
    std::vector<uint64_t> sensor2Data;
    std::vector<uint64_t> sensor3Data;

    QVector<QVector<double>> sensor1Vibration;//原始振动位移
    QVector<QVector<double>> sensor1Speed;
    QVector<QVector<double>> sensor1RollCycle;
    QVector<QVector<double>> sensor1RemoveBias;//去恒偏后加了常量
    QVector<QVector<double>> sensor1_Original_Bias;//只去了恒偏

    QVector<QVector<double>> sensor2Vibration;
    QVector<QVector<double>> sensor2Speed;
    QVector<QVector<double>> sensor2RollCycle;
    QVector<QVector<double>> sensor2RemoveBias;
    QVector<QVector<double>> sensor2_Original_Bias;

    QVector<QVector<double>> sensor3Vibration;
    QVector<QVector<double>> sensor3Speed;
    QVector<QVector<double>> sensor3RollCycle;
    QVector<QVector<double>> sensor3RemoveBias;
    QVector<QVector<double>> sensor3_Original_Bias;

    QLineEdit *angleEdit1 ;
    QLineEdit *angleEdit2 ;
    QLineEdit *angleEdit3 ;

    double sensor1Angle;
    double sensor2Angle;
    double sensor3Angle;


    QVector<double> sensor1VibMax;
    QVector<double> sensor1VibMin;
    QVector<double> sensor2VibMax;
    QVector<double> sensor2VibMin;
    QVector<double> sensor3VibMax;
    QVector<double> sensor3VibMin;

    QComboBox *bladeComboBox;
    QComboBox *sensorComboBox;

    QRadioButton *singleBladeRadioButton;
    QRadioButton *allBladesRadioButton;


signals:
    void fileReadError(const QString &errorMessage);
    void fileReadCompleted(const QString &message);
    void DataNotEnough(const QString &message);
    void CalAngle(const QString &message);
    void vibCalCompleted(const QString &message);


//-------LM--------窗口的参数
public:
    void initializeData();//导入数据按钮的槽函数
    void SetVibRang();//设置振动区间
    void LMPlotClear();
    void ReSetVibPlot();
    void on_LMfitButton_clicked();
    void clearTableButton();

private:
    QCustomPlot *a;//绘制去恒偏的振动位移
    QComboBox *b;//传感器的编号
    QComboBox *c;//叶片的编号
    QLineEdit *d;//起始圈数
    QLineEdit *e;//终止圈数
    QTableWidget *tableWidget;//表格

    QVector<QVector<QVector<double>>> Fre_Xdata;
    QVector<QVector<QVector<double>>> CycleNum_Xdata;
    QVector<QVector<QVector<double>>> NoBiasVib_Ydata;

    SensorFitting LMfit;
    void performLMfit(int downlimit, int uplimit,
                      QVector<double> speed, QVector<double> bias,
                      int Q_value);

//-------------数据降噪的窗口--------------------------------------
private:
   void exportBinFileThread(const QString& riseFilePath, const QString& fallFilePath,
                      const QString& outputRiseFileName, const QString& outputFallFileName);
   void exportBinFileThread();
   QString generateOutputFileName(const QString& inputFilePath, const QString& suffix);

   std::vector<QTextEdit*> risePathTexts;
   std::vector<QTextEdit*> fallPathTexts;
   std::vector<QPushButton*> chooseRiseButtons;
   std::vector<QPushButton*> chooseFallButtons;
   std::vector<QPushButton*> exportBinButtons;
   std::vector<QString> riseFilePaths;
   std::vector<QString> fallFilePaths;

   QTextEdit *outputPathText;
   QString outputDirectory;

private slots:
   void chooseRiseFile(int channel);
   void chooseFallFile(int channel);
   void chooseOutputDirectory();
   void exportBinFile(int channel);
   void exportToExcel();
};

#endif // PARAMETER_IDENTIFICATION_H
