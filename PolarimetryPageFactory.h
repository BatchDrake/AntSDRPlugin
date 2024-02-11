//
//    PolarimetryPageFactory.h: description
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
#ifndef PolarimetryPageFACTORY_H
#define PolarimetryPageFACTORY_H

#include <TabWidgetFactory.h>

namespace SigDigger{
  class PolarimetryPageFactory : public TabWidgetFactory
  {
  public:
    PolarimetryPageFactory(Suscan::Plugin *);
    virtual ~PolarimetryPageFactory() override;

    virtual const char *name() const override;
    virtual TabWidget *make(UIMediator *) override;

  };
}

#endif // PolarimetryPageFACTORY_H
