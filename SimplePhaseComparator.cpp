//
//    SimplePhaseComparator.cpp: description
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
#include "SimplePhaseComparator.h"

using namespace SigDigger;

SimplePhaseComparator::SimplePhaseComparator(
    UIMediator *mediator, QObject *parent) : QObject(parent)
{
  m_mediator = mediator;

  m_forwarder_hi = new RawChannelForwarder(mediator, this);
  m_forwarder_lo = new RawChannelForwarder(mediator, this);

  connectAll();
}

SimplePhaseComparator::~SimplePhaseComparator()
{

}

void
SimplePhaseComparator::connectAll()
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
SimplePhaseComparator::setAnalyzer(Suscan::Analyzer *analyzer)
{
  m_analyzer = analyzer;

  m_forwarder_lo->setAnalyzer(analyzer);
  m_forwarder_hi->setAnalyzer(analyzer);
}

void
SimplePhaseComparator::setFFTSizeHint(unsigned int fftSize)
{
  m_forwarder_lo->setFFTSizeHint(fftSize);
  m_forwarder_hi->setFFTSizeHint(fftSize);
}

bool
SimplePhaseComparator::calcOffsetFrequencies(
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
SimplePhaseComparator::open(SUFREQ freq, SUFLOAT bandwidth)
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
SimplePhaseComparator::isRunning() const
{
  return m_forwarder_lo->isRunning() || m_forwarder_hi->isRunning();
}

bool
SimplePhaseComparator::close()
{
  if (m_analyzer == nullptr)
    return false;

  m_forwarder_lo->close();
  m_forwarder_hi->close();

  return true;
}

qreal
SimplePhaseComparator::setBandwidth(qreal bandwidth)
{
  qreal ret1, ret2;

  m_desiredBandwidth = bandwidth;

  ret1 = m_forwarder_lo->setBandwidth(bandwidth);
  ret2 = m_forwarder_hi->setBandwidth(bandwidth);

  return .5 * (ret1 + ret2);
}

void
SimplePhaseComparator::setFrequency(qreal freq)
{
  qreal off1, off2;

  m_desiredFrequency = freq;

  if (!calcOffsetFrequencies(freq, off1, off2))
    return;

  m_forwarder_lo->setFrequency(off1);
  m_forwarder_hi->setFrequency(off2);
}

qreal
SimplePhaseComparator::getFrequencyLo() const
{
  if (m_analyzer == nullptr)
    return 0;

  return m_forwarder_lo->getFrequency() + m_analyzer->getFrequency();
}

qreal
SimplePhaseComparator::getFrequencyHi() const
{
  if (m_analyzer == nullptr)
    return 0;

  return m_forwarder_hi->getFrequency() + m_analyzer->getFrequency();
}

qreal
SimplePhaseComparator::getMinBandwidth() const
{
  return fmax(
        m_forwarder_lo->getMinBandwidth(),
        m_forwarder_hi->getMinBandwidth());
}

qreal
SimplePhaseComparator::getMaxBandwidth() const
{
  return fmin(
        m_forwarder_lo->getMaxBandwidth(),
        m_forwarder_hi->getMaxBandwidth());
}

qreal
SimplePhaseComparator::getTrueBandwidth() const
{
  return .5 * (m_forwarder_lo->getTrueBandwidth() + m_forwarder_hi->getTrueBandwidth());
}

qreal
SimplePhaseComparator::getEquivFs() const
{
  if (m_forwarder_lo->isRunning())
    return m_forwarder_lo->getEquivFs();
  else if (m_forwarder_hi->isRunning())
    return m_forwarder_hi->getEquivFs();

  return 0;
}

unsigned
SimplePhaseComparator::getDecimation() const
{
  if (m_forwarder_lo->isRunning())
    return m_forwarder_lo->getDecimation();
  else if (m_forwarder_hi->isRunning())
    return m_forwarder_hi->getDecimation();

  return 1;
}

const std::vector<SUCOMPLEX> &
SimplePhaseComparator::data() const
{
  return m_lastBuffer;
}

///////////////////////////////// Slots ////////////////////////////////////////
void
SimplePhaseComparator::onStateChanged(int state, QString const &message)
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
SimplePhaseComparator::onDataAvailable()
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

      m_lastBuffer.resize(bufLo.size());

      for (size_t i = 0; i < bufLo.size(); ++i)
        m_lastBuffer[i] = bufLo[i] * SU_C_CONJ(bufHi[i]);
      m_loAvail = m_hiAvail = false;

      emit dataAvailable();
    }
  }
}

