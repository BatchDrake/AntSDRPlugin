//
//    PolarimetryPage.cpp: description
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
#include "PolarimetryPage.h"
#include "ui_PolarimetryPage.h"
#include <ColorConfig.h>
#include "CoherentDetector.h"
#include <QDateTime>
#include <SigDiggerHelpers.h>
#include <QFileDialog>
#include <QMessageBox>
#include <QTextStream>
#include <QDir>
#include <QFile>
#include <QLabel>

using namespace SigDigger;

// Yay Qt6
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#  define DirectoryOnly Directory
#endif

#define GAIN_SCALING .707

#define LOG_OF_1DB 0.11512925464970229

#define STRINGFY(x) #x
#define STORE(field) obj.set(STRINGFY(field), this->field)
#define LOAD(field) this->field = conf.get(STRINGFY(field), this->field)

void
PolarimetryPageConfig::deserialize(Suscan::Object const &conf)
{
  LOAD(integratiomTime);
  LOAD(relativeGain);
  LOAD(relativePhase);
  LOAD(swapVH);
  LOAD(flipVH);
  LOAD(autoScroll);
  LOAD(autoFit);
}

Suscan::Object &&
PolarimetryPageConfig::serialize()
{
  Suscan::Object obj(SUSCAN_OBJECT_TYPE_OBJECT);

  obj.setClass("PolarimetryPageConfig");

  STORE(integratiomTime);
  STORE(relativeGain);
  STORE(relativePhase);
  STORE(swapVH);
  STORE(flipVH);
  STORE(autoScroll);
  STORE(autoFit);

  return persist(obj);
}


PolarimetryPage::PolarimetryPage(
    TabWidgetFactory *factory,
    UIMediator *mediator,
    QWidget *parent) :
  TabWidget(factory, mediator, parent),
  ui(new Ui::PolarimetryPage)
{
  ui->setupUi(this);

  ui->polarizationView->setGain(1e-1);

  Waveform *wfs[4] = {
    ui->IWaveform, ui->QWaveform, ui->UWaveform, ui->VWaveform
  };

  for (auto i = 0; i < 4; ++i) {
    wfs[i]->setRealComponent((i & 1) == 0);
    wfs[i]->setAutoFitToEnvelope(false);
  }

  ui->QWaveform->reuseDisplayData(ui->IWaveform);
  ui->VWaveform->reuseDisplayData(ui->UWaveform);

  ui->IWaveform->setData(&m_iq);
  ui->QWaveform->setData(&m_iq);
  ui->UWaveform->setData(&m_uv);
  ui->VWaveform->setData(&m_uv);

  connectAll();
}

void
PolarimetryPage::connectAll()
{
  connect(
        ui->clearButton,
        SIGNAL(clicked(bool)),
        this,
        SLOT(onClear()));

  connect(
        ui->saveButton,
        SIGNAL(clicked(bool)),
        this,
        SLOT(onSave()));

  connect(
        ui->fitHButton,
        SIGNAL(clicked(bool)),
        this,
        SLOT(onHFit()));

  connect(
        ui->fitVButton,
        SIGNAL(clicked(bool)),
        this,
        SLOT(onVFit()));

  connect(
        ui->autoFitButton,
        SIGNAL(clicked(bool)),
        this,
        SLOT(onAutoFit()));

  connect(
        ui->autoScrollButton,
        SIGNAL(clicked(bool)),
        this,
        SLOT(onAutoScroll()));

  connect(
        ui->swapVHCheck,
        SIGNAL(clicked(bool)),
        this,
        SLOT(onAntennaChanged()));

  connect(
        ui->mirrorVHCheck,
        SIGNAL(clicked(bool)),
        this,
        SLOT(onAntennaChanged()));

  connect(
        ui->vhGainSPin,
        SIGNAL(valueChanged(qreal)),
        this,
        SLOT(onAntennaChanged()));

  connect(
        ui->vhPhaseSpin,
        SIGNAL(valueChanged(qreal)),
        this,
        SLOT(onAntennaChanged()));
}

static void
setElidedLabelText(QLabel *label, QString text)
{
    QFontMetrics metrix(label->font());
    int width = label->width() - 2;
    QString clippedText = metrix.elidedText(text, Qt::ElideMiddle, width);
    label->setText(clippedText);
}

void
PolarimetryPage::updateGain()
{
  if (m_accumCount > 2) {
    SUFLOAT mag, gain = 1;
    SUFLOAT accum = m_accumPwr / m_accumCount;
    mag = SU_SQRT(accum);

    SU_SPLPF_FEED(m_max, mag, .5);
    if (m_max > std::numeric_limits<SUFLOAT>::epsilon()) {
      gain = GAIN_SCALING / m_max;

      ui->polarizationView->setGain(gain);
    }
  }
}

void
PolarimetryPage::updateStokes()
{
  if (m_accumCount > 0) {
    SUFLOAT I, Q, U, V;
    SUCOMPLEX Ex = m_Ex / SU_SQRT(m_accumCount);
    SUCOMPLEX Ey = m_Ey / SU_SQRT(m_accumCount);

    I = SU_C_REAL(Ex * SU_C_CONJ(Ex)) + SU_C_REAL(Ey * SU_C_CONJ(Ey));
    Q = SU_C_REAL(Ex * SU_C_CONJ(Ex)) - SU_C_REAL(Ey * SU_C_CONJ(Ey));

    U = 2 * SU_C_REAL(Ex * SU_C_CONJ(Ey));
    V = 2 * SU_C_IMAG(Ex * SU_C_CONJ(Ey));

    SU_SPLPF_FEED(m_I, I, m_alpha);
    SU_SPLPF_FEED(m_Q, Q / m_I, m_alpha);
    SU_SPLPF_FEED(m_U, U / m_I, m_alpha);
    SU_SPLPF_FEED(m_V, V / m_I, m_alpha);

    m_iq.push_back(I + SU_I * Q);
    m_uv.push_back(U + SU_I * V);

    ui->iLabel->setText(SuWidgetsHelpers::formatPowerOf10(I));
    ui->qLabel->setText(QString::asprintf("%+3.6lf%%", 1e2 * m_Q));
    ui->uLabel->setText(QString::asprintf("%+3.6lf%%", 1e2 * m_U));
    ui->vLabel->setText(QString::asprintf("%+3.6lf%%", 1e2 * m_V));

    refreshData();
  }
}

void
PolarimetryPage::feedMeasurements(
    const SUCOMPLEX *hSamp,
    const SUCOMPLEX *vSamp,
    SUSCOUNT size)
{
  if (m_intSamples == 0)
    return;

  for (SUSCOUNT i = 0; i < size; ++i) {
    m_Ex += hSamp[i];
    m_Ey += vSamp[i];

    m_accumPwr += SU_C_REAL(
          hSamp[i] * SU_C_CONJ(hSamp[i])
        + vSamp[i] * SU_C_CONJ(vSamp[i]));
    ++m_accumCount;

    if (m_accumCount == m_intSamples)
      updateAll();
  }

  if (m_accumCount == m_intSamples)
    updateAll();
}

const SUCOMPLEX *
PolarimetryPage::adjustVSamp(const SUCOMPLEX *v, size_t size)
{
  m_vSamp.resize(size);

  for (size_t i = 0; i < size; ++i)
    m_vSamp[i] = v[i] * m_vFactor;

  return m_vSamp.data();
}

void
PolarimetryPage::feed(
    struct timeval const &tv,
    const SUCOMPLEX *hSamp,
    const SUCOMPLEX *vSamp,
    SUSCOUNT size)
{
  if (m_config->swapVH) {
    const SUCOMPLEX *v = adjustVSamp(hSamp, size);
    feedMeasurements(vSamp, hSamp, size);
    ui->polarizationView->feed(vSamp, v, size);
  } else {
    const SUCOMPLEX *v = adjustVSamp(vSamp, size);
    feedMeasurements(hSamp, vSamp, size);
    ui->polarizationView->feed(hSamp, v, size);
  }
}

void
PolarimetryPage::setProperties(
    Polarimeter *owner,
    SUFLOAT sampRate,
    SUFREQ frequency,
    SUFLOAT)
{
  m_owner = owner;

  m_frequency = frequency;

  if (!m_paramsSet) {
    m_sampRate = sampRate;

    emit nameChanged(
          "Polarimetry at " + SuWidgetsHelpers::formatQuantity(
            frequency,
            "Hz"));
  }

  calcIntegrationTime();
  m_paramsSet = true;
}

std::string
PolarimetryPage::getLabel() const
{
  return "Polarimetry";
}

void
PolarimetryPage::closeRequested()
{
  emit closeReq();
}

void
PolarimetryPage::setColorConfig(ColorConfig const &cfg)
{
  QString css;
  Waveform *wfs[4] = {
    ui->IWaveform, ui->QWaveform, ui->UWaveform, ui->VWaveform
  };

  css += "color: " + cfg.lcdForeground.name() + ";\n";
  css += "background-color: " + cfg.lcdBackground.name() + ";\n";

  ui->iLabel->setStyleSheet(css);
  ui->qLabel->setStyleSheet(css);
  ui->uLabel->setStyleSheet(css);
  ui->vLabel->setStyleSheet(css);

  for (unsigned int i = 0; i < 4; ++i) {
    wfs[i]->setBackgroundColor(cfg.spectrumBackground);
    wfs[i]->setForegroundColor(cfg.spectrumForeground);
    wfs[i]->setAxesColor(cfg.spectrumAxes);
    wfs[i]->setTextColor(cfg.spectrumText);
    wfs[i]->setSelectionColor(cfg.selection);
  }

  ui->polarizationView->setBackgroundColor(cfg.spectrumBackground);
  ui->polarizationView->setForegroundColor(cfg.spectrumForeground);

  ui->polarizationView->setAxesColor(cfg.spectrumAxes);
}

Suscan::Serializable *
PolarimetryPage::allocConfig(void)
{
  return m_config = new PolarimetryPageConfig();
}

void
PolarimetryPage::refreshUi()
{
  ui->fitVButton->setEnabled(!m_config->autoFit);
}

void
PolarimetryPage::showEvent(QShowEvent *)
{
  /*
  ui->polarizationView->setMinimumWidth(ui->actionsWidget->height());
  ui->polarizationView->setMinimumHeight(ui->actionsWidget->height());

  ui->polarizationView->setMaximumWidth(ui->actionsWidget->height());
  ui->polarizationView->setMaximumHeight(ui->actionsWidget->height());*/
}

void
PolarimetryPage::applyAntennaConfig()
{
  auto radians = SU_DEG2RAD(m_config->relativePhase);

  m_vFactor = SU_C_EXP(
        SU_ASFLOAT(LOG_OF_1DB * m_config->relativeGain) + SU_I * radians);

  if (m_config->flipVH)
    m_vFactor = -m_vFactor;

}

void
PolarimetryPage::applyPlotConfig()
{
  setAutoScroll(m_config->autoScroll);
}

void
PolarimetryPage::applyConfig()
{
  BLOCKSIG(ui->autoFitButton, setChecked(m_config->autoFit));
  BLOCKSIG(ui->autoScrollButton, setChecked(m_config->autoScroll));
  BLOCKSIG(ui->swapVHCheck, setChecked(m_config->swapVH));
  BLOCKSIG(ui->mirrorVHCheck, setChecked(m_config->flipVH));
  BLOCKSIG(ui->vhGainSPin, setValue(m_config->relativeGain));
  BLOCKSIG(ui->vhPhaseSpin, setValue(m_config->relativePhase));

  setAutoScroll(m_config->autoScroll);

  calcIntegrationTime();
  applyAntennaConfig();

  refreshUi();
}

void
PolarimetryPage::fitVertical(Waveform *waveform)
{
  if (waveform == nullptr) {
    fitVertical(ui->IWaveform);
    fitVertical(ui->QWaveform);
    fitVertical(ui->UWaveform);
    fitVertical(ui->VWaveform);
  } else {
    if (waveform->getDataLength() > 0) {
      SUCOMPLEX dataMin = waveform->getDataMin();
      SUCOMPLEX dataMax = waveform->getDataMax();
      qreal min, max;

      min = SU_C_REAL(dataMin);
      max = SU_C_REAL(dataMax);

      if (min == max) {
        min -= 1;
        max += 1;
      }

      waveform->zoomVertical(min, max);
    }
  }
}

void
PolarimetryPage::fitHorizontal(Waveform *waveform)
{
  if (waveform == nullptr) {
    fitHorizontal(ui->IWaveform);
    fitHorizontal(ui->QWaveform);
    fitHorizontal(ui->UWaveform);
    fitHorizontal(ui->VWaveform);
  } else {
    waveform->zoomHorizontalReset();
  }
}

void
PolarimetryPage::setAutoScroll(bool autoScroll, Waveform *waveform)
{
  if (waveform == nullptr) {
    setAutoScroll(autoScroll, ui->IWaveform);
    setAutoScroll(autoScroll, ui->QWaveform);
    setAutoScroll(autoScroll, ui->UWaveform);
    setAutoScroll(autoScroll, ui->VWaveform);
  } else {
    waveform->setAutoScroll(autoScroll);
  }
}

void
PolarimetryPage::refreshData(Waveform *waveform)
{
  if (waveform == nullptr) {
    refreshData(ui->IWaveform);
    refreshData(ui->QWaveform);
    refreshData(ui->UWaveform);
    refreshData(ui->VWaveform);
  } else {
    waveform->refreshData();
  }
}

void
PolarimetryPage::zoomWaveforms()
{
  qint64 width = ui->IWaveform->getVerticalAxisWidth();
  qint64 end   =static_cast<qint64>(ui->IWaveform->size().width()) - width;

  ui->IWaveform->zoomHorizontal(-width, end);
  ui->QWaveform->zoomHorizontal(-width, end);
  ui->UWaveform->zoomHorizontal(-width, end);
  ui->VWaveform->zoomHorizontal(-width, end);
}

void
PolarimetryPage::calcIntegrationTime()
{
  if (m_config == nullptr || m_sampRate <= 0) {
    m_intSamples = 0;
  } else {
    qreal fs;
    m_intSamples = SCAST(SUSCOUNT, m_config->integratiomTime * m_sampRate);

    fs = 1. / SCAST(qreal, m_config->integratiomTime);

    ui->IWaveform->setSampleRate(fs);
    ui->QWaveform->setSampleRate(fs);
    ui->UWaveform->setSampleRate(fs);
    ui->VWaveform->setSampleRate(fs);

    m_alpha = SU_SPLPF_ALPHA(POLARIMETER_STOKES_UPDATE_TAU * fs);
  }
}

void
PolarimetryPage::updateAll()
{
  bool firstTime = m_iq.empty();

  updateGain();
  updateStokes();

  if (firstTime)
    zoomWaveforms();

  if (m_config->autoFit)
    fitVertical();

  m_accumPwr   = 0;
  m_accumCount = 0;
  m_Ex = m_Ey  = 0;
}

void
PolarimetryPage::setTimeStamp(struct timeval const &ts)
{
  m_lastTimeStamp = ts;
}

PolarimetryPage::~PolarimetryPage()
{
  delete ui;
}

////////////////////////////////// Slots ///////////////////////////////////////
void
PolarimetryPage::onClear()
{
  m_iq.clear();
  m_uv.clear();

  m_I = m_Q = m_U = m_V = 0.;

  refreshData();
  fitVertical();
}

void
PolarimetryPage::onSave()
{

}

void
PolarimetryPage::onVFit()
{
  fitVertical();
}

void
PolarimetryPage::onHFit()
{
  fitHorizontal();
}

void
PolarimetryPage::onAutoScroll()
{
  m_config->autoScroll = ui->autoScrollButton->isChecked();
  refreshUi();

  applyPlotConfig();
}

void
PolarimetryPage::onAutoFit()
{
  m_config->autoFit = ui->autoFitButton->isChecked();
  refreshUi();
}

void
PolarimetryPage::onAntennaChanged()
{
  m_config->relativePhase = ui->vhPhaseSpin->value();
  m_config->relativeGain  = ui->vhGainSPin->value();
  m_config->flipVH        = ui->mirrorVHCheck->isChecked();
  m_config->swapVH        = ui->swapVHCheck->isChecked();

  applyAntennaConfig();
}

