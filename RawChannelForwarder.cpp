//
//    RawChannelForwarder.cpp: description
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
#include "RawChannelForwarder.h"
#include <UIMediator.h>
#include <SuWidgetsHelpers.h>
#include <Suscan/AnalyzerRequestTracker.h>
#include <SigDiggerHelpers.h>

using namespace SigDigger;

//////////////////////////////// RawChannelForwarder ////////////////////////////
RawChannelForwarder::RawChannelForwarder(UIMediator *mediator, QObject *parent)
  : QObject{parent}
{
  m_mediator = mediator;
  m_tracker = new Suscan::AnalyzerRequestTracker(this);

  this->connectAll();

  this->setState(RAW_CHANNEL_FORWARDER_IDLE, "Idle");
}

RawChannelForwarder::~RawChannelForwarder()
{
}

void
RawChannelForwarder::connectAll()
{
  connect(
        this->m_tracker,
        SIGNAL(opened(Suscan::AnalyzerRequest const &)),
        this,
        SLOT(onOpened(Suscan::AnalyzerRequest const &)));

  connect(
        this->m_tracker,
        SIGNAL(cancelled(Suscan::AnalyzerRequest const &)),
        this,
        SLOT(onCancelled(Suscan::AnalyzerRequest const &)));

  connect(
        this->m_tracker,
        SIGNAL(error(Suscan::AnalyzerRequest const &, const std::string &)),
        this,
        SLOT(onError(Suscan::AnalyzerRequest const &, const std::string &)));
}

qreal
RawChannelForwarder::adjustBandwidth(qreal desired) const
{
  if (m_decimation == 0)
    return desired;

  return m_chanRBW * ceil(desired / m_chanRBW);
}

void
RawChannelForwarder::disconnectAnalyzer()
{
  disconnect(m_analyzer, nullptr, this, nullptr);

  this->setState(RAW_CHANNEL_FORWARDER_IDLE, "Analyzer closed");
}

void
RawChannelForwarder::connectAnalyzer()
{
  connect(
        m_analyzer,
        SIGNAL(inspector_message(const Suscan::InspectorMessage &)),
        this,
        SLOT(onInspectorMessage(const Suscan::InspectorMessage &)));

  connect(
        m_analyzer,
        SIGNAL(samples_message(const Suscan::SamplesMessage &)),
        this,
        SLOT(onInspectorSamples(const Suscan::SamplesMessage &)));
}

void
RawChannelForwarder::closeChannel()
{
  if (m_analyzer != nullptr && m_inspHandle != -1)
    m_analyzer->closeInspector(m_inspHandle);

  m_inspHandle = -1;
}

void
RawChannelForwarder::setFFTSizeHint(unsigned int fftSize)
{
  m_fftSize = fftSize;
}


// Depending on the state, a few things must be initialized
void
RawChannelForwarder::setState(RawChannelForwarderState state, QString const &msg)
{
  QStringList correctedList;

  if (m_state != state) {
    m_state = state;

    switch (state) {
      case RAW_CHANNEL_FORWARDER_IDLE:
        if (m_inspHandle != -1)
          this->closeChannel();

        m_inspId = 0xffffffff;
        m_equivSampleRate = 0;
        m_fullSampleRate = 0;
        m_decimation = 0;
        m_chanRBW = 0;
        break;

      case RAW_CHANNEL_FORWARDER_CONFIGURING:
        break;

      case RAW_CHANNEL_FORWARDER_RUNNING:
        break;

      default:
        break;
    }

    emit stateChanged(state, msg);
  }
}

bool
RawChannelForwarder::openChannel()
{
  Suscan::Channel ch;

  ch.bw    = m_desiredBandwidth;
  ch.fc    = m_desiredFrequency;
  ch.fLow  = -.5 * m_desiredBandwidth;
  ch.fHigh = +.5 * m_desiredBandwidth;

  if (!m_tracker->requestOpen("raw", ch, QVariant(), false))
    return false;

  this->setState(RAW_CHANNEL_FORWARDER_OPENING, "Opening inspector...");

  return true;
}

///////////////////////////////// Public API //////////////////////////////////
RawChannelForwarderState
RawChannelForwarder::state() const
{
  return m_state;
}

void
RawChannelForwarder::setAnalyzer(Suscan::Analyzer *analyzer)
{
  if (m_analyzer != nullptr)
    this->disconnectAnalyzer();

  m_analyzer = nullptr;
  if (analyzer == nullptr)
    this->setState(RAW_CHANNEL_FORWARDER_IDLE, "Capture stopped");
  else
    this->setState(RAW_CHANNEL_FORWARDER_IDLE, "Analyzer changed");

  m_analyzer = analyzer;

  if (m_analyzer != nullptr)
    connectAnalyzer();

  m_tracker->setAnalyzer(analyzer);
}

bool
RawChannelForwarder::isRunning() const
{
  return m_state != RAW_CHANNEL_FORWARDER_IDLE;
}

bool
RawChannelForwarder::close()
{
  if (isRunning()) {
    if (m_state == RAW_CHANNEL_FORWARDER_OPENING)
      m_tracker->cancelAll();
    this->setState(RAW_CHANNEL_FORWARDER_IDLE, "Closed by user");

    return true;
  }

  return false;
}

qreal
RawChannelForwarder::getMaxBandwidth() const
{
  return m_maxBandwidth;
}

qreal
RawChannelForwarder::getMinBandwidth() const
{
  return m_chanRBW;
}

qreal
RawChannelForwarder::getTrueBandwidth() const
{
  return m_trueBandwidth;
}

qreal
RawChannelForwarder::setBandwidth(qreal desired)
{
  qreal ret;
  m_desiredBandwidth = desired;

  if (m_state > RAW_CHANNEL_FORWARDER_OPENING) {
    m_trueBandwidth = adjustBandwidth(m_desiredBandwidth);
    m_analyzer->setInspectorBandwidth(m_inspHandle, m_trueBandwidth);
    ret = m_trueBandwidth;
  } else {
    ret = desired;
  }

  return ret;
}

void
RawChannelForwarder::setFrequency(qreal f_off)
{
  m_desiredFrequency = f_off;
  if (m_state > RAW_CHANNEL_FORWARDER_OPENING) {
    m_analyzer->setInspectorFreq(m_inspHandle, m_desiredFrequency);
  }
}

qreal
RawChannelForwarder::getFrequency() const
{
  return m_desiredFrequency;
}

unsigned
RawChannelForwarder::getDecimation() const
{
  return m_decimation;
}

qreal
RawChannelForwarder::getEquivFs() const
{
  if (m_state > RAW_CHANNEL_FORWARDER_OPENING)
    return m_equivSampleRate;
  else
    return 0;
}

bool
RawChannelForwarder::open(SUFREQ f_off, SUFLOAT bw)
{
  if (this->isRunning())
    return false;

  this->setFrequency(f_off);
  this->setBandwidth(SCAST(qreal, bw));

  return openChannel();
}

///////////////////////////// Analyzer slots //////////////////////////////////
void
RawChannelForwarder::onInspectorMessage(Suscan::InspectorMessage const &msg)
{
  if (msg.getInspectorId() == m_inspId) {
    // This refers to us!

    switch (msg.getKind()) {
      case SUSCAN_ANALYZER_INSPECTOR_MSGKIND_SET_BANDWIDTH:
        if (state() != RAW_CHANNEL_FORWARDER_RUNNING)
          this->setState(RAW_CHANNEL_FORWARDER_RUNNING, "Inspector running");
        break;

      case SUSCAN_ANALYZER_INSPECTOR_MSGKIND_CLOSE:
        m_inspHandle = -1;
        this->setState(RAW_CHANNEL_FORWARDER_IDLE, "Inspector closed");
        break;

      case SUSCAN_ANALYZER_INSPECTOR_MSGKIND_WRONG_KIND:
      case SUSCAN_ANALYZER_INSPECTOR_MSGKIND_WRONG_OBJECT:
      case SUSCAN_ANALYZER_INSPECTOR_MSGKIND_WRONG_HANDLE:
        this->setState(RAW_CHANNEL_FORWARDER_IDLE, "Error during channel opening");
        break;

      case SUSCAN_ANALYZER_INSPECTOR_MSGKIND_SET_TLE:
        break;

      case SUSCAN_ANALYZER_INSPECTOR_MSGKIND_ORBIT_REPORT:
        break;

      default:
        break;
    }
  }
}

void
RawChannelForwarder::onInspectorSamples(Suscan::SamplesMessage const &msg)
{
  // Feed samples, only if the sample rate is right
  if (msg.getInspectorId() == m_inspId) {
    const SUCOMPLEX *samples = msg.getSamples();
    unsigned int count = msg.getCount();

    if (m_state == RAW_CHANNEL_FORWARDER_RUNNING) {
      m_lastBuffer.resize(count);
      m_lastBuffer.assign(samples, samples + count);

      emit dataAvailable();
    }
  }
}

const std::vector<SUCOMPLEX> &
RawChannelForwarder::data() const
{
  return m_lastBuffer;
}

////////////////////////////// RawChannelor slots ////////////////////////////////
void
RawChannelForwarder::onOpened(Suscan::AnalyzerRequest const &req)
{
  // Async step 2: update state
  if (m_analyzer != nullptr) {
    // Async step 3: set parameters
    m_inspHandle      = req.handle;
    m_inspId          = req.inspectorId;
    m_fullSampleRate  = SCAST(qreal, req.basebandRate);
    m_equivSampleRate = SCAST(qreal, req.equivRate);
    m_decimation      = SCAST(unsigned, m_fullSampleRate / m_equivSampleRate);

    m_maxBandwidth    = m_equivSampleRate;
    m_chanRBW         = m_fullSampleRate / m_fftSize;

    m_trueBandwidth   = adjustBandwidth(m_desiredBandwidth);

    // Adjust bandwidth to something that is physical and determined by the FFT
    m_analyzer->setInspectorBandwidth(m_inspHandle, m_trueBandwidth);


    // We now transition to LAUNCHING and wait for the RawChannel initialization
    this->setState(RAW_CHANNEL_FORWARDER_RUNNING, "Channel running...");
  }
}

void
RawChannelForwarder::onCancelled(Suscan::AnalyzerRequest const &)
{
  this->setState(RAW_CHANNEL_FORWARDER_IDLE, "Closed");
}

void
RawChannelForwarder::onError(Suscan::AnalyzerRequest const &, std::string const &err)
{
  this->setState(
        RAW_CHANNEL_FORWARDER_IDLE,
        "Failed to open inspector: " + QString::fromStdString(err));
}
