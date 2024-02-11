//
//    Polarimeter.cpp: description
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

#include <UIMediator.h>
#include <MainSpectrum.h>
#include <QDir>
#include <GlobalProperty.h>
#include <QTextStream>

#include "Polarimeter.h"
#include "PolarimetryPage.h"
#include "PolarimetryPageFactory.h"
#include "CoherentChannelForwarder.h"
#include "ui_Polarimeter.h"

#define STRINGFY(x) #x
#define STORE(field) obj.set(STRINGFY(field), this->field)
#define LOAD(field) this->field = conf.get(STRINGFY(field), this->field)

using namespace SigDigger;

//////////////////////////// Widget config /////////////////////////////////////
void
PolarimeterConfig::deserialize(Suscan::Object const &conf)
{
  LOAD(collapsed);
}

Suscan::Object &&
PolarimeterConfig::serialize()
{
  Suscan::Object obj(SUSCAN_OBJECT_TYPE_OBJECT);

  obj.setClass("PolarimeterConfig");

  STORE(collapsed);

  return persist(obj);
}

/////////////////////////// Widget implementation //////////////////////////////
Polarimeter::Polarimeter(
    PolarimeterFactory *factory,
    UIMediator *mediator,
    QWidget *parent) :
  ToolWidget(factory, mediator, parent),
  ui(new Ui::Polarimeter)
{
  ui->setupUi(this);

  assertConfig();

  m_forwarder = new CoherentChannelForwarder(mediator, this);
  m_mediator  = mediator;
  m_spectrum  = mediator->getMainSpectrum();

  setProperty("collapsed", m_panelConfig->collapsed);

  refreshUi();
  connectAll();
}

Polarimeter::~Polarimeter()
{
  delete ui;
}

void
Polarimeter::connectAll()
{
  connect(
        ui->openButton,
        SIGNAL(clicked(bool)),
        this,
        SLOT(onOpenChannel()));

  connect(
        ui->closeButton,
        SIGNAL(clicked(bool)),
        this,
        SLOT(onCloseChannel()));

  connect(
        ui->frequencySpin,
        SIGNAL(valueChanged(double)),
        this,
        SLOT(onAdjustFrequency()));

  connect(
        ui->bandwidthSpin,
        SIGNAL(valueChanged(double)),
        this,
        SLOT(onAdjustBandwidth()));

  connect(
        m_spectrum,
        SIGNAL(frequencyChanged(qint64)),
        this,
        SLOT(onSpectrumFrequencyChanged(qint64)));

  connect(
        m_forwarder,
        SIGNAL(opened()),
        this,
        SLOT(onComparatorOpened()));

  connect(
        m_forwarder,
        SIGNAL(error(QString)),
        this,
        SLOT(onComparatorError(QString)));

  connect(
        m_forwarder,
        SIGNAL(closed()),
        this,
        SLOT(onComparatorClosed()));

  connect(
        m_forwarder,
        SIGNAL(dataAvailable()),
        this,
        SLOT(onComparatorData()));

  connect(
        m_forwarder,
        SIGNAL(stateChanged(int,int,QString)),
        this,
        SLOT(onComparatorStateChanged(int, int, QString)));
}

void
Polarimeter::updatePlotProperties()
{
  if (m_plotPage != nullptr)
    m_plotPage->setProperties(
          this,
          m_forwarder->getEquivFs(),
          ui->frequencySpin->value(),
          ui->bandwidthSpin->value());
}

void
Polarimeter::applySpectrumState()
{
  if (m_analyzer != nullptr) {
    qreal fc = SCAST(qreal, m_spectrum->getCenterFreq());
    qreal fs = SCAST(qreal, m_analyzer->getSampleRate());
    qreal min = fc - .5 * fs;
    qreal max = fc + .5 * fs;

    ui->frequencySpin->setMinimum(min);
    ui->frequencySpin->setMaximum(max);

    updatePlotProperties();
  }

  onAdjustFrequency();
}

QColor
Polarimeter::channelColor(bool state) const
{
  if (state)
    return QColor("#00ff00");
  else
    return QColor("#bfbf00");
}

void
Polarimeter::refreshNamedChannel()
{
  bool shouldHaveNamChan =
         m_analyzer != nullptr
      && m_forwarder->isRunning();

  // Check whether we should have a named channel here.
  if (shouldHaveNamChan != m_haveNamChan) { // Inconsistency!
    m_haveNamChan = shouldHaveNamChan;

    // Make sure we have a named channel
    if (m_haveNamChan) {
      auto chBw   = m_forwarder->getTrueBandwidth();
      auto loFreq = m_forwarder->getFrequencyLo();
      auto hiFreq = m_forwarder->getFrequencyHi();

      m_namChanLo = m_mediator->getMainSpectrum()->addChannel(
            "Phase comparator (LO)",
            loFreq,
            -chBw / 2,
            +chBw / 2,
            channelColor(m_loOpened),
            channelColor(m_loOpened),
            channelColor(m_loOpened));
      m_namChanHi = m_mediator->getMainSpectrum()->addChannel(
            "Phase comparator (HI)",
            hiFreq,
            -chBw / 2,
            +chBw / 2,
            channelColor(m_hiOpened),
            channelColor(m_hiOpened),
            channelColor(m_hiOpened));
    } else {
      // We should NOT have a named channel, remove
      m_spectrum->removeChannel(m_namChanLo);
      m_spectrum->removeChannel(m_namChanHi);
      m_spectrum->updateOverlay();
    }
  } else if (m_haveNamChan) {
    auto chBw   = m_forwarder->getTrueBandwidth();
    auto loFreq = m_forwarder->getFrequencyLo();
    auto hiFreq = m_forwarder->getFrequencyHi();

    m_namChanLo.value()->frequency   = loFreq;
    m_namChanLo.value()->lowFreqCut  = -chBw / 2;
    m_namChanLo.value()->highFreqCut = +chBw / 2;
    m_namChanLo.value()->cutOffColor = channelColor(m_loOpened);
    m_namChanLo.value()->markerColor = channelColor(m_loOpened);
    m_namChanLo.value()->boxColor    = channelColor(m_loOpened);
    m_spectrum->refreshChannel(m_namChanLo);

    m_namChanHi.value()->frequency   = hiFreq;
    m_namChanHi.value()->lowFreqCut  = -chBw / 2;
    m_namChanHi.value()->highFreqCut = +chBw / 2;
    m_namChanHi.value()->cutOffColor = channelColor(m_hiOpened);
    m_namChanHi.value()->markerColor = channelColor(m_hiOpened);
    m_namChanHi.value()->boxColor    = channelColor(m_hiOpened);
    m_spectrum->refreshChannel(m_namChanHi);

    m_spectrum->updateOverlay();
  }
}

void
Polarimeter::refreshUi()
{
  bool running   = m_forwarder->isRunning();
  bool canRun    = m_analyzer != nullptr && !running;
  bool canAdjust = running;

  ui->frequencySpin->setEnabled(canAdjust);
  ui->bandwidthSpin->setEnabled(canAdjust);

  BLOCKSIG_BEGIN(ui->openButton);
    ui->openButton->setEnabled(canRun);
    ui->closeButton->setEnabled(running);
  BLOCKSIG_END();
}

// Configuration methods
Suscan::Serializable *
Polarimeter::allocConfig()
{
  return m_panelConfig = new PolarimeterConfig();
}

void
Polarimeter::applyConfig()
{
  setProperty("collapsed", m_panelConfig->collapsed);

  refreshUi();
}

bool
Polarimeter::event(QEvent *event)
{
  if (event->type() == QEvent::DynamicPropertyChange) {
    QDynamicPropertyChangeEvent *const propEvent =
        static_cast<QDynamicPropertyChangeEvent*>(event);
    QString propName = propEvent->propertyName();
    if (propName == "collapsed")
      m_panelConfig->collapsed = property("collapsed").value<bool>();
  }

  return QWidget::event(event);
}

// Overriden methods
void
Polarimeter::setState(int, Suscan::Analyzer *analyzer)
{
  m_analyzer = analyzer;
  m_forwarder->setAnalyzer(analyzer);


  if (analyzer != nullptr) {
    auto windowSize = m_mediator->getAnalyzerParams()->windowSize;
    m_forwarder->setFFTSizeHint(windowSize);
    applySpectrumState();
  }

  refreshNamedChannel();
  refreshUi();
}

void
Polarimeter::openPlot()
{
  auto sus = Suscan::Singleton::get_instance();
  auto factory = sus->findTabWidgetFactory("PolarimetryPage");

  if (m_analyzer == nullptr)
    return;

  m_plotPage = SCAST(PolarimetryPage *, factory->make(m_mediator));
  m_plotPage->setColorConfig(m_colors);

  connect(
        m_plotPage,
        SIGNAL(closeReq()),
        this,
        SLOT(onClosePlotPage()));

  connect(
        m_plotPage,
        SIGNAL(frequencyChanged(double)),
        this,
        SLOT(onAdjustFrequencyRequested(qreal)));


  connect(
        m_plotPage,
        SIGNAL(bandwidthChanged(qreal)),
        this,
        SLOT(onAdjustBandwidthRequested(qreal)));

  updatePlotProperties();

  m_mediator->addTabWidget(m_plotPage);
}

void
Polarimeter::setQth(Suscan::Location const &)
{

}

void
Polarimeter::setColorConfig(ColorConfig const &colors)
{
  m_colors = colors;
}

void
Polarimeter::setTimeStamp(struct timeval const &)
{

}

void
Polarimeter::setProfile(Suscan::Source::Config &)
{

}


////////////////////////////// Slots ///////////////////////////////////////////
void
Polarimeter::onOpenChannel()
{
  auto bandwidth  = m_spectrum->getBandwidth();
  auto loFreq     = m_spectrum->getLoFreq();
  auto centerFreq = m_spectrum->getCenterFreq();
  auto delta      = m_analyzer->getSampleRate() * .25;
  auto freq       = centerFreq + loFreq;

  BLOCKSIG(ui->bandwidthSpin, setValue(bandwidth));
  BLOCKSIG(ui->frequencySpin, setValue(freq + delta));

  auto result = m_forwarder->open(freq, bandwidth);

  if (!result) {
    QMessageBox::critical(
          this,
          "Cannot open inspector",
          "Failed to open phase comparator. See log window for details");
  }
}

void
Polarimeter::onAdjustFrequencyRequested(qreal freq)
{
  ui->frequencySpin->setValue(freq);
  onAdjustFrequency();
}

void
Polarimeter::onAdjustBandwidthRequested(qreal bw)
{
  ui->bandwidthSpin->setValue(bw);
  onAdjustBandwidth();
}

void
Polarimeter::onCloseChannel()
{
  m_forwarder->close();
}

void
Polarimeter::onAdjustFrequency()
{
  if (m_analyzer != nullptr) {
    auto delta = m_analyzer->getSampleRate() * .25;
    m_forwarder->setFrequency(ui->frequencySpin->value() - delta);
    updatePlotProperties();
    refreshNamedChannel();
  }
}

void
Polarimeter::onAdjustBandwidth()
{
  m_forwarder->setBandwidth(ui->bandwidthSpin->value());
  updatePlotProperties();
  refreshNamedChannel();
}

void
Polarimeter::onSpectrumFrequencyChanged(qint64)
{
  applySpectrumState();
}


void
Polarimeter::onComparatorOpened()
{
  m_count = 0;

  ui->stateLabel->setText("Comparator opened");
  openPlot();

  refreshUi();
  refreshNamedChannel();
}

void
Polarimeter::onComparatorClosed()
{
  ui->stateLabel->setText("Comparator closed");
  m_plotPage = nullptr;

  refreshUi();
  refreshNamedChannel();
}

void
Polarimeter::onComparatorError(QString error)
{
  ui->stateLabel->setText("Comparator error: " + error);
  m_plotPage = nullptr;

  refreshUi();
  refreshNamedChannel();
}

void
Polarimeter::onComparatorData()
{
  auto hiData = m_forwarder->hiData();
  auto loData = m_forwarder->loData();

  if (m_plotPage != nullptr && m_analyzer != nullptr) {
    m_plotPage->feed(
          m_analyzer->getSourceTimeStamp(),
          hiData.data(),
          loData.data(),
          loData.size());
  }

  ++m_count;
  //ui->stateLabel->setText(QString::number(m_count) + " packets");
}

void
Polarimeter::onComparatorStateChanged(int ch, int state, QString msg)
{
  bool fullyOpened = state == RAW_CHANNEL_FORWARDER_RUNNING;

  if (ch == 0)
    m_loOpened = fullyOpened;
  else
    m_hiOpened = fullyOpened;

  if (state != RAW_CHANNEL_FORWARDER_IDLE)
    ui->stateLabel->setText("Channel " + QString::number(ch + 1) + ": " + msg);

  refreshNamedChannel();
}

void
Polarimeter::onClosePlotPage()
{
  PolarimetryPage *sender = SCAST(PolarimetryPage *, QObject::sender());

  if (sender == m_plotPage) {
    m_plotPage = nullptr;
    m_forwarder->close();
  }

  delete sender;
}
