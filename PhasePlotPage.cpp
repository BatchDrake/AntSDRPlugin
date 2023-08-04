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

void
PhasePlotPageConfig::deserialize(Suscan::Object const &)
{
}

Suscan::Object &&
PhasePlotPageConfig::serialize()
{
  Suscan::Object obj(SUSCAN_OBJECT_TYPE_OBJECT);

  obj.setClass("PhasePlotPageConfig");

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

  m_data.insert(m_data.end(), data, data + size);

  if (m_paramsSet)
    ui->waveform->refreshData();

   ui->phaseView->feed(data, size);
}

void
PhasePlotPage::setProperties(PhaseComparator *owner, SUFREQ frequency, SUFLOAT sampRate)
{
  m_owner = owner;
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
}

Suscan::Serializable *
PhasePlotPage::allocConfig(void)
{
  return m_config = new PhasePlotPageConfig();
}

void
PhasePlotPage::applyConfig(void)
{
  // NO-OP
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
      ui->phaseView->setGain(1 / m_max);
    } else {
      SU_SPLPF_FEED(m_max, 10 * mag, 1e-2);
      if (m_max > std::numeric_limits<SUFLOAT>::epsilon())
        ui->phaseView->setGain(1 / m_max);
    }
    m_accumCount = 0;
  }
}

PhasePlotPage::~PhasePlotPage()
{
  delete ui;
}
