//
//    PhaseComparatorFactory.h: description
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
#ifndef PHASECOMPARATORFACTORY_H
#define PHASECOMPARATORFACTORY_H

#include <ToolWidgetFactory.h>

namespace SigDigger {
  class PhaseComparatorFactory : public ToolWidgetFactory
  {
  public:
    // FeatureFactory overrides
    const char *name() const override;

    // ToolWidgetFactory overrides
    ToolWidget *make(UIMediator *) override;
    std::string getTitle() const override;

    PhaseComparatorFactory(Suscan::Plugin *);
  };
}

#endif // PHASECOMPARATORFACTORY_H
