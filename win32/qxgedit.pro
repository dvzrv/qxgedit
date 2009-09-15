INCPATH += ../src

HEADERS += ../src/XGParam.h \
           ../src/XGParamObserver.h \
           ../src/XGParamWidget.h \
           ../src/XGParamSysex.h \
           ../src/qxgeditXGMasterMap.h \
           ../src/qxgeditAbout.h \
           ../src/qxgeditAmpEg.h \
           ../src/qxgeditCheck.h \
           ../src/qxgeditCombo.h \
           ../src/qxgeditDial.h \
           ../src/qxgeditDrop.h \
           ../src/qxgeditDrumEg.h \
           ../src/qxgeditEdit.h \
           ../src/qxgeditFilter.h \
           ../src/qxgeditKnob.h \
           ../src/qxgeditPartEg.h \
           ../src/qxgeditPitch.h \
           ../src/qxgeditScale.h \
           ../src/qxgeditSpin.h \
           ../src/qxgeditUserEg.h \
           ../src/qxgeditVibra.h \
           ../src/qxgeditMidiDevice.h \
           ../src/qxgeditOptions.h \
           ../src/qxgeditOptionsForm.h \
           ../src/qxgeditMainForm.h

SOURCES += ../src/XGParam.cpp \
           ../src/XGParamObserver.cpp \
           ../src/XGParamWidget.cpp \
           ../src/XGParamSysex.cpp \
           ../src/qxgeditXGMasterMap.cpp \
           ../src/qxgeditAmpEg.cpp \
           ../src/qxgeditCheck.cpp \
           ../src/qxgeditCombo.cpp \
           ../src/qxgeditDial.cpp \
           ../src/qxgeditDrop.cpp \
           ../src/qxgeditDrumEg.cpp \
           ../src/qxgeditEdit.cpp \
           ../src/qxgeditFilter.cpp \
           ../src/qxgeditKnob.cpp \
           ../src/qxgeditPartEg.cpp \
           ../src/qxgeditPitch.cpp \
           ../src/qxgeditScale.cpp \
           ../src/qxgeditSpin.cpp \
           ../src/qxgeditUserEg.cpp \
           ../src/qxgeditVibra.cpp \
           ../src/qxgeditMidiDevice.cpp \
           ../src/qxgeditOptions.cpp \
           ../src/qxgeditOptionsForm.cpp \
           ../src/qxgeditMainForm.cpp \
           ../src/main.cpp

FORMS    = ../src/qxgeditOptionsForm.ui \
           ../src/qxgeditMainForm.ui

RESOURCES += ../icons/qxgedit.qrc

TEMPLATE = app
CONFIG  += qt thread warn_on debug
LANGUAGE = C++

win32 {
	CONFIG  += console
	INCPATH += C:\usr\local\include
	LIBS    += -LC:\usr\local\lib
}

LIBS += -lasound
