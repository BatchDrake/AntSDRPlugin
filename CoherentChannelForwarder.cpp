//
//    CoherentChannelForwarder.cpp: description
//    Copyright (C) 2024 Gonzalo José Carracedo Carballal
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
#include "CoherentChannelForwarder.h"

using namespace SigDigger;

CoherentChannelForwarder::CoherentChannelForwarder(
    UIMediator *mediator, QObject *parent) : QObject(parent)
{
  m_mediator = mediator;

  m_forwarder_hi = new RawChannelForwarder(mediator, this);
  m_forwarder_lo = new RawChannelForwarder(mediator, this);

  connectAll();
}

CoherentChannelForwarder::~CoherentChannelForwarder()
{

}

void
CoherentChannelForwarder::connectAll()
{
  connect(
        m_forwarder_hi,
        SIGNAL(dataAvailable()),
        this,
        SLOT(onDataAvailable()));

  connect(
        m_forwarder_lo,
        SIGNAL(dataAvailable()),
        this,
        SLOT(onDataAvailable()));

  connect(
        m_forwarder_hi,
        SIGNAL(stateChanged(int,QString)),
        this,
        SLOT(onStateChanged(int,QString)));

  connect(
        m_forwarder_lo,
        SIGNAL(stateChanged(int,QString)),
        this,
        SLOT(onStateChanged(int,QString)));
}

void
CoherentChannelForwarder::setAnalyzer(Suscan::Analyzer *analyzer)
{
  m_analyzer = analyzer;

  m_forwarder_lo->setAnalyzer(analyzer);
  m_forwarder_hi->setAnalyzer(analyzer);
}

void
CoherentChannelForwarder::setFFTSizeHint(unsigned int fftSize)
{
  m_forwarder_lo->setFFTSizeHint(fftSize);
  m_forwarder_hi->setFFTSizeHint(fftSize);
}

bool
CoherentChannelForwarder::calcOffsetFrequencies(
    qreal freq, qreal &off1, qreal &off2)
{
  qreal delta;

  if (m_analyzer == nullptr)
    return false;

  delta = .5 * SCAST(qreal, m_analyzer->getSampleRate());

  off1 = freq - m_analyzer->getFrequency();
  if (off1 > 0)
    off1 -= delta;
  off2 = off1 + delta;

  return true;
}

bool
CoherentChannelForwarder::open(SUFREQ freq, SUFLOAT bandwidth)
{
  qreal off1, off2;

  if (!calcOffsetFrequencies(freq, off1, off2))
    return false;

  if (isRunning())
    return false;

  m_desiredFrequency = freq;
  m_desiredBandwidth = bandwidth;

  m_forwarder_lo->open(off1, bandwidth);
  m_forwarder_hi->open(off2, bandwidth);

  return true;
}

bool
CoherentChannelForwarder::isRunning() const
{
  return m_forwarder_lo->isRunning() || m_forwarder_hi->isRunning();
}

bool
CoherentChannelForwarder::close()
{
  if (m_analyzer == nullptr)
    return false;

  m_forwarder_lo->close();
  m_forwarder_hi->close();

  return true;
}

qreal
CoherentChannelForwarder::setBandwidth(qreal bandwidth)
{
  qreal ret1, ret2;

  m_desiredBandwidth = bandwidth;

  ret1 = m_forwarder_lo->setBandwidth(bandwidth);
  ret2 = m_forwarder_hi->setBandwidth(bandwidth);

  return .5 * (ret1 + ret2);
}

void
CoherentChannelForwarder::setFrequency(qreal freq)
{
  qreal off1, off2;

  m_desiredFrequency = freq;

  if (!calcOffsetFrequencies(freq, off1, off2))
    return;

  m_forwarder_lo->setFrequency(off1);
  m_forwarder_hi->setFrequency(off2);
}

qreal
CoherentChannelForwarder::getFrequency() const
{
  return m_desiredFrequency;
}

qreal
CoherentChannelForwarder::getFrequencyLo() const
{
  if (m_analyzer == nullptr)
    return 0;

  return m_forwarder_lo->getFrequency() + m_analyzer->getFrequency();
}

qreal
CoherentChannelForwarder::getFrequencyHi() const
{
  if (m_analyzer == nullptr)
    return 0;

  return m_forwarder_hi->getFrequency() + m_analyzer->getFrequency();
}

qreal
CoherentChannelForwarder::getMinBandwidth() const
{
  return fmax(
        m_forwarder_lo->getMinBandwidth(),
        m_forwarder_hi->getMinBandwidth());
}

qreal
CoherentChannelForwarder::getMaxBandwidth() const
{
  return fmin(
        m_forwarder_lo->getMaxBandwidth(),
        m_forwarder_hi->getMaxBandwidth());
}

qreal
CoherentChannelForwarder::getTrueBandwidth() const
{
  return .5 * (m_forwarder_lo->getTrueBandwidth() + m_forwarder_hi->getTrueBandwidth());
}

qreal
CoherentChannelForwarder::getEquivFs() const
{
  if (m_forwarder_lo->isRunning())
    return m_forwarder_lo->getEquivFs();
  else if (m_forwarder_hi->isRunning())
    return m_forwarder_hi->getEquivFs();

  return 0;
}

unsigned
CoherentChannelForwarder::getDecimation() const
{
  if (m_forwarder_lo->isRunning())
    return m_forwarder_lo->getDecimation();
  else if (m_forwarder_hi->isRunning())
    return m_forwarder_hi->getDecimation();

  return 1;
}

const std::vector<SUCOMPLEX> &
CoherentChannelForwarder::hiData() const
{
  return m_lastHiBuffer;
}

const std::vector<SUCOMPLEX> &
CoherentChannelForwarder::loData() const
{
  return m_lastLoBuffer;
}
///////////////////////////////// Slots ////////////////////////////////////////
void
CoherentChannelForwarder::onStateChanged(int state, QString const &message)
{
  RawChannelForwarder *sender = SCAST(RawChannelForwarder *, QObject::sender());

  emit stateChanged(sender == m_forwarder_hi, state, message);

  if (state == RAW_CHANNEL_FORWARDER_IDLE) {
    if (message.startsWith("Failed"))
      emit error(message);
    else if (!isRunning())
      emit closed();

    m_forwarder_hi->close();
    m_forwarder_lo->close();

    m_loRunning = m_hiRunning = false;
    m_loAvail   = m_hiAvail   = false;
  } else if (state == RAW_CHANNEL_FORWARDER_RUNNING) {
    if (sender == m_forwarder_lo)
      m_loRunning = true;
    else if (sender == m_forwarder_hi)
      m_hiRunning = true;

    if (m_loRunning && m_hiRunning)
      emit opened();
  }
}

void
CoherentChannelForwarder::onDataAvailable()
{
  RawChannelForwarder *sender = SCAST(RawChannelForwarder *, QObject::sender());

  if (m_loRunning && m_hiRunning) {
    if (sender == m_forwarder_lo)
      m_loAvail = true;
    else if (sender == m_forwarder_hi)
      m_hiAvail = true;

    if (m_loAvail && m_hiAvail) {
      auto bufLo = m_forwarder_lo->data();
      auto bufHi = m_forwarder_hi->data();

      if (bufLo.size() != bufHi.size()) {
        emit error("Synchronous buffer have different sizes\n");
        close();
        return;
      }

      m_lastHiBuffer = bufHi;
      m_lastLoBuffer = bufLo;

      m_loAvail = m_hiAvail = false;

      emit dataAvailable();
    }
  }
}

