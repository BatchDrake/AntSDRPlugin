//
//    AD9361SourcePage.cpp: description
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
#include "AD9361SourcePage.h"
#include <SuWidgetsHelpers.h>
#include <suscan/util/cfg.h>
#include "ui_AD9361SourcePage.h"

AD9361SourcePage::AD9361SourcePage(
    SigDigger::SourceConfigWidgetFactory *factory,
    QWidget *parent) : SigDigger::SourceConfigWidget(factory, parent)
{
  ui = new Ui::AD9361SourcePage();

  ui->setupUi(this);

  connectAll();
}

AD9361SourcePage::~AD9361SourcePage()
{
  delete ui;
}

void
AD9361SourcePage::connectAll()
{
  connect(
        ui->uriEdit,
        SIGNAL(textEdited(QString)),
        this,
        SLOT(onConfigChanged()));
}

uint64_t
AD9361SourcePage::getCapabilityMask() const
{
  uint64_t perms = SUSCAN_ANALYZER_ALL_SDR_PERMISSIONS;

  perms &= ~SUSCAN_ANALYZER_PERM_SET_AGC;
  perms &= ~SUSCAN_ANALYZER_PERM_SET_ANTENNA;
  perms &= ~SUSCAN_ANALYZER_PERM_SET_DC_REMOVE;

  return perms;
}

bool
AD9361SourcePage::getPreferredRates(QList<int> &) const
{
  return false;
}

void
AD9361SourcePage::refreshUi()
{
  if (m_config == nullptr)
    return;

  // Set format
  auto uri = m_config->getParam("uri");
  QString strUri = QString::fromStdString(uri);

  if (strUri.size() == 0)
    strUri = "ip:192.168.1.10";

  BLOCKSIG(ui->uriEdit, setText(strUri));
}

void
AD9361SourcePage::activateWidget()
{
  refreshUi();
  emit changed();
}

bool
AD9361SourcePage::deactivateWidget()
{
  return true;
}

void
AD9361SourcePage::notifySingletonChanges()
{
}

void
AD9361SourcePage::setConfigRef(Suscan::Source::Config &cfg)
{
  m_config = &cfg;
  refreshUi();
}


///////////////////////////////////// Slots ////////////////////////////////////
void
AD9361SourcePage::onConfigChanged()
{
  if (m_config == nullptr)
    return;

  m_config->setParam("uri", ui->uriEdit->text().toStdString());

  emit changed();
}
