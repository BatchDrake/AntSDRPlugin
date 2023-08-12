//
//    PhasePlotPage.h: description
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
#ifndef PHASEPLOTPAGE_H
#define PHASEPLOTPAGE_H

#include <TabWidgetFactory.h>
#include <QShowEvent>

namespace Ui {
  class PhasePlotPage;
}

namespace SigDigger {
  class PhaseComparator;
  class CoherentDetector;

  class PhasePlotPageConfig : public Suscan::Serializable {
  public:
    bool   autoFit            = true;
    bool   autoScroll         = true;
    bool   doPlot             = true;
    float  gainDb             = 0;
    float  phaseOrigin        = 0;
    bool   logEvents          = false;
    float  measurementTime    = .2;
    float  coherenceThreshold = 10.;
    double maxAlloc           = 256 * (1 << 20);
    bool   angleOfArrival     = false;
    bool   autoSave           = false;
    std::string saveDir       = "";

    // Overriden methods
    void deserialize(Suscan::Object const &conf) override;
    Suscan::Object &&serialize() override;
  };

  class PhasePlotPage : public TabWidget
  {
    Q_OBJECT

    bool m_paramsSet              = false;
    PhaseComparator *m_owner      = nullptr;
    CoherentDetector *m_detector  = nullptr;
    PhasePlotPageConfig *m_config = nullptr;

    std::vector<SUCOMPLEX> m_data;
    std::vector<SUCOMPLEX> m_empty;

    SUFLOAT   m_sampRate;
    SUCOMPLEX m_accumulated;
    SUSCOUNT  m_accumCount = 0;
    SUFLOAT   m_max = 0;
    SUFLOAT   m_gain = 1;
    SUCOMPLEX m_phaseAdjust = 1;

    FILE     *m_autoSaveFp = nullptr;
    SUSCOUNT  m_savedSize  = 0;

    struct timeval m_lastTimeStamp;
    struct timeval m_lastEvent;
    struct timeval m_firstSamples;

    bool      m_haveFirstSamples = false;
    bool      m_infoLogged = false;
    bool      m_haveEvent = false;
    bool      m_haveSelection = false;

    void refreshMeasurements();
    void logDetectorInfo();
    void clearData();
    void refreshUi();
    void plotSelectionPhase(qint64, qint64);
    void connectAll();
    void logText(QString const &);
    void logText(struct timeval const &, QString const &);

    void showEvent(QShowEvent *) override;

  public:
    explicit PhasePlotPage(
        TabWidgetFactory *,
        UIMediator *,
        QWidget *parent = nullptr);

    ~PhasePlotPage() override;

    void feed(struct timeval const &tv, const SUCOMPLEX *, SUSCOUNT);
    void setFreqencyLimits(SUFREQ min, SUFREQ max);

    QString genAutoSaveFileName() const;

    void abortAutoSaveFile(int error);
    void cycleAutoSaveFile();

    void setProperties(
        PhaseComparator *,
        SUFLOAT sampRate,
        SUFREQ  frequency,
        SUFLOAT bandwidth);

    virtual std::string getLabel() const override;
    virtual void closeRequested() override;
    virtual void setColorConfig(ColorConfig const &) override;

    virtual Suscan::Serializable *allocConfig(void) override;
    virtual void applyConfig(void) override;
    virtual void setTimeStamp(struct timeval const &) override;

  private:
    Ui::PhasePlotPage *ui;

  signals:
    void closeReq();
    void frequencyChanged(qreal);
    void bandwidthChanged(qreal);

  public slots:
    void onSavePlot();
    void onAutoScrollToggled();
    void onEnablePlotToggled();
    void onClear();
    void onMaxAllocChanged();
    void onAutoFitToggled();
    void onAoAToggled();
    void onGainChanged();
    void onChangeFrequency();
    void onChangeBandwidth();
    void onChangePhaseOrigin();
    void onChangeMeasurementTime();
    void onChangeCoherenceThreshold();
    void onLogEnableToggled();
    void onSaveLog();
    void onClearLog();

    void onToggleAutoSave();
    void onBrowseSaveDir();
    void onHSelection(qreal, qreal);
  };

}

#endif // PHASEPLOTPAGE_H
