//
//    RawChannelForwarder.h: description
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
#ifndef RAW_CHANNEL_FORWARDER_H
#define RAW_CHANNEL_FORWARDER_H

#include <QObject>
#include <Suscan/Library.h>
#include <Suscan/Analyzer.h>

namespace Suscan {
  class Analyzer;
  class AnalyzerRequestTracker;
  struct AnalyzerRequest;
};

namespace SigDigger {
  class UIMediator;

  enum RawChannelForwarderState {
    RAW_CHANNEL_FORWARDER_IDLE,         // Channel closed
    RAW_CHANNEL_FORWARDER_OPENING,      // Have request Id, open() sent
    RAW_CHANNEL_FORWARDER_CONFIGURING,    // Have inspector Id, set_params() sent
    RAW_CHANNEL_FORWARDER_RUNNING,      // set_params ack, starting sample delivery (hold)
  };

  class RawChannelForwarder : public QObject
  {
    Q_OBJECT

    Suscan::Analyzer   *m_analyzer = nullptr;
    Suscan::AnalyzerRequestTracker *m_tracker = nullptr;

    Suscan::Handle      m_inspHandle  = -1;
    uint32_t            m_inspId      = 0xffffffff;
    UIMediator         *m_mediator    = nullptr;
    bool                m_inspectorOpened = false;
    RawChannelForwarderState m_state       = RAW_CHANNEL_FORWARDER_IDLE;
    qreal               m_desiredBandwidth = 0;
    qreal               m_desiredFrequency = 0;

    // These are only set if state > OPENING
    qreal               m_fullSampleRate;
    qreal               m_equivSampleRate;
    unsigned            m_decimation;
    qreal               m_maxBandwidth;
    qreal               m_chanRBW;
    unsigned int        m_fftSize = 8192;

    // These are only set during streaming
    qreal               m_trueBandwidth;
    std::vector<SUCOMPLEX> m_lastBuffer;

    qreal adjustBandwidth(qreal desired) const;
    void disconnectAnalyzer();
    void connectAnalyzer();
    void closeChannel();
    bool openChannel();
    void setState(RawChannelForwarderState, QString const &);

    void connectAll();


  public:
    explicit RawChannelForwarder(UIMediator *, QObject *parent = nullptr);
    virtual ~RawChannelForwarder() override;

    RawChannelForwarderState state() const;
    void  setAnalyzer(Suscan::Analyzer *);
    void  setFFTSizeHint(unsigned int);

    bool  open(SUFREQ, SUFLOAT);
    bool  isRunning() const;
    bool  close();

    qreal setBandwidth(qreal);
    void  setFrequency(qreal);
    qreal getFrequency() const;

    qreal getMinBandwidth() const;
    qreal getMaxBandwidth() const;
    qreal getTrueBandwidth() const;
    qreal getEquivFs() const;
    unsigned getDecimation() const;

    const std::vector<SUCOMPLEX> &data() const;

  public slots:
    void onInspectorMessage(Suscan::InspectorMessage const &);
    void onInspectorSamples(Suscan::SamplesMessage const &);
    void onOpened(Suscan::AnalyzerRequest const &);
    void onCancelled(Suscan::AnalyzerRequest const &);
    void onError(Suscan::AnalyzerRequest const &, std::string const &);

  signals:
    void stateChanged(int, QString const &);
    void dataAvailable();
  };
}

#endif // RAW_CHANNEL_FORWARDER_H
