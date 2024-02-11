QT += core widgets gui

TEMPLATE = lib
DEFINES += ANTSDRPLUGIN_LIBRARY

CONFIG += c++11
CONFIG += unversioned_libname unversioned_soname

isEmpty(PLUGIN_DIRECTORY) {
  _HOME = $$(HOME)
  isEmpty(_HOME) {
    error(Cannot deduce user home directory. Please provide a valid plugin installation path through the PLUGIN_DIRECTORY property)
  }

  PLUGIN_DIRECTORY=$$_HOME/.suscan/plugins
}

isEmpty(SUWIDGETS_PREFIX) {
  SUWIDGETS_INSTALL_HEADERS=$$[QT_INSTALL_HEADERS]/SuWidgets
} else {
  SUWIDGETS_INSTALL_HEADERS=$$SUWIDGETS_PREFIX/include/SuWidgets
}

isEmpty(SIGDIGGER_PREFIX) {
  SIGDIGGER_INSTALL_HEADERS=$$[QT_INSTALL_HEADERS]/SigDigger
} else {
  SIGDIGGER_INSTALL_HEADERS=$$SIGDIGGER_PREFIX/include/SigDigger
}

# Default rules for deployment.
target.path = $$PLUGIN_DIRECTORY
!isEmpty(target.path): INSTALLS += target

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += Registration.cpp \
  AD9361SourcePage.cpp \
  AD9361SourcePageFactory.cpp \
  2rx_ad9361.c \
  CoherentChannelForwarder.cpp \
  CoherentDetector.cpp \
  PhaseComparator.cpp \
  PhaseComparatorFactory.cpp \
  PhasePlotPage.cpp \
  PhasePlotPageFactory.cpp \
  Polarimeter.cpp \
  PolarimeterFactory.cpp \
  PolarimetryPage.cpp \
  PolarimetryPageFactory.cpp \
  RawChannelForwarder.cpp \
  SimplePhaseComparator.cpp

HEADERS += 2rx_ad9361.h \
  AD9361SourcePage.h \
  AD9361SourcePageFactory.h \
  CoherentChannelForwarder.h \
  CoherentDetector.h \
  PhaseComparator.h \
  PhaseComparatorFactory.h \
  PhasePlotPage.h \
  PhasePlotPageFactory.h \
  Polarimeter.h \
  PolarimeterFactory.h \
  PolarimetryPage.h \
  PolarimetryPageFactory.h \
  RawChannelForwarder.h \
  SimplePhaseComparator.h

INCLUDEPATH += $$SUWIDGETS_INSTALL_HEADERS $$SIGDIGGER_INSTALL_HEADERS

unix: CONFIG += link_pkgconfig
unix: PKGCONFIG += suscan sigutils fftw3 sndfile libiio libad9361

CONFIG += c++11

FORMS += \
  AD9361SourcePage.ui \
  PhaseComparator.ui \
  PhasePlotPage.ui \
  Polarimeter.ui \
  PolarimetryPage.ui

