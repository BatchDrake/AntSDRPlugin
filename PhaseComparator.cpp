//
//    PhaseComparator.cpp: description
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

#include "PhaseComparator.h"
#include "PhasePlotPage.h"
#include "PhasePlotPageFactory.h"
#include "SimplePhaseComparator.h"
#include "ui_PhaseComparator.h"

#define STRINGFY(x) #x
#define STORE(field) obj.set(STRINGFY(field), this->field)
#define LOAD(field) this->field = conf.get(STRINGFY(field), this->field)

using namespace SigDigger;

//////////////////////////// Widget config /////////////////////////////////////
void
PhaseComparatorConfig::deserialize(Suscan::Object const &conf)
{
  LOAD(collapsed);
}

Suscan::Object &&
PhaseComparatorConfig::serialize()
{
  Suscan::Object obj(SUSCAN_OBJECT_TYPE_OBJECT);

  obj.setClass("PhaseComparatorConfig");

  STORE(collapsed);

  return persist(obj);
}

/////////////////////////// Widget implementation //////////////////////////////
PhaseComparator::PhaseComparator(
    PhaseComparatorFactory *factory,
    UIMediator *mediator,
    QWidget *parent) :
  ToolWidget(factory, mediator, parent),
  ui(new Ui::PhaseComparator)
{
  ui->setupUi(this);

  assertConfig();

  m_comparator = new SimplePhaseComparator(mediator, this);
  m_mediator  = mediator;
  m_spectrum  = mediator->getMainSpectrum();

  setProperty("collapsed", m_panelConfig->collapsed);

  refreshUi();
  connectAll();
}

PhaseComparator::~PhaseComparator()
{
  delete ui;
}

void
PhaseComparator::connectAll()
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
        m_comparator,
        SIGNAL(opened()),
        this,
        SLOT(onComparatorOpened()));

  connect(
        m_comparator,
        SIGNAL(error(QString)),
        this,
        SLOT(onComparatorError(QString)));

  connect(
        m_comparator,
        SIGNAL(closed()),
        this,
        SLOT(onComparatorClosed()));

  connect(
        m_comparator,
        SIGNAL(dataAvailable()),
        this,
        SLOT(onComparatorData()));

  connect(
        m_comparator,
        SIGNAL(stateChanged(int,int,QString)),
        this,
        SLOT(onComparatorStateChanged(int, int, QString)));
}

void
PhaseComparator::applySpectrumState()
{
  if (m_analyzer != nullptr) {
    qreal fc = SCAST(qreal, m_spectrum->getCenterFreq());
    qreal fs = SCAST(qreal, m_analyzer->getSampleRate());

    ui->frequencySpin->setMinimum(fc - .5 * fs);
    ui->frequencySpin->setMaximum(fc + .5 * fs);
  }

  onAdjustFrequency();
}

QColor
PhaseComparator::channelColor(bool state) const
{
  if (state)
    return QColor("#00ff00");
  else
    return QColor("#bfbf00");
}

void
PhaseComparator::refreshNamedChannel()
{
  bool shouldHaveNamChan =
         m_analyzer != nullptr
      && m_comparator->isRunning();

  // Check whether we should have a named channel here.
  if (shouldHaveNamChan != m_haveNamChan) { // Inconsistency!
    m_haveNamChan = shouldHaveNamChan;

    // Make sure we have a named channel
    if (m_haveNamChan) {
      auto chBw   = m_comparator->getTrueBandwidth();
      auto loFreq = m_comparator->getFrequencyLo();
      auto hiFreq = m_comparator->getFrequencyHi();

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
    auto chBw   = m_comparator->getTrueBandwidth();
    auto loFreq = m_comparator->getFrequencyLo();
    auto hiFreq = m_comparator->getFrequencyHi();

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
PhaseComparator::refreshUi()
{
  bool running   = m_comparator->isRunning();
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
PhaseComparator::allocConfig()
{
  return m_panelConfig = new PhaseComparatorConfig();
}

void
PhaseComparator::applyConfig()
{
  setProperty("collapsed", m_panelConfig->collapsed);

  refreshUi();
}

bool
PhaseComparator::event(QEvent *event)
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
PhaseComparator::setState(int, Suscan::Analyzer *analyzer)
{
  m_analyzer = analyzer;
  m_comparator->setAnalyzer(analyzer);


  if (analyzer != nullptr) {
    auto windowSize = m_mediator->getAnalyzerParams()->windowSize;
    m_comparator->setFFTSizeHint(windowSize);
    applySpectrumState();
  }

  refreshNamedChannel();
  refreshUi();
}

void
PhaseComparator::openPlot()
{
  auto sus = Suscan::Singleton::get_instance();
  auto factory = sus->findTabWidgetFactory("PhasePlotPage");

  m_plotPage = SCAST(PhasePlotPage *, factory->make(m_mediator));
  m_plotPage->setProperties(
        this,
        .5 * (m_comparator->getFrequencyHi() + m_comparator->getFrequencyLo()),
        m_comparator->getEquivFs());
  m_plotPage->setColorConfig(m_colors);
  connect(
        m_plotPage,
        SIGNAL(closeReq()),
        this,
        SLOT(onClosePlotPage()));

  m_mediator->addTabWidget(m_plotPage);
}

void
PhaseComparator::setQth(Suscan::Location const &)
{

}

void
PhaseComparator::setColorConfig(ColorConfig const &colors)
{
  m_colors = colors;
}

void
PhaseComparator::setTimeStamp(struct timeval const &)
{

}

void
PhaseComparator::setProfile(Suscan::Source::Config &)
{

}


////////////////////////////// Slots ///////////////////////////////////////////
void
PhaseComparator::onOpenChannel()
{
  auto bandwidth  = m_spectrum->getBandwidth();
  auto loFreq     = m_spectrum->getLoFreq();
  auto centerFreq = m_spectrum->getCenterFreq();
  auto freq       = centerFreq + loFreq;

  BLOCKSIG(ui->bandwidthSpin, setValue(bandwidth));
  BLOCKSIG(ui->frequencySpin, setValue(freq));

  auto result = m_comparator->open(freq, bandwidth);

  if (!result) {
    QMessageBox::critical(
          this,
          "Cannot open inspector",
          "Failed to open phase comparator. See log window for details");
  }
}

void
PhaseComparator::onCloseChannel()
{
  m_comparator->close();
}

void
PhaseComparator::onAdjustFrequency()
{
  m_comparator->setFrequency(ui->frequencySpin->value());
  refreshNamedChannel();
}

void
PhaseComparator::onAdjustBandwidth()
{
  m_comparator->setBandwidth(ui->bandwidthSpin->value());
  refreshNamedChannel();
}

void
PhaseComparator::onSpectrumFrequencyChanged(qint64)
{
  applySpectrumState();
}


void
PhaseComparator::onComparatorOpened()
{
  m_count = 0;

  ui->stateLabel->setText("Comparator opened");
  openPlot();

  refreshUi();
  refreshNamedChannel();
}

void
PhaseComparator::onComparatorClosed()
{
  ui->stateLabel->setText("Comparator closed");
  m_plotPage = nullptr;

  refreshUi();
  refreshNamedChannel();
}

void
PhaseComparator::onComparatorError(QString error)
{
  ui->stateLabel->setText("Comparator error: " + error);
  m_plotPage = nullptr;

  refreshUi();
  refreshNamedChannel();
}

void
PhaseComparator::onComparatorData()
{
  auto data = m_comparator->data();

  if (m_plotPage != nullptr)
    m_plotPage->feed(data.data(), data.size());

  ++m_count;
  //ui->stateLabel->setText(QString::number(m_count) + " packets");
}

void
PhaseComparator::onComparatorStateChanged(int ch, int state, QString msg)
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
PhaseComparator::onClosePlotPage()
{
  PhasePlotPage *sender = SCAST(PhasePlotPage *, QObject::sender());

  if (sender == m_plotPage) {
    m_plotPage = nullptr;
    m_comparator->close();
  }

  sender->deleteLater();
}
