//
//    PolarimetryPage.h: description
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
#ifndef PolarimetryPage_H
#define PolarimetryPage_H

#include <TabWidgetFactory.h>
#include <QShowEvent>
#include <list>

#include "CoherentDetector.h"

#define POLARIMETER_STOKES_UPDATE_TAU 1

namespace Ui {
  class PolarimetryPage;
}

class Waveform;

namespace SigDigger {
  class Polarimeter;

  class PolarimetryPageConfig : public Suscan::Serializable {
  public:
    float integratiomTime = .1f;
    float relativeGain = 1;
    float relativePhase = 0;
    bool swapVH     = false;
    bool flipVH     = false;
    bool autoScroll = true;
    bool autoFit    = true;

    // Overriden methods
    void deserialize(Suscan::Object const &conf) override;
    Suscan::Object &&serialize() override;
  };

  class PolarimetryPage : public TabWidget
  {
    Q_OBJECT

    Polarimeter *m_owner = nullptr;
    PolarimetryPageConfig *m_config = nullptr;

    SUFREQ       m_frequency;
    bool         m_paramsSet = false;

    SUFLOAT      m_I = 0, m_Q = 0, m_U = 0, m_V = 0;
    SUFLOAT      m_sampRate = -1;
    SUFLOAT      m_accumPwr;
    SUCOMPLEX    m_Ex = 0;
    SUCOMPLEX    m_Ey = 0;
    SUSCOUNT     m_accumCount = 0;
    SUSCOUNT     m_intSamples = 0;
    SUFLOAT      m_max = 0;
    SUFLOAT      m_gain = 0;
    SUFLOAT      m_alpha;

    SUCOMPLEX    m_vFactor = 1.;

    std::vector<SUCOMPLEX> m_iq;
    std::vector<SUCOMPLEX> m_uv;
    std::vector<SUCOMPLEX> m_vSamp;

    struct timeval m_lastTimeStamp;

    const SUCOMPLEX *adjustVSamp(const SUCOMPLEX *v, size_t size);
    void calcIntegrationTime();
    void refreshUi();
    void showEvent(QShowEvent *) override;
    void connectAll();
    void updateGain();
    void updateStokes();
    void zoomWaveforms();
    void fitVertical(Waveform *wf = nullptr);
    void fitHorizontal(Waveform *wf = nullptr);
    void setAutoScroll(bool, Waveform *wf = nullptr);
    void refreshData(Waveform *wf = nullptr);
    void updateAll();

    void applyAntennaConfig();
    void applyPlotConfig();

    void feedMeasurements(
        const SUCOMPLEX *hSamp,
        const SUCOMPLEX *vSamp,
        SUSCOUNT size);

  public:
    explicit PolarimetryPage(
        TabWidgetFactory *,
        UIMediator *,
        QWidget *parent = nullptr);

    ~PolarimetryPage() override;

    void feed(
        struct timeval const &tv,
        const SUCOMPLEX *hSamp,
        const SUCOMPLEX *vSamp,
        SUSCOUNT size);

    void setProperties(
        Polarimeter *,
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
    Ui::PolarimetryPage *ui;

  signals:
    void closeReq();
    void frequencyChanged(qreal);
    void bandwidthChanged(qreal);

  public slots:
    void onClear();
    void onSave();
    void onVFit();
    void onHFit();
    void onAutoScroll();
    void onAutoFit();
    void onAntennaChanged();
  };

}

#endif // PolarimetryPage_H
