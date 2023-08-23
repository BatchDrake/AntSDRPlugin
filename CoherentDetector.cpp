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
#include <sys/time.h>

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
  m_count  = 0;
  m_iqAcc  = 0;
  m_pwrAcc = 0;
  m_prev   = 1;
}

void
CoherentDetector::resize(size_t size = 0)
{
  m_size = size;
  m_alpha = SU_SPLPF_ALPHA(size);
}

void
CoherentDetector::setThreshold(float threshold)
{
  m_threshold2 = threshold * threshold;
}

void
CoherentDetector::setDipolePhase(SUFLOAT phase)
{
  m_dipPhase = phase;
}

void
CoherentDetector::setHoldMax(size_t hold)
{
  m_holdMax = hold;
}

size_t
CoherentDetector::feed(const SUCOMPLEX *data, size_t size)
{
  size_t i;

  SUCOMPLEX iq;
  SUCOMPLEX prev = m_prev;
  SUFLOAT   detSig = m_detSig;
  bool      signaled;
  SUFLOAT   angle;
  bool      searching = true;

  for (i = 0; i < size && searching; ++i) {
    iq = data[i];

    angle = SU_C_ARG(iq * SU_C_CONJ(prev));
    if (isinf(angle) || isnan(angle))
      angle = 0;

    SU_SPLPF_FEED(detSig, angle * angle, m_alpha);
    signaled = detSig < m_threshold2;

    if (m_triggered) {
      // Triggered! Accumulate signal data
      m_iqAcc  += iq;
      m_pwrAcc += SU_C_REAL(iq * SU_C_CONJ(iq));
      ++m_count;

      if (!signaled) {
        if (++m_holdCntr > m_holdMax) {
          // EVENT!
          m_lastEvent.length    = m_count - m_holdCntr - 1;
          m_lastEvent.meanPhase = SU_C_ARG(m_iqAcc);
          m_lastEvent.meanPower = m_pwrAcc / m_count;

          m_lastEvent.aoa[0]    = SU_ASIN(m_lastEvent.meanPhase / m_dipPhase);
          m_lastEvent.aoa[1]    = M_PI - m_lastEvent.aoa[0];
          m_haveEvent = true;

          m_triggered = false;
          searching   = false;
        }
      } else {
        // Signal present: reset hold counter
        m_holdCntr = 0;
      }
    } else {
      // Non triggered
      if (signaled) {
        // But triggered now
        gettimeofday(&m_lastEvent.timeStamp, nullptr);
        m_iqAcc     = iq;
        m_count     = 1;
        m_pwrAcc    = SU_C_REAL(iq * SU_C_CONJ(iq));
        m_holdCntr  = 0;
        m_triggered = true;
        searching   = false;
      }
    }

    prev = iq;
  }

  m_prev      = prev;
  m_detSig    = detSig;

  return i;
}

CoherentEvent
CoherentDetector::lastEvent()
{
  return m_lastEvent;
}


bool
CoherentDetector::triggered() const
{
  return m_triggered;
}

bool
CoherentDetector::haveEvent() const
{
  return m_haveEvent;
}
