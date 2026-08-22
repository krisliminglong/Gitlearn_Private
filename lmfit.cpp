#include "lmfit.h"
#include <QMessageBox>
#include <cmath>
/*LM拟合曲线的方法介绍：如果后面的师弟师妹看到本段代码，首先需要有一个概念，需要拟合的参数有A0振动幅值、共振中心频率、品质因素（与阻尼有关）
、初始相位、恒偏量这五个未知量。拟合的过程时：有一组x值（代表转速频率），和对应的y值（代表振动）。y和x是有符合振动方程的规律的。所以我们以需要拟合的参数
 P[A0,fn,Q,phase,Dx]为基准，去求解|y-f（x，P）|^2差值到达一定小后，就可以认为此时的P就是我们要求解的未知量。所以过程显而易见：第一步：先初始化待拟合的参数P
 第二步：计算误差；第三步：调整参数更新使得误差到达一定的精度，得到未知参数。参数更新是使用雅可比矩阵J和控制参数λ完成的，
 具体是公式是P（new）=P（old）-[（J^T*J+λI）^-1]*[J^T]*r;    J^T是J的转置矩阵、I是单位矩阵、[（J^T*J+λI）^-1]里面的-1表示矩阵的逆，r是每次计算的误差值*/
SensorFitting::SensorFitting()
{
    qDebug()<<"已经创建类";
}

std::tuple<Eigen::MatrixXd, QVector<double>, QVector<double>,
           QVector<double>> SensorFitting::fitProbe(int downlimit, int uplimit,
                                                    const QVector<double>& sensorSpeed,
                                                    const QVector<double>& sensorBias,
                                                    int Q_value)
{
    Eigen::MatrixXd est_params;//拟合后参数
    QVector<double> x, y, y_fit;//原始数据，拟合数据
    try
    {
        qDebug() << "开始读取数据";
        Eigen::MatrixXd rotatingspeed(uplimit - downlimit, 1);
        Eigen::MatrixXd displacement(uplimit - downlimit, 1);
        Eigen::MatrixXd fn(1, 1);

        prepareData(downlimit, uplimit, sensorSpeed, sensorBias, rotatingspeed, displacement, fn);

        qDebug() << "读取数据完成";

        Eigen::MatrixXd params = Eigen::MatrixXd::Zero(5, 1);
        params(0, 0) = 0; // 初始化位移A0
        params(1, 0) = fn(0); // 初始化中心频率为振幅最大值
        params(2, 0) = Q_value; // 品质因数
        params(3, 0) = 0; // 初相位
        params(4, 0) = 0; // 恒偏量

        int num_iter = 100;
        qDebug() << "开始拟合";
        est_params = LM(num_iter, params, rotatingspeed, displacement);
        qDebug() << "拟合完成";

        plotFitting(rotatingspeed, displacement, est_params, x, y, y_fit);
        qDebug() << "绘图数据读取完成";
    }
    catch (std::exception& e)
    {
        QMessageBox::critical(nullptr, "Error", e.what());
    }

    return std::make_tuple(est_params, x, y, y_fit);
}


void SensorFitting::prepareData(int downlimit, int uplimit,
                                const QVector<double>& sensorSpeed,
                                const QVector<double>& sensorBias,
                                Eigen::MatrixXd& rotatingspeed,
                                Eigen::MatrixXd& displacement,
                                Eigen::MatrixXd& fn)
{
    QVector<double> y1(uplimit - downlimit);
    for (int i = downlimit; i < uplimit; ++i)
    {
        rotatingspeed(i - downlimit) = sensorSpeed[i] / 60.0;  // 将转速转换为频率
        displacement(i - downlimit) = sensorBias[i];
        y1[i - downlimit] = displacement(i - downlimit);
    }
    // 找到最大值和最小值对应的索引
    int max_index1 = std::distance(y1.begin(), std::max_element(y1.begin(), y1.end()));
    int min_index1 = std::distance(y1.begin(), std::min_element(y1.begin(), y1.end()));

    // 根据最大值和最小值确定振幅最大时 fn
    if (std::abs(y1[max_index1]) > std::abs(y1[min_index1]))
    {
        fn(0, 0) = rotatingspeed(max_index1);
    }
    else
    {
        fn(0, 0) = rotatingspeed(min_index1);
    }
}


void SensorFitting::plotFitting(const Eigen::MatrixXd& data_input,
                                const Eigen::MatrixXd& data_output,
                                const Eigen::MatrixXd& params,
                                QVector<double>& x, QVector<double>& y,
                                QVector<double>& y_fit)
{
    x.resize(data_input.rows());
    y.resize(data_output.rows());
    y_fit.resize(data_input.rows());

    for (int i = 0; i < data_input.rows(); ++i)
    {
        x[i] = data_input(i, 0);
        y[i] = data_output(i, 0);
        y_fit[i] = myFunc(params, data_input(i, 0));
    }
}



double SensorFitting::myFunc(const Eigen::MatrixXd& params, double input_data)
{
    double a = params(0, 0);//A0
    double b = params(1, 0);//Fn
    double c = params(2, 0);//Q
    double d = params(3, 0);//相位角
    double e = params(4, 0);//恒偏量
    double eta = c * (1 - pow(input_data / b, 2)) / (input_data / b);//η’
    return a * c * (eta * cos(d) + sin(d)) / (input_data / b * (1 + eta * eta)) + e;
}

double SensorFitting::calDeriv(const Eigen::MatrixXd& params, double input_data, int param_index)
{
    //先传入整个参数、对应的索引，然后对这个参数前后微小的移动，返回值是利用导数的定义计算近相似的导数
    Eigen::MatrixXd params1 = params;
    Eigen::MatrixXd params2 = params;
    params1(param_index, 0) += 0.000001;
    params2(param_index, 0) -= 0.000001;
    double data_est_output1 = myFunc(params1, input_data);
    double data_est_output2 = myFunc(params2, input_data);
    return (data_est_output1 - data_est_output2) / 0.000002;//返回对应参数的导数（偏导）
}

Eigen::MatrixXd SensorFitting::calJacobian(const Eigen::MatrixXd& params,
                                           const Eigen::MatrixXd& input_data)
{
    int num_params = params.rows();//通过行索引获取参数的数量
    int num_data = input_data.rows();
    Eigen::MatrixXd J = Eigen::MatrixXd::Zero(num_data, num_params);
    for (int i = 0; i < num_params; ++i)
    {
        for (int j = 0; j < num_data; ++j)
        {
            J(j, i) = calDeriv(params, input_data(j, 0), i);//对应参数下的每个数据下的偏导数
        }
    }
    return J;
}

//计算误差
Eigen::MatrixXd SensorFitting::calResidual(const Eigen::MatrixXd& params,
                                           const Eigen::MatrixXd& input_data,
                                           const Eigen::MatrixXd& output_data)
{
    //unaryExpr是Eigen库中的对向量元素逐个操作的函数，参数为一个函数或者lamda表达式，[&](double x)表示捕获外部的变量，这里是指
    //input_data一个一个的元素，然后把这个数给myFunc，逐个计算在该参数下拟合值是多少。并存在data_est_output中
    Eigen::MatrixXd data_est_output = input_data.unaryExpr([&](double x) { return myFunc(params, x); });
    return output_data - data_est_output;//返回误差值
}

double SensorFitting::getInitU(const Eigen::MatrixXd& A, double tao)
{
    //A.diagonal().maxCoeff()是在返回A中的最大值
    return tao * A.diagonal().maxCoeff();
}

Eigen::MatrixXd SensorFitting::LM(int num_iter, Eigen::MatrixXd params,
                                  const Eigen::MatrixXd& input_data,
                                  const Eigen::MatrixXd& output_data)
{
//    qDebug()<<"进入LM拟合";
    int num_params = params.rows();
    int k = 0;
    //先计算一次误差值
    Eigen::MatrixXd residual = calResidual(params, input_data, output_data);
    //计算雅可比矩阵
    Eigen::MatrixXd Jacobian = calJacobian(params, input_data);
    Eigen::MatrixXd A = Jacobian.transpose() * Jacobian;
    //计算梯度向量
    Eigen::MatrixXd g = Jacobian.transpose() * residual;

    double u = getInitU(A, 1e-3);//动态变化的控制参数

    double v = 2;

    // 判断是否满足停止条件（梯度的无穷范数是否小于等于阈值），寻找梯度g中的最大值
    //如果满足这个条件，则说明梯度的最大值已经非常接近于零，表示当前的参数几乎已经收敛到局部最小值附近，因此可以停止迭代。
    bool stop = (g.lpNorm<Eigen::Infinity>() <= 1e-15);

    while (!stop && k < num_iter)
    {
        k++;
        bool found_step = false;
        while (!found_step)
        {
            //创建了一个尺寸为 num_params x num_params 的单位矩阵xu以后得到与A相似的Hessian_LM矩阵。
            //u表示一个阻尼因子，如果 u 较大，算法更像梯度下降法；如果 u 较小，算法更像高斯-牛顿法。
            Eigen::MatrixXd Hessian_LM = A + u * Eigen::MatrixXd::Identity(num_params, num_params);

            //步长表示在当前参数基础上需要调整的方向和距离，以使得目标函数值最小化。
            Eigen::MatrixXd step = Hessian_LM.inverse() * g;

            //步长小表示当前参数值已经非常接近最优解，进一步调整参数的效果非常有限，所以到一定程度就可以退出
            if (step.norm() <= 1e-15)
            {
                stop = true;//外层循环终止
                break;//跳出内层循环
            }
            else
            {
                //在当前参数基础上进行了一次更新，减少步长后尝试朝着减少误差的方向移动
                Eigen::MatrixXd new_params = params + step;
                //新的误差矩阵
                Eigen::MatrixXd new_residual = calResidual(new_params, input_data, output_data);
                //ρ 值（rou）用于评估当前步长的有效性。具体来说，它比较了当前残差的平方和新的残差的平方之间的差异，除以步长和梯度的乘积。
                double rou = (residual.squaredNorm() - new_residual.squaredNorm()) / ((step.transpose() * (u * step + g))(0, 0));
                //如果 ρ 值大于零，表示新的参数减少了误差，可以接受；否则，需要调整步长和阻尼因子继续尝试。
                if (rou > 0)
                {
                    params = new_params;//接收有效的修正
                    residual = new_residual;//接受最新一次调整的误差
                    Jacobian = calJacobian(params, input_data);//计算新的雅可比
                    A = Jacobian.transpose() * Jacobian;//更新A矩阵
                    g = Jacobian.transpose() * residual;//更新梯度矩阵
                    //stop达到和误差到达标准时，就可以让跳出循环了
                    stop = (g.lpNorm<Eigen::Infinity>() <= 1e-15) || (residual.squaredNorm() <= 1e-15);
                    //根据 rou 的值调整阻尼因子 u 和参数 v。
                    //如果 rou 大于零，则减小 u 的值，使得算法更接近高斯-牛顿法；同时将 v 重置为2。
                    u = u * std::max(1.0 / 3, 1 - pow(2 * rou - 1, 3));
                    v = 2;
                    found_step = true;//退出内层循环，在大循环中继续逼近
                }
                else
                {
                    //如果 rou 小于或等于零，表示步长无效，则增大阻尼因子 u 并调整参数 v，继续在内层循环中尝试不同的步长。
                    u = u * v;
                    v = 2 * v;
                }
            }
        }
    }
    return params;//返回已经拟合好的参数
}



