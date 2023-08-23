//
//    CoherentDetector.h: Detects signals based on their phase coherence
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

#ifndef COHERENTDETECTOR_H
#define COHERENTDETECTOR_H

#include <sigutils/types.h>
#include <vector>

//
// The coherent detector works as follows:
// 1. Constantly demodulate the signal in FM, using arg(x[n] * conj(x[n-1]))
//    This has units of angle per sample. If we square this up, we end up
//    having square angle per sample. We will accumulate this magnitude.
// 2. After m_size samples, divide accumulator by m_size. This is a MSE of
//    the phase coherence.
// 3. Compare this division against the threshold.
//

namespace SigDigger {
  struct CoherentEvent {
    struct timeval timeStamp;
    SUSCOUNT length;
    SUFLOAT  meanPhase;
    SUFLOAT  meanPower;
    SUFLOAT  aoa[2];
  };

  class CoherentDetector
  {
    SUSCOUNT  m_holdMax    = 0;
    SUSCOUNT  m_holdCntr   = 0;
    SUCOMPLEX m_prev       = 1;
    SUSCOUNT  m_size       = 0;
    SUFLOAT   m_alpha      = 0;
    SUFLOAT   m_threshold2 = 0;
    SUFLOAT   m_detSig     = M_PI * M_PI;
    SUFLOAT   m_dipPhase   = M_PI;

    bool      m_triggered = false;
    SUSCOUNT  m_count     = 0;
    SUCOMPLEX m_iqAcc     = 0;
    SUFLOAT   m_pwrAcc    = 0;
    bool      m_haveEvent = false;

    CoherentEvent m_lastEvent;

  public:
    CoherentDetector();

    bool enabled() const;
    void reset();
    void resize(size_t);
    void setDipolePhase(SUFLOAT);
    void setHoldMax(size_t);
    void setThreshold(float); // In radians, always

    size_t feed(const SUCOMPLEX *, size_t);
    bool  triggered() const;
    CoherentEvent lastEvent();
    bool  haveEvent() const;
  };
}

#endif // COHERENTDETECTOR_H
