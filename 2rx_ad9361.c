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

#include "2rx_ad9361.h"
#include <analyzer/source.h>
#include <util/hashlist.h>
#include <util/cfg.h>
#include <sys/time.h>
#include <poll.h>

#include <ad9361.h>

/****************************** Implementation ********************************/
SUPRIVATE void
suscan_source_ad9361_close(void *ptr)
{
  struct suscan_source_ad9361 *self = (struct suscan_source_ad9361 *) ptr;

  if (self->rx0_i != NULL)
    iio_channel_disable(self->rx0_i);
  
  if (self->rx0_q != NULL)
    iio_channel_disable(self->rx0_q);

  if (self->rx1_i != NULL)
    iio_channel_disable(self->rx1_i);

  if (self->rx1_q != NULL)
    iio_channel_disable(self->rx1_q);

  if (self->rx_buf != NULL) {
    iio_buffer_cancel(self->rx_buf);
    iio_buffer_destroy(self->rx_buf);
  }

  if (self->context != NULL)
    iio_context_destroy(self->context);

  if (self->synth_buffer != NULL)
    free(self->synth_buffer);
  
  free(self);
}

SUPRIVATE SUBOOL
suscan_source_ad9361_find(
  struct suscan_source_ad9361 *self,
  suscan_source_config_t *config)
{
  const char *uri;
  SUBOOL ok = SU_FALSE;

  uri = suscan_source_config_get_param(config, "uri");
  if (uri == NULL)
    uri = "ip:192.168.1.10";
  
  self->context = iio_create_context_from_uri(uri);
  if (self->context == NULL) {
    SU_ERROR("Cannot find Pluto/ANTSDR device at `%s'\n", uri);
    goto done;
  }

  self->phy_dev = iio_context_find_device(self->context, "ad9361-phy");
  if (self->phy_dev == NULL) {
    SU_ERROR("IIO context created, but no AD9361 (real or hack) found\n");
    SU_ERROR("Please make sure that the firmware is correct and that\n");
    SU_ERROR("the underlying AD936x device is detected as AD9361\n");
    goto done;
  }

  self->phy_rx0 = iio_device_find_channel(self->phy_dev, "voltage0", false);
  if (self->phy_rx0 == NULL) {
    SU_ERROR("AD9361 device found, but no RX channel 0 was found\n");
    goto done;
  }

  self->phy_rx1 = iio_device_find_channel(self->phy_dev, "voltage1", false);
  if (self->phy_rx1 == NULL) {
    SU_ERROR("AD9361 device found, but no RX channel 1 was found\n");
    goto done;
  }

  iio_channel_attr_write(self->phy_rx1, "rf_port_select", "A_BALANCED");
  
  iio_channel_attr_write(self->phy_rx0, "gain_control_mode", "manual");
  iio_channel_attr_write(self->phy_rx1, "gain_control_mode", "manual");

  self->rx_dev = iio_context_find_device(self->context, "cf-ad9361-lpc");
  if(self->rx_dev == NULL){
    SU_ERROR("AD9361 device found, but RX IQ device is not available.\n");
    goto done;
  }

  self->alt_chan = iio_device_find_channel(self->phy_dev, "altvoltage0", true);
  if(self->alt_chan == NULL){
    SU_ERROR("AD9361 device found, but alternate voltage channel is missing.\n");
    goto done;
  }

  ok = SU_TRUE;

done:
  return ok;
}

SUPRIVATE struct iio_channel *
suscan_source_ad9361_config_stream_dev(
  struct suscan_source_ad9361 *self,
  int chid)
{
  struct iio_channel *channel;
  char name[16];
  snprintf(name, sizeof(name), "voltage%d", chid);

  channel = iio_device_find_channel(self->rx_dev, name, false);
  if (channel == NULL) {
    SU_ERROR("AD9361: IQ channel `%s' not found\n", name);
    return NULL;
  }

  iio_channel_enable(channel);

  return channel;
}

/*
 * Black magic obtained from https://github.com/pothosware/SoapyPlutoSDR/blob/master/PlutoSDR_Settings.cpp
 */
SUPRIVATE SUBOOL
suscan_source_ad9316_set_samp_rate(
  struct suscan_source_ad9361 *self,
  SUFLOAT rate)
{
  SUBOOL ok = SU_FALSE;
  SUBOOL decimation = SU_FALSE;
  long long samplerate = (long long) rate;
  int const fir = 4;

	/* 
   * note: sample rates below 25e6/12 need x8 decimation/interpolation or x4 FIR to 25e6/48,
	 * below 25e6/96 need x8 decimation/interpolation and x4 FIR, minimum is 25e6/384
	 * if libad9361 is available it will load an approporiate FIR.
   */
  if (samplerate < (25e6 / (12 * fir))) {
    if (samplerate * 8 < (25e6 / 48)) {
      SU_ERROR("sample rate of %g Hz is not supported (too low).\n", rate);
      goto done;
    } else if (samplerate * 8 < (25e6 / 12)) {
      SU_ERROR("sample rate of %g Hz needs a FIR setting loaded.\n", rate);
      goto done;
    }

    decimation = SU_TRUE;
    samplerate = samplerate * 8;
  }

  iio_channel_attr_write_longlong(
    iio_device_find_channel(self->phy_dev, "voltage0", false),
    "sampling_frequency",
    samplerate);
  
  iio_channel_attr_write_longlong(
    iio_device_find_channel(self->rx_dev, "voltage0", false),
    "sampling_frequency",
    decimation ? samplerate / 8 : samplerate);

	if (ad9361_set_bb_rate(self->phy_dev, (unsigned long) samplerate)) {
		SU_ERROR("Failed to set baseband rate\n");
    goto done;
  }

  ok = SU_TRUE;

done:
  return ok;
}

SUPRIVATE SUBOOL
suscan_source_ad9361_init(
  struct suscan_source_ad9361 *self,
  suscan_source_config_t *config)
{
  SUBOOL ok = SU_FALSE;

  SU_TRY(suscan_source_ad9316_set_samp_rate(self, config->samp_rate));

  SU_TRYZ(
    iio_channel_attr_write_longlong(
      self->phy_rx0,
      "rf_bandwidth",
      config->samp_rate / 16));

  SU_TRYZ(
    iio_channel_attr_write_longlong(
      self->alt_chan,
      "frequency",
      config->freq - config->lnb_freq));

  /* Open in 2R2T mode */
  SU_TRY(self->rx0_i = suscan_source_ad9361_config_stream_dev(self, 0));
  SU_TRY(self->rx0_q = suscan_source_ad9361_config_stream_dev(self, 1));
  SU_TRY(self->rx1_i = suscan_source_ad9361_config_stream_dev(self, 2));
  SU_TRY(self->rx1_q = suscan_source_ad9361_config_stream_dev(self, 3));

  SU_TRYZ(iio_device_set_kernel_buffers_count(self->rx_dev, 2));

  self->samp_rate = config->samp_rate;

  /* I am sure EA4GPZ will love this */
  su_ncqo_init(&self->rx0_nco, -.5);
  su_ncqo_init(&self->rx1_nco, +.5);

  ok = SU_TRUE;

done:
  return ok;
}

static struct suscan_source_gain_desc g_ad9361_pga_desc = {
  .name = "PGA",
  .def  = 0,
  .min  = 0,
  .max  = 73,
  .step = 1
};

SUPRIVATE SUBOOL
suscan_source_ad9361_init_info(
  struct suscan_source_ad9361 *self,
  struct suscan_source_info *info)
{
  struct suscan_source_gain_info *ginfo = NULL;
  struct suscan_source_gain_value gain;
  char *dup = NULL;
  SUBOOL ok = SU_FALSE;

  info->realtime    = SU_TRUE;
  
  /* Adjust permissions */
  info->permissions = SUSCAN_ANALYZER_ALL_SDR_PERMISSIONS;
  info->permissions &= ~SUSCAN_ANALYZER_PERM_SET_DC_REMOVE;

  /* Set sample rate */
  info->source_samp_rate    = self->samp_rate;
  info->effective_samp_rate = self->samp_rate;
  info->measured_samp_rate  = self->samp_rate;
  
  /* Adjust limits */
  info->freq_min = 70e6;
  info->freq_max = 6e9;

  /* Get current source time */
  gettimeofday(&info->source_time, NULL);
  gettimeofday(&info->source_start, NULL);

  /* Add gains */
  gain.desc = &g_ad9361_pga_desc;
  gain.val  =  g_ad9361_pga_desc.def;

  SU_TRY(ginfo = suscan_source_gain_info_new(&gain));
  SU_TRYC(PTR_LIST_APPEND_CHECK(info->gain, ginfo));

  ginfo = NULL;

  /* Add antenna */
  SU_TRY(dup = strdup("A_BALANCED"));
  SU_TRYC(PTR_LIST_APPEND_CHECK(info->antenna, dup));
  dup = NULL;

  ok = SU_TRUE;

done:
  if (ginfo != NULL)
    suscan_source_gain_info_destroy(ginfo);

  if (dup != NULL)
    free(dup);

  return ok;
}

SUPRIVATE void *
suscan_source_ad9361_open(
  suscan_source_t *source,
  suscan_source_config_t *config,
  struct suscan_source_info *info)
{
  struct suscan_source_ad9361 *new = NULL;

  SU_ALLOCATE_FAIL(new, struct suscan_source_ad9361);
  new->config = config;
  new->source = source;

  SU_ALLOCATE_MANY_FAIL(
    new->synth_buffer,
    AD9361_DEFAULT_BUFFER_SIZE,
    SUCOMPLEX);
  
  new->synth_buffer_size     = AD9361_DEFAULT_BUFFER_SIZE;
  new->synth_buffer_consumed = new->synth_buffer_size;
  
  if (!suscan_source_ad9361_find(new, config))
    goto fail;

  if (!suscan_source_ad9361_init(new, config))
    goto fail;

  if (!suscan_source_ad9361_init_info(new, info))
    goto fail;
  
  return new;

fail:
  if (new != NULL)
    suscan_source_ad9361_close(new);

  return NULL;
}

SUPRIVATE SUBOOL
suscan_source_ad9361_start(void *userdata)
{
  struct suscan_source_ad9361 *self = (struct suscan_source_ad9361 *) userdata;

  if (self->started)
    return SU_TRUE;

  self->rx_buf = iio_device_create_buffer(
    self->rx_dev,
    AD9361_DEFAULT_BUFFER_SIZE,
    false);
  if (self->rx_buf == NULL) {
    SU_ERROR(
      "AD9361: failed to create 4 x %d byte kernel buffers\n",
      AD9361_DEFAULT_BUFFER_SIZE);
    return SU_FALSE;
  }

  self->started = SU_TRUE;
  self->running = SU_TRUE;

  return SU_TRUE;
}

SUPRIVATE SUBOOL
suscan_source_ad9361_acquire(struct suscan_source_ad9361 *self)
{
  SUBOOL ok = SU_FALSE;
  const int16_t *data;
  SUSCOUNT samples, i, j;
  SUCOMPLEX rx0, rx1;
  int n_read;

  n_read = iio_buffer_refill(self->rx_buf);
  if (n_read < 0)
    goto done;

  samples = n_read / (4 * sizeof(int16_t));

  if (samples > AD9361_DEFAULT_BUFFER_SIZE) {
    SU_ERROR("Buffer is just too big! This is an error\n");
    return SU_FALSE;
  }

  data = iio_buffer_start(self->rx_buf);

  /* TODO: Use Volk */
  for (i = 0; i < samples; ++i) {
    j = i << 2;

    rx0 = (data[j | 0] + I * data[j | 1]) / 32768.;
    rx1 = (data[j | 2] + I * data[j | 3]) / 32768.;

    /* Just tell me you don't love how these channels are combined */
    self->synth_buffer[i] = 
        rx0 * su_ncqo_read(&self->rx0_nco) 
      + rx1 * su_ncqo_read(&self->rx1_nco);
  }

  self->synth_buffer_size     = samples;
  self->synth_buffer_consumed = 0;

  ok = SU_TRUE;

done:
  return ok;
}

SUPRIVATE SUSDIFF
suscan_source_ad9361_read(
  void *userdata,
  SUCOMPLEX *buf,
  SUSCOUNT size)
{
  struct suscan_source_ad9361 *self = (struct suscan_source_ad9361 *) userdata;
  SUSCOUNT available;

  if (!self->running)
    return 0;

  available = self->synth_buffer_size - self->synth_buffer_consumed;
  if (available == 0) {
    if (!suscan_source_ad9361_acquire(self))
      return -1;
    available = self->synth_buffer_size - self->synth_buffer_consumed;
  }

  if (size > available)
    size = available;

  memcpy(
    buf,
    self->synth_buffer + self->synth_buffer_consumed,
    size * sizeof(SUCOMPLEX));

  self->synth_buffer_consumed += size;
  self->total_samples         += size;

  return size;
}

SUPRIVATE void
suscan_source_ad9361_get_time(void *userdata, struct timeval *tv)
{
  (void) userdata;
  gettimeofday(tv, NULL);
}

SUPRIVATE SUBOOL
suscan_source_ad9361_cancel(void *userdata)
{
  struct suscan_source_ad9361 *self = (struct suscan_source_ad9361 *) userdata;
  
  self->running = SU_FALSE;

  return SU_TRUE;
}

SUPRIVATE SUBOOL
suscan_source_ad9361_set_frequency(void *userdata, SUFREQ freq)
{
  struct suscan_source_ad9361 *self = (struct suscan_source_ad9361 *) userdata;
  int ret;
  
  ret = iio_channel_attr_write_longlong(
    self->alt_chan,
    "frequency",
    (long long) freq);
  if (ret != 0) {
    SU_ERROR("Failed to set device frequency (%s)\n", strerror(-ret));
    return SU_FALSE;
  }
  return SU_TRUE;
}

SUPRIVATE SUBOOL
suscan_source_ad9361_set_gain(void *userdata, const char *name, SUFLOAT gain)
{
  struct suscan_source_ad9361 *self = (struct suscan_source_ad9361 *) userdata;
  int ret;

  if (strcmp(name, "PGA") != 0) {
    SU_ERROR("Unknown gain `%s'\n", name);
    return SU_FALSE;
  }

  ret = iio_channel_attr_write_double(self->phy_rx0, "hardwaregain", gain);
  if (ret != 0) {
    SU_ERROR("Failed to set gain on RX 0: %s\n", strerror(-ret));
    return SU_FALSE;
  }

  ret = iio_channel_attr_write_double(self->phy_rx1, "hardwaregain", gain);
  if (ret != 0) {
    SU_ERROR("Failed to set gain on RX 1: %s\n", strerror(-ret));
    return SU_FALSE;
  }

  return SU_TRUE;
}

SUPRIVATE SUBOOL
suscan_source_ad9361_get_freq_limits(
  const suscan_source_config_t *self,
  SUFREQ *min,
  SUFREQ *max)
{
  (void) self;

  *min = 70e6;
  *max = 6e9;

  return SU_TRUE;
}

SUPRIVATE struct suscan_source_interface g_ad9361_source =
{
  .name            = "ad9361",
  .desc            = "Pluto/ANTSDR 2RX combined source",
  .realtime        = SU_FALSE,

  .open            = suscan_source_ad9361_open,
  .close           = suscan_source_ad9361_close,
  .start           = suscan_source_ad9361_start,
  .cancel          = suscan_source_ad9361_cancel,
  .read            = suscan_source_ad9361_read,
  .get_time        = suscan_source_ad9361_get_time,
  .set_frequency   = suscan_source_ad9361_set_frequency,
  .set_gain        = suscan_source_ad9361_set_gain,
  .get_freq_limits = suscan_source_ad9361_get_freq_limits,

  /* Unset members */
  .is_real_time    = NULL,
  .set_antenna     = NULL,
  .set_bandwidth   = NULL,
  .set_ppm         = NULL,
  .set_dc_remove   = NULL,
  .set_agc         = NULL,
  .estimate_size   = NULL,
  .seek            = NULL,
  .max_size        = NULL,
};

SUBOOL
suscan_source_register_ad9361(void)
{
  int ndx;
  SUBOOL ok = SU_FALSE;

  SU_TRYC(ndx = suscan_source_register(&g_ad9361_source));

  ok = SU_TRUE;

done:
  return ok;
}
