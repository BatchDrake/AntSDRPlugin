//
//    SimplePhaseComparator.h: description
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
#ifndef SIMPLEPHASECOMPARATOR_H
#define SIMPLEPHASECOMPARATOR_H

#include <QObject>
#include "RawChannelForwarder.h"

namespace SigDigger {
  class SimplePhaseComparator : public QObject
  {
    Q_OBJECT

    Suscan::Analyzer    *m_analyzer = nullptr;
    UIMediator          *m_mediator    = nullptr;

    RawChannelForwarder *m_forwarder_lo = nullptr;
    RawChannelForwarder *m_forwarder_hi = nullptr;

    qreal               m_desiredBandwidth = 0;
    qreal               m_desiredFrequency = 0;
    std::vector<SUCOMPLEX> m_lastBuffer;

    bool m_hiRunning = false;
    bool m_loRunning = false;

    bool m_hiAvail = false;
    bool m_loAvail = false;

    void connectAll();
    bool calcOffsetFrequencies(qreal freq, qreal &off1, qreal &off2);

  public:
    SimplePhaseComparator(UIMediator *, QObject *parent = nullptr);
    virtual ~SimplePhaseComparator() override;

    const std::vector<SUCOMPLEX> &data() const;

    void  setAnalyzer(Suscan::Analyzer *);
    void  setFFTSizeHint(unsigned int);

    bool  open(SUFREQ, SUFLOAT);
    bool  isRunning() const;
    bool  close();

    qreal setBandwidth(qreal bandwidth);
    void  setFrequency(qreal freq);
    qreal getFrequency() const;

    qreal getFrequencyLo() const;
    qreal getFrequencyHi() const;

    qreal getMinBandwidth() const;
    qreal getMaxBandwidth() const;
    qreal getTrueBandwidth() const;
    qreal getEquivFs() const;
    unsigned getDecimation() const;

  public slots:
    void onStateChanged(int, QString const &);
    void onDataAvailable();

  signals:
    void stateChanged(int, int, QString);
    void error(QString);
    void opened();
    void closed();
    void dataAvailable();
  };
}

#endif // SIMPLEPHASECOMPARATOR_H
