//
//    PhasePlotPage.cpp: description
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
#include "PhasePlotPage.h"
#include "ui_PhasePlotPage.h"
#include <ColorConfig.h>

using namespace SigDigger;

#define STRINGFY(x) #x
#define STORE(field) obj.set(STRINGFY(field), this->field)
#define LOAD(field) this->field = conf.get(STRINGFY(field), this->field)

void
PhasePlotPageConfig::deserialize(Suscan::Object const &conf)
{
  LOAD(autoFit);
  LOAD(autoScroll);
  LOAD(gainDb);
  LOAD(phaseOrigin);
  LOAD(logEvents);
  LOAD(measurementTime);
  LOAD(coherenceThreshold);
}

Suscan::Object &&
PhasePlotPageConfig::serialize()
{
  Suscan::Object obj(SUSCAN_OBJECT_TYPE_OBJECT);

  obj.setClass("PhasePlotPageConfig");

  STORE(autoFit);
  STORE(autoScroll);
  STORE(gainDb);
  STORE(phaseOrigin);
  STORE(logEvents);
  STORE(measurementTime);
  STORE(coherenceThreshold);

  return persist(obj);
}


PhasePlotPage::PhasePlotPage(
    TabWidgetFactory *factory,
    UIMediator *mediator,
    QWidget *parent) :
  TabWidget(factory, mediator, parent),
  ui(new Ui::PhasePlotPage)
{
  ui->setupUi(this);

  ui->waveform->setShowWaveform(false);
  ui->waveform->setShowEnvelope(true);
  ui->waveform->setShowPhase(true);
  ui->waveform->setAutoFitToEnvelope(true);
  ui->waveform->setData(&m_data);
  ui->waveform->setAutoScroll(true);

  ui->phaseView->setHistorySize(100);
}


void
PhasePlotPage::feed(const SUCOMPLEX *data, SUSCOUNT size)
{
  for (SUSCOUNT i = 0; i < size; ++i)
    m_accumulated += data[i];
  m_accumCount += size;


  if (m_paramsSet) {
    bool first = m_data.size() == 0;
    m_data.insert(m_data.end(), data, data + size);
    ui->waveform->refreshData();

    if (first)
      ui->waveform->zoomHorizontal(0., 10.);
    ui->phaseView->feed(data, size);
  }
}

void
PhasePlotPage::setProperties(PhaseComparator *owner, SUFREQ frequency, SUFLOAT sampRate)
{
  m_owner = owner;

  if (!m_paramsSet)
    ui->waveform->setSampleRate(sampRate);

  m_paramsSet = true;
}

std::string
PhasePlotPage::getLabel() const
{
  return "Phase plot";
}

void
PhasePlotPage::closeRequested()
{
  emit closeReq();
}

void
PhasePlotPage::setColorConfig(ColorConfig const &cfg)
{
  ui->waveform->setBackgroundColor(cfg.spectrumBackground);
  ui->waveform->setForegroundColor(cfg.spectrumForeground);
  ui->waveform->setAxesColor(cfg.spectrumAxes);
  ui->waveform->setTextColor(cfg.spectrumText);
  ui->waveform->setSelectionColor(cfg.selection);

  ui->phaseView->setBackgroundColor(cfg.spectrumBackground);
  ui->phaseView->setAxesColor(cfg.spectrumAxes);
}

Suscan::Serializable *
PhasePlotPage::allocConfig(void)
{
  return m_config = new PhasePlotPageConfig();
}

void
PhasePlotPage::refreshUi()
{
  BLOCKSIG(ui->autoFitButton,          setChecked(m_config->autoFit));
  BLOCKSIG(ui->autoScrollButton,       setChecked(m_config->autoScroll));
  BLOCKSIG(ui->gainSpin,               setValue(m_config->gainDb));
  BLOCKSIG(ui->enableLoggerButton,     setChecked(m_config->logEvents));
  BLOCKSIG(ui->measurementTimeSpin,    setTimeValue(m_config->measurementTime));
  BLOCKSIG(ui->coherenceThresholdSpin, setValue(m_config->coherenceThreshold));

  ui->gainSpin->setEnabled(!m_config->autoFit);
  ui->waveform->setAutoFitToEnvelope(m_config->autoFit);
  ui->waveform->setAutoScroll(m_config->autoScroll);

  if (!m_config->autoFit) {
    m_gain = SU_POWER_MAG_RAW(m_config->gainDb);
    qreal limits = 1 / m_gain;
    ui->waveform->zoomHorizontal(-limits, +limits);
    ui->phaseView->setGain(m_gain);
  }
}

void
PhasePlotPage::applyConfig(void)
{
  refreshUi();
}

void
PhasePlotPage::setTimeStamp(struct timeval const &)
{
  if (m_accumCount > 0) {
    SUFLOAT mag;
    m_accumulated /= m_accumCount;
    mag = SU_C_ABS(m_accumulated);
    if (mag > m_max) {
      m_max = mag;
      ui->phaseView->setGain(1. / m_max);
    } else {
      SU_SPLPF_FEED(m_max, mag, 1e-2);
      if (m_max > std::numeric_limits<SUFLOAT>::epsilon())
        ui->phaseView->setGain(1. / m_max);
    }
    m_accumCount = 0;
  }
}

PhasePlotPage::~PhasePlotPage()
{
  delete ui;
}
