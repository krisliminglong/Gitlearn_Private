QT       += core gui printsupport

include(./qtxlsx/src/xlsx/qtxlsx.pri)

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# INCLUDEPATH +=E:\Qt\ITEMS\DigitalDAQ32\eigen-3.4.0  #矩阵的计算库，绝对路径
INCLUDEPATH += $$PWD/eigen-3.4.0   # 使用相对路径
# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

#软件的图标
RC_ICONS=DAQ.ico

SOURCES += \
    calculation.cpp \
    common/ostools/spcm_ostools_win.cpp \
    common/spcm_lib_card.cpp \
    common/spcm_lib_data.cpp \
    common/spcm_lib_thread.cpp \
    datathread.cpp \
    lmfit.cpp \
    main.cpp \
    mainwindow.cpp \
    monitoringwindow.cpp \
    mydial.cpp \
    parameter_identification.cpp \
    qcustomplot.cpp \
    ringbuffer.cpp \
    savefile.cpp \
    wavedisplay.cpp \
    workfunction.cpp

HEADERS += \
    c_header/dlltyp.h \
    c_header/errors.h \
    c_header/regs.h \
    c_header/spcerr.h \
    c_header/spcm_drv.h \
    c_header/spectrum.h \
    calculation.h \
    common/ostools/spcm_md5.h \
    common/ostools/spcm_network_winLin.h \
    common/ostools/spcm_ostools.h \
    common/ostools/spcm_oswrap.h \
    common/spcm_lib_card.h \
    common/spcm_lib_data.h \
    common/spcm_lib_thread.h \
    datathread.h \
    dll_loading/spcm_drv_def.h \
    lmfit.h \
    mainwindow.h \
    monitoringwindow.h \
    mydial.h \
    parameter_identification.h \
    qcustomplot.h \
    ringbuffer.h \
    savefile.h \
    sb5_file/sb5_file.h \
    ui_uipage2.h \
    wavedisplay.h \
    workfunction.h

FORMS += \
    mainwindow.ui \
    monitoringwindow.ui \
    parameter_identification.ui \
    wavedisplay.ui

TRANSLATIONS += \
    DigitalDAQ32_zh_CN.ts

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target


#库的导入
win32: LIBS += -L$$PWD/c_header/ -lspcm_win32_msvcpp
INCLUDEPATH += $$PWD/c_header
DEPENDPATH += $$PWD/c_header
#win32:!win32-g++: PRE_TARGETDEPS += $$PWD/c_header/spcm_win32_msvcpp.lib
#else:win32-g++: PRE_TARGETDEPS += $$PWD/c_header/libspcm_win32_msvcpp.a

win32: LIBS += -L$$PWD/c_header/ -lspectrum
INCLUDEPATH += $$PWD/c_header
DEPENDPATH += $$PWD/c_header
#win32:!win32-g++: PRE_TARGETDEPS += $$PWD/c_header/spectrum.lib
#else:win32-g++: PRE_TARGETDEPS += $$PWD/c_header/libspectrum.a

win32: LIBS += -L$$PWD/c_header/ -lspectrum_comp
INCLUDEPATH += $$PWD/c_header
DEPENDPATH += $$PWD/c_header
#win32:!win32-g++: PRE_TARGETDEPS += $$PWD/c_header/spectrum_comp.lib
#else:win32-g++: PRE_TARGETDEPS += $$PWD/c_header/libspectrum_comp.a

