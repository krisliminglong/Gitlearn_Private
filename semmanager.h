#ifndef SEMMANAGER_H
#define SEMMANAGER_H

#include<QSemaphore>
class SemManager
{
public:
    QSemaphore semA;
    QSemaphore semB;
    static SemManager* getInstance();//由于是Static类的，所以在外部获取时，都会获取同一个实列
private:
    static SemManager* instance;
    SemManager();//构造函数被私有了，不能在外部空间定义实列
};

#endif // SEMMANAGER_H
