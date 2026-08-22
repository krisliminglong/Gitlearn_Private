#include "semmanager.h"

SemManager *SemManager::instance=0;//这个0实际上是指nullptr
SemManager *SemManager::getInstance()
{
    if(instance==0)
    {
        instance=new SemManager;
    }
    return instance;
}

SemManager::SemManager()
    :semA(1),semB(0)
{

}
