/*

  Copyright (C) 2023 Gonzalo José Carracedo Carballal

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as
  published by the Free Software Foundation, version 3.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this program.  If not, see
  <http://www.gnu.org/licenses/>

*/

#ifndef _AD9361_H
#define _AD9361_H

#include <stdio.h>
#include <iio.h>
#include <sigutils/types.h>
#include <sigutils/ncqo.h>
#include <analyzer/source.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define AD9361_DEFAULT_BUFFER_SIZE 65536

struct suscan_source_ad9361 {
  struct suscan_source_config *config;
  suscan_source_t *source;
  SUSCOUNT total_samples;
  SUSCOUNT sample_size;
  SUFLOAT  samp_rate;

  SUBOOL   started;
  SUBOOL   running;
  int      nco_ndx;

  struct iio_context *context;
  struct iio_device  *rx_dev;
  struct iio_device  *phy_dev;
  struct iio_buffer  *rx_buf;

  struct iio_channel *phy_rx0, *phy_rx1;
  struct iio_channel *rx0_i, *rx0_q;
  struct iio_channel *rx1_i, *rx1_q;
  struct iio_channel *alt_chan;
  
  SUCOMPLEX *synth_buffer;
  SUSCOUNT   synth_buffer_size;
  SUSCOUNT   synth_buffer_consumed;
};

SUBOOL suscan_source_register_ad9361(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* _AD9361_H */
