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
#include "CoherentDetector.h"
#include <QDateTime>
#include <SigDiggerHelpers.h>
#include <QFileDialog>
#include <QMessageBox>
#include <QTextStream>

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
  LOAD(maxAlloc);
  LOAD(angleOfArrival);
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
  STORE(maxAlloc);
  STORE(angleOfArrival);

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

  ui->savePlotButton->setEnabled(false);

  m_data.reserve(1 << 10);
  m_detector = new CoherentDetector();

  connectAll();
}

void
PhasePlotPage::connectAll()
{
  connect(
        ui->savePlotButton,
        SIGNAL(clicked(bool)),
        this,
        SLOT(onSavePlot()));

  connect(
        ui->autoScrollButton,
        SIGNAL(toggled(bool)),
        this,
        SLOT(onAutoScrollToggled()));

  connect(
        ui->phaseAoAButton,
        SIGNAL(toggled(bool)),
        this,
        SLOT(onAoAToggled()));

  connect(
        ui->clearButton,
        SIGNAL(clicked(bool)),
        this,
        SLOT(onClear()));

  connect(
        ui->autoFitButton,
        SIGNAL(toggled(bool)),
        this,
        SLOT(onAutoFitToggled()));

  connect(
        ui->gainSpin,
        SIGNAL(valueChanged(double)),
        this,
        SLOT(onGainChanged()));

  connect(
        ui->freqSpin,
        SIGNAL(valueChanged(double)),
        this,
        SLOT(onChangeFrequency()));

  connect(
        ui->bwSpin,
        SIGNAL(valueChanged(double)),
        this,
        SLOT(onChangeBandwidth()));

  connect(
        ui->phaseOriginSpin,
        SIGNAL(valueChanged(double)),
        this,
        SLOT(onChangePhaseOrigin()));

  connect(
        ui->maxAllocMiBSpin,
        SIGNAL(valueChanged(double)),
        this,
        SLOT(onMaxAllocChanged()));

  connect(
        ui->measurementTimeSpin,
        SIGNAL(changed(qreal,qreal)),
        this,
        SLOT(onChangeMeasurementTime()));

  connect(
        ui->coherenceThresholdSpin,
        SIGNAL(valueChanged(double)),
        this,
        SLOT(onChangeCoherenceThreshold()));

  connect(
        ui->enableLoggerButton,
        SIGNAL(toggled(bool)),
        this,
        SLOT(onLogEnableToggled()));

  connect(
        ui->saveLogButton,
        SIGNAL(clicked(bool)),
        this,
        SLOT(onSaveLog()));

  connect(
        ui->clearLogButton,
        SIGNAL(clicked(bool)),
        this,
        SLOT(onClearLog()));
}

void
PhasePlotPage::logText(QString const &text)
{
  logText(m_lastTimeStamp, text);
}

void
PhasePlotPage::logText(struct timeval const &time, QString const &text)
{
  QString prefix;
  QDateTime timestamp = QDateTime::fromSecsSinceEpoch(time.tv_sec);
  QString date = timestamp.toUTC().toString();
  prefix = "[" + date + "] ";

  ui->logTextEdit->appendPlainText(prefix + text);
}

void
PhasePlotPage::feed(struct timeval const &tv, const SUCOMPLEX *data, SUSCOUNT size)
{
  SUSCOUNT ptr = 0, got;
  SUSCOUNT newSize, newAlloc;

  for (SUSCOUNT i = 0; i < size; ++i)
    m_accumulated += data[i];

  m_accumCount += size;

  if (ui->logTextEdit->document()->isEmpty())
    logDetectorInfo();

  if (m_paramsSet) {
    bool first = m_data.size() == 0;
    SUSCOUNT orig = m_data.size();

    // Ensure size and rollover
    newSize = orig + size;
    if (newSize > m_data.capacity()) {
      // Time to reallocate!
      size_t maxAlloc = m_config->maxAlloc / sizeof(SUCOMPLEX);
      ui->waveform->safeCancel();

      // Ideally, attempt to double capacity
      newAlloc = 2 * m_data.capacity();

      if (newAlloc <= maxAlloc) {
        // We can
        m_data.reserve(newAlloc);
      } else {
        // We are hiting the limit, is it enough?
        if (newSize < maxAlloc) {
          // Yes
          m_data.reserve(maxAlloc);
        } else {
          // No, rollover
          logText(
                "Maximum buffer size reached (" +
                SuWidgetsHelpers::formatBinaryQuantity(maxAlloc * sizeof(SUCOMPLEX)) +
                "), clearing buffer");
          orig = 0;
        }
      }
    }

    if (orig == 0)
      clearData();

    m_data.resize(orig + size);

    for (SUSCOUNT i = 0; i < size; ++i)
      m_data[i + orig] = data[i] * m_phaseAdjust;

    if (first) {
      ui->waveform->zoomHorizontal(0., 10.);
      ui->savePlotButton->setEnabled(true);
      ui->waveform->refreshData();
    }

    ui->phaseView->feed(data, size);
  }

  if (m_config->logEvents && m_detector->enabled()) {
    while (ptr < size) {
      got = m_detector->feed(data + ptr, size - ptr);
      if (m_detector->triggered() != m_haveEvent) {
        struct timeval delta, time;
        qreal progress = ptr / m_sampRate;
        m_haveEvent = m_detector->triggered();

        delta.tv_sec  = std::floor(progress);
        delta.tv_usec = std::floor(progress * 1e6);
        timeradd(&tv, &delta, &time);

        if (m_haveEvent) {
          m_lastEvent = time;
          logText(time, "Coherent event detected.");
        } else {
          if (m_detector->haveEvent()) {
            QString phaseInfoText;
            timersub(&time, &m_lastEvent, &delta);
            qreal asSeconds = delta.tv_sec + delta.tv_usec * 1e-6;

            if (m_config->angleOfArrival) {
              qreal aoa1, aoa2;

              aoa1 = -SU_ASIN(m_detector->lastPhase() / M_PI);
              aoa2 = M_PI - aoa1;

              phaseInfoText =
                  "AoA = " + SuWidgetsHelpers::formatQuantity(
                    SU_RAD2DEG(aoa1),
                    4,
                    "deg",
                    true) +
                  " or " + SuWidgetsHelpers::formatQuantity(
                    SU_RAD2DEG(aoa2),
                    4,
                    "deg",
                    true);
            } else {
              phaseInfoText =
                  "dPhi = " +SuWidgetsHelpers::formatQuantity(
                    SU_RAD2DEG(m_detector->lastPhase()),
                    4,
                    "º");
            }

            logText(
                  time,
                  "Coherent event end. T = " +
                  SuWidgetsHelpers::formatQuantity(asSeconds, 4, "s") +
                  ", S = " +
                  QString::number(SU_POWER_DB_RAW(m_detector->lastPower())) +
                  " dB, " +
                  phaseInfoText);
          }
        }
      }
      ptr += got;
    }
  }
}

void
PhasePlotPage::logDetectorInfo()
{
  logText("Coherent detector configuration:");
  logText("  Channel:              " + QString::number(ui->freqSpin->value()) + " Hz, " + QString::number(ui->bwSpin->value()) + " Hz bandwidth");
  logText("  Gain:                 " + QString::number(ui->gainSpin->value()) + " dB, " + SuWidgetsHelpers::formatQuantity(m_config->phaseOrigin, "º") + " offset");
  logText("  Max phase dispersion: " + SuWidgetsHelpers::formatQuantity(m_config->coherenceThreshold, "º"));
  logText("  Measuremen interval:  " + SuWidgetsHelpers::formatQuantity(
            m_config->measurementTime,
            4,
            "s"));
}

void
PhasePlotPage::clearData()
{
  qint64 size = m_data.size() * sizeof(SUCOMPLEX);
  size_t prevAlloc = m_data.capacity();

  ui->waveform->safeCancel();

  m_data.resize(0);
  m_data.reserve(prevAlloc);

  ui->waveform->refreshData();
  ui->savePlotButton->setEnabled(false);

  if (m_haveEvent) {
    m_haveEvent = false;
    logText(
          "Data buffer cleared after a capture of " +
          SuWidgetsHelpers::formatBinaryQuantity(size));
  }
}

void
PhasePlotPage::setFreqencyLimits(SUFREQ min, SUFREQ max)
{
  ui->freqSpin->setMinimum(min);
  ui->freqSpin->setMaximum(max);
}

void
PhasePlotPage::setProperties(
    PhaseComparator *owner,
    SUFLOAT sampRate,
    SUFREQ frequency,
    SUFLOAT bandwidth)
{
  m_owner = owner;

  if (!m_paramsSet) {
    m_sampRate = sampRate;
    ui->waveform->setSampleRate(sampRate);
    ui->bwSpin->setMinimum(0);
    ui->bwSpin->setMaximum(sampRate);
    ui->measurementTimeSpin->setTimeMin(2 / m_sampRate);
    ui->measurementTimeSpin->setTimeMax(3600);
    ui->measurementTimeSpin->setBestUnits(true);

    ui->sampRateLabel->setText(
          SuWidgetsHelpers::formatQuantity(sampRate, 4, "sps"));


    emit nameChanged(
          "Phase comparison at " + SuWidgetsHelpers::formatQuantity(
            frequency,
            "Hz"));
  }

  BLOCKSIG(ui->freqSpin, setValue(frequency));
  BLOCKSIG(ui->bwSpin,   setValue(bandwidth));

  m_paramsSet = true;
}

std::string
PhasePlotPage::getLabel() const
{
  return ("Phase comparison at " + SuWidgetsHelpers::formatQuantity(
          ui->freqSpin->value(),
          "Hz")).toStdString();
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
  ui->phaseView->setForegroundColor(cfg.spectrumForeground);

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
  BLOCKSIG(ui->maxAllocMiBSpin,        setValue(m_config->maxAlloc / (1 << 20)));
  BLOCKSIG(ui->phaseAoAButton,         setChecked(m_config->angleOfArrival));

  ui->phaseView->setAoA(m_config->angleOfArrival);
  ui->gainSpin->setEnabled(!m_config->autoFit);
  ui->waveform->setAutoFitToEnvelope(m_config->autoFit);
  ui->waveform->setAutoScroll(m_config->autoScroll);

  if (!m_config->autoFit) {
    m_gain = SU_POWER_MAG_RAW(m_config->gainDb);
    qreal limits = 1 / m_gain;


    ui->waveform->zoomVertical(-limits, +limits);
    ui->phaseView->setGain(m_gain);
  }
}

void
PhasePlotPage::applyConfig(void)
{
  refreshUi();

  m_phaseAdjust = SU_C_EXP(-SU_I * SU_DEG2RAD(m_config->phaseOrigin));
  m_detector->resize(m_config->measurementTime * m_sampRate);
  m_detector->setThreshold(SU_DEG2RAD(m_config->coherenceThreshold));
}

void
PhasePlotPage::setTimeStamp(struct timeval const &ts)
{
  // Update gain
  m_lastTimeStamp = ts;

  if (m_config->autoFit && m_accumCount > 0) {
    SUFLOAT mag, gain;
    bool adjust = false;
    m_accumulated /= m_accumCount;
    mag = SU_C_ABS(m_accumulated);

    if (mag > m_max) {
      m_max = mag;
      gain = 1. / m_max;
      adjust = true;
    } else {
      SU_SPLPF_FEED(m_max, mag, 1e-2);
      if (m_max > std::numeric_limits<SUFLOAT>::epsilon()) {
        adjust = true;
        gain = 1. / m_max;
      }
    }

    if (adjust) {
      m_config->gainDb = SU_POWER_DB_RAW(gain);
      BLOCKSIG(ui->gainSpin, setValue(m_config->gainDb));
    }

    m_accumCount = 0;
  }

  // Update buffer size
  ui->sizeLabel->setText(
        SuWidgetsHelpers::formatBinaryQuantity(
          m_data.capacity() * sizeof(SUCOMPLEX)));

  if (m_data.size() > 0)
    ui->waveform->refreshData();
}

PhasePlotPage::~PhasePlotPage()
{
  ui->waveform->safeCancel();
  delete m_detector;
  delete ui;
}

////////////////////////////////// Slots ///////////////////////////////////////
void
PhasePlotPage::onSavePlot()
{
  SigDiggerHelpers::openSaveSamplesDialog(
        this,
        m_data.data(),
        m_data.size(),
        m_sampRate,
        0,
        m_data.size(),
        Suscan::Singleton::get_instance()->getBackgroundTaskController());
}

void
PhasePlotPage::onAutoScrollToggled()
{
  m_config->autoScroll = ui->autoScrollButton->isChecked();
  refreshUi();
}

void
PhasePlotPage::onClear()
{
  clearData();
}

void
PhasePlotPage::onMaxAllocChanged()
{
  bool flush = false;
  m_config->maxAlloc = ui->maxAllocMiBSpin->value() * (1 << 20);

  ui->waveform->safeCancel();

  if (m_data.size() > m_config->maxAlloc) {
    flush = true;
    m_data.resize(0);
  }

  m_data.reserve(m_config->maxAlloc / sizeof(SUCOMPLEX));

  if (flush)
    ui->waveform->setData(&m_data, true, true);
}


void
PhasePlotPage::onAutoFitToggled()
{
  m_config->autoFit = ui->autoFitButton->isChecked();
  refreshUi();
}

void
PhasePlotPage::onGainChanged()
{
  m_config->gainDb = ui->gainSpin->value();
  refreshUi();
}

void
PhasePlotPage::onChangeFrequency()
{
  emit frequencyChanged(ui->freqSpin->value());
}

void
PhasePlotPage::onChangeBandwidth()
{
  emit bandwidthChanged(ui->bwSpin->value());
}

void
PhasePlotPage::onChangePhaseOrigin()
{
  m_config->phaseOrigin = ui->phaseOriginSpin->value();
  m_phaseAdjust = SU_C_EXP(-SU_I * SU_DEG2RAD(m_config->phaseOrigin));
}

void
PhasePlotPage::onChangeMeasurementTime()
{
  m_config->measurementTime = ui->measurementTimeSpin->timeValue();
  m_detector->resize(m_config->measurementTime * m_sampRate);

  logDetectorInfo();
}

void
PhasePlotPage::onChangeCoherenceThreshold()
{
  m_config->coherenceThreshold = ui->coherenceThresholdSpin->value();
  m_detector->setThreshold(SU_DEG2RAD(m_config->coherenceThreshold));

  logDetectorInfo();
}

void
PhasePlotPage::onLogEnableToggled()
{
  m_config->logEvents = ui->enableLoggerButton->isChecked();
  m_detector->reset();
  m_haveEvent = false;
}

void
PhasePlotPage::onAoAToggled()
{
  m_config->angleOfArrival = ui->phaseAoAButton->isChecked();
  refreshUi();
}

void
PhasePlotPage::onSaveLog()
{
  bool done = false;

  do {
    QFileDialog dialog(this);
    QStringList filters;

    dialog.setFileMode(QFileDialog::FileMode::AnyFile);
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setWindowTitle(QString("Save event log"));

    filters << "Event log (*.log)"
            << "All files (*)";

    dialog.setNameFilters(filters);

    if (dialog.exec()) {
      auto selected = dialog.selectedFiles();
      QString path = selected.first();
      QFile outfile;

      outfile.setFileName(path);
      outfile.open(QIODevice::Text | QIODevice::WriteOnly);

      if (!outfile.isOpen()) {
        QMessageBox::critical(
              this,
              "Save event log",
              "Cannot save event file: " + outfile.errorString());
      } else {
        QTextStream out(&outfile);
        out << ui->logTextEdit->toPlainText() << "\n";
        done = true;
      }
    } else {
      done = true;
    }
  } while (!done);
}

void
PhasePlotPage::onClearLog()
{
  ui->logTextEdit->clear();
  m_infoLogged = false;
}
