//
//    PhaseComparator.h: description
//    Copyright (C) 2023 Gonzalo José Carracedo Carballal
//
//    This program is free software: you can redistribute it and/or modify
//    it under the terms of the GNU Lesser General Public License as
//    published by the Free Software Foundation, either version 3 of the
//    License, or (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful, but
//    WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU Lesser General Public License for more details.
//
//    You should have received a copy of the GNU Lesser General Public
//    License along with this program.  If not, see
//    <http://www.gnu.org/licenses/>
//
#ifndef PHASECOMPARATOR_H
#define PHASECOMPARATOR_H

#include "PhaseComparatorFactory.h"
#include <QWidget>
#include <WFHelpers.h>
#include <QWidget>
#include <QFile>

namespace Ui {
  class PhaseComparator;
}

namespace SigDigger {
  class SimplePhaseComparator;
  class MainSpectrum;
  class GlobalProperty;
  class DetachableProcess;
  class PhasePlotPage;

  class PhaseComparatorConfig : public Suscan::Serializable {
  public:
    bool collapsed = false;

    // Overriden methods
    void deserialize(Suscan::Object const &conf) override;
    Suscan::Object &&serialize() override;
  };


  class PhaseComparator : public ToolWidget
  {
    Q_OBJECT

    Suscan::Analyzer       *m_analyzer    = nullptr;
    PhaseComparatorConfig  *m_panelConfig = nullptr;
    SimplePhaseComparator  *m_comparator  = nullptr;
    MainSpectrum           *m_spectrum    = nullptr;
    SUSCOUNT                m_count       = 0;

    // Named channels
    NamedChannelSetIterator m_namChanLo;
    NamedChannelSetIterator m_namChanHi;
    bool                    m_loOpened = false;
    bool                    m_hiOpened = false;
    bool m_haveNamChan = false;

    // Other UI state properties
    bool    m_haveFirstReading = false;

    // Current plot
    PhasePlotPage *m_plotPage = nullptr;

    void openPlot();
    void applySpectrumState();
    void connectAll();
    void refreshUi();
    void refreshNamedChannel();
    QColor channelColor(bool state) const;

  public:
    explicit PhaseComparator(PhaseComparatorFactory *, UIMediator *, QWidget *parent = nullptr);
    ~PhaseComparator() override;

    // Configuration methods
    Suscan::Serializable *allocConfig() override;
    void applyConfig() override;
    bool event(QEvent *) override;

    // Overriden methods
    void setState(int, Suscan::Analyzer *) override;
    void setQth(Suscan::Location const &) override;
    void setColorConfig(ColorConfig const &) override;
    void setTimeStamp(struct timeval const &) override;
    void setProfile(Suscan::Source::Config &) override;

  public slots:
    void onOpenChannel();
    void onCloseChannel();
    void onAdjustFrequency();
    void onAdjustBandwidth();

    void onComparatorOpened();
    void onComparatorClosed();
    void onComparatorError(QString);
    void onComparatorData();
    void onComparatorStateChanged(int, int, QString);

    void onSpectrumFrequencyChanged(qint64);
    void onClosePlotPage();
  private:
    Ui::PhaseComparator *ui;
  };

}


#endif // PHASECOMPARATOR_H
