//
//    CoherentDetector.cpp: Detects signals based on their phase coherence
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

#include "CoherentDetector.h"

using namespace SigDigger;

CoherentDetector::CoherentDetector()
{

}

bool
CoherentDetector::enabled() const
{
  return m_size > 0;
}

void
CoherentDetector::reset()
{
  m_triggered = false;
  m_count = 0;
  m_angDeltaAcc = 0.;
}

void
CoherentDetector::resize(size_t size = 0)
{
  m_size = size;
}

void
CoherentDetector::setThreshold(float threshold)
{
  m_threshold2 = threshold * threshold;
}

size_t
CoherentDetector::feed(const SUCOMPLEX *data, size_t size)
{
  size_t avail, count;
  bool triggered;
  SUFLOAT ang, accum, power;
  SUCOMPLEX prev;
  SUCOMPLEX iqAcc;

  count = m_count;
  triggered = m_triggered;
  if (count >= m_size)
    avail = 0;
  else
    avail = m_size - count;

  if (size > avail)
    size = avail;

  prev  = m_prev;
  accum = m_angDeltaAcc;
  power = m_powerAcc;
  iqAcc = m_iqAcc;

  // Demodulate
  for (size_t i = 0; i < size; ++i) {
    power += SU_C_REAL(data[i] * SU_C_CONJ(data[i]));
    ang    = SU_C_ARG(data[i] * SU_C_CONJ(prev));
    accum += ang * ang;
    prev   = data[i];
    iqAcc += data[i];
  }

  m_prev        = prev;

  avail -= size;
  count += size;

  if (triggered) {
    m_powerCount += size;
    m_powerAcc    = power;
    m_iqAcc       = iqAcc;
  }

  // Detect
  if (avail == 0) {
    accum /= count;

    if (triggered) {
      if (accum > 4 * m_threshold2) {
        if (m_powerCount > 0) {
          m_lastPower = m_powerAcc / m_powerCount;
          m_lastPhase = SU_C_ARG(m_iqAcc);
          m_haveEvent = true;
        }

        m_powerAcc   = 0;
        m_powerCount = 0;
        m_iqAcc      = 0;
        m_triggered  = false;
      }
    } else {
      if (accum <= m_threshold2) {
        m_triggered  = true;
        m_powerAcc   = power;
        m_powerCount = size;
        m_haveEvent  = false;
        m_iqAcc      = iqAcc;
      }
    }
    accum = 0;
    count = 0;
  }

  m_angDeltaAcc = accum;
  m_count       = count;

  return size;
}

bool
CoherentDetector::triggered() const
{
  return m_triggered;
}

SUFLOAT
CoherentDetector::lastPower() const
{
  return m_lastPower;
}

SUFLOAT
CoherentDetector::lastPhase() const
{
  return m_lastPhase;
}

bool
CoherentDetector::haveEvent() const
{
  return m_haveEvent;
}
