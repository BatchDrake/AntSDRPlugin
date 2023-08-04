//
//    PhasePlotPage.h: description
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
#ifndef PHASEPLOTPAGE_H
#define PHASEPLOTPAGE_H

#include <TabWidgetFactory.h>

namespace Ui {
  class PhasePlotPage;
}

namespace SigDigger {
  class PhaseComparator;

  class PhasePlotPageConfig : public Suscan::Serializable {
  public:
    // Overriden methods
    void deserialize(Suscan::Object const &conf) override;
    Suscan::Object &&serialize() override;
  };

  class PhasePlotPage : public TabWidget
  {
    Q_OBJECT

    bool m_paramsSet              = false;
    PhaseComparator *m_owner      = nullptr;
    PhasePlotPageConfig *m_config = nullptr;

    std::vector<SUCOMPLEX> m_data;
    SUCOMPLEX m_accumulated;
    SUSCOUNT  m_accumCount = 0;
    SUFLOAT   m_max = 0;

  public:
    explicit PhasePlotPage(
        TabWidgetFactory *,
        UIMediator *,
        QWidget *parent = nullptr);

    ~PhasePlotPage();

    void feed(const SUCOMPLEX *, SUSCOUNT);
    void setProperties(PhaseComparator *, SUFREQ frequency, SUFLOAT sampRate);

    virtual std::string getLabel() const override;
    virtual void closeRequested() override;
    virtual void setColorConfig(ColorConfig const &) override;

    virtual Suscan::Serializable *allocConfig(void) override;
    virtual void applyConfig(void) override;
    virtual void setTimeStamp(struct timeval const &) override;

  private:
    Ui::PhasePlotPage *ui;

  signals:
    void closeReq();
  };

}

#endif // PHASEPLOTPAGE_H
