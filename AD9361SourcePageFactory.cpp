//
//    AD9361SourcePageFactory.cpp: description
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

#include "AD9361SourcePageFactory.h"
#include "AD9361SourcePage.h"

const char *
AD9361SourcePageFactory::name() const
{
  return "ad9361";
}

SigDigger::SourceConfigWidget *
AD9361SourcePageFactory::make()
{
  return new AD9361SourcePage(this);
}
