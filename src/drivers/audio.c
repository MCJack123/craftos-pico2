/**
 * @file audio.c
 * @brief Audio processing module
 */

/*
 * Adapted from https://github.com/TuriSc/RP2040-PWM-DMA-Audio
 * MIT License
 * 
 * Copyright (c) 2021 Ricardo Massaro
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/pwm.h"
#include "hardware/sync.h"
#include "hardware/clocks.h"
#include "audio.h"

/**
 * @def REPETITION_RATE
 * @brief Sample repetition rate
 */
#define REPETITION_RATE 4

/**
 * @struct MIXER_SOURCE
 * @brief Mixer source structure
 */
struct MIXER_SOURCE {
  /**
   * @var samples
   * @brief Sample data
   */
  const unsigned char *samples;

  /**
   * @var len
   * @brief Sample length
   */
  int len;

  /**
   * @var loop_start
   * @brief Loop start index
   */
  int loop_start;

  /**
   * @var pos
   * @brief Current position
   */
  int pos;

  /**
   * @var active
   * @brief Source active flag
   */
  bool active;

  /**
   * @var loop
   * @brief Loop flag
   */
  bool loop;

  /**
   * @var volume
   * @brief Volume value (8.8 fixed point)
   */
  uint16_t volume;

  audio_callback callback;
  void* callback_userp;
};

struct AUDIO_OUTPUT {
  bool active;
  int audio_pin;

  /**
   * @var single_sample
   * @brief Single sample value
   */
  uint32_t single_sample;

  /**
   * @var single_sample_ptr
   * @brief Pointer to single sample value
   */
  uint32_t *single_sample_ptr;

  /**
   * @var pwm_dma_chan
   * @brief PWM DMA channel
   */
  int pwm_dma_chan;

  /**
   * @var trigger_dma_chan
   * @brief Trigger DMA channel
   */
  int trigger_dma_chan;

  /**
   * @var sample_dma_chan
   * @brief Sample DMA channel
   */
  int sample_dma_chan;

  /**
   * @var audio_buffers
   * @brief Audio buffers
   */
  uint8_t audio_buffers[2][AUDIO_BUFFER_SIZE];

  /**
   * @var cur_audio_buffer
   * @brief Current audio buffer index
   */
  volatile int cur_audio_buffer;

  /**
   * @var last_audio_buffer
   * @brief Last audio buffer index
   */
  volatile int last_audio_buffer;

  /**
   * @var mixer_sources
   * @brief Mixer sources array
   */
  struct MIXER_SOURCE mixer_sources[AUDIO_MAX_SOURCES];

  /**
   * @var mixer_buffer
   * @brief Mixer buffer
   */
  int16_t mixer_buffer[AUDIO_BUFFER_SIZE];
};

struct AUDIO_OUTPUT outputs[AUDIO_MAX_OUTPUTS] = {0};

/**
 * @brief DMA handler function
 */
static void __isr __time_critical_func(dma_handler)()
{
  int n = 0;
  for (; n < AUDIO_MAX_OUTPUTS && !dma_channel_get_irq1_status(outputs[n].trigger_dma_chan); n++);
  if (n == AUDIO_MAX_OUTPUTS) return; /* spurious interrupt */
  outputs[n].cur_audio_buffer = 1 - outputs[n].cur_audio_buffer;
  dma_hw->ch[outputs[n].sample_dma_chan].al1_read_addr       = (intptr_t) &outputs[n].audio_buffers[outputs[n].cur_audio_buffer][0];
  dma_hw->ch[outputs[n].trigger_dma_chan].al3_read_addr_trig = (intptr_t) &outputs[n].single_sample_ptr;

  dma_hw->ints1 = 1u << outputs[n].trigger_dma_chan;
}

/**
 * @brief Initialize audio module
 * @param audio_pin Audio pin number
 * @param sample_freq Sample frequency
 */
audio_output_t audio_init(int audio_pin, int sample_freq) /* TODO: IRQ number */
{
  int output_num = 0;
  for (; output_num < AUDIO_MAX_OUTPUTS && outputs[output_num].active; output_num++);
  if (output_num == AUDIO_MAX_OUTPUTS) return NULL;

  audio_output_t output = &outputs[output_num];
  memset(output, 0, sizeof(struct AUDIO_OUTPUT));
  output->single_sample_ptr = &output->single_sample;
  output->active = true;
  output->audio_pin = audio_pin;

  gpio_set_function(audio_pin, GPIO_FUNC_PWM);

  int audio_pin_slice = pwm_gpio_to_slice_num(audio_pin);
  int audio_pin_chan = pwm_gpio_to_channel(audio_pin);

  uint f_clk_sys = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_SYS);
  float clock_div = ((float)f_clk_sys * 1000.0f) / 254.0f / (float) sample_freq / (float) REPETITION_RATE;

  pwm_config config = pwm_get_default_config();
  pwm_config_set_clkdiv(&config, clock_div);
  pwm_config_set_wrap(&config, 254);
  pwm_init(audio_pin_slice, &config, true);

  output->pwm_dma_chan     = dma_claim_unused_channel(false);
  output->trigger_dma_chan = dma_claim_unused_channel(false);
  output->sample_dma_chan  = dma_claim_unused_channel(false);
  if (output->pwm_dma_chan == -1 || output->trigger_dma_chan == -1 || output->sample_dma_chan == -1) return NULL;

  // setup PWM DMA channel
  dma_channel_config pwm_dma_chan_config = dma_channel_get_default_config(output->pwm_dma_chan);
  channel_config_set_transfer_data_size(&pwm_dma_chan_config, DMA_SIZE_32);              // transfer 32 bits at a time
  channel_config_set_read_increment(&pwm_dma_chan_config, false);                        // always read from the same address
  channel_config_set_write_increment(&pwm_dma_chan_config, false);                       // always write to the same address
  channel_config_set_chain_to(&pwm_dma_chan_config, output->sample_dma_chan);                    // trigger sample DMA channel when done
  channel_config_set_dreq(&pwm_dma_chan_config, DREQ_PWM_WRAP0 + audio_pin_slice);       // transfer on PWM cycle end
  dma_channel_configure(output->pwm_dma_chan,
                        &pwm_dma_chan_config,
                        &pwm_hw->slice[audio_pin_slice].cc,   // write to PWM slice CC register
                        &output->single_sample,                       // read from single_sample
                        REPETITION_RATE,                      // transfer once per desired sample repetition
                        false                                 // don't start yet
                        );

  // setup trigger DMA channel
  dma_channel_config trigger_dma_chan_config = dma_channel_get_default_config(output->trigger_dma_chan);
  channel_config_set_transfer_data_size(&trigger_dma_chan_config, DMA_SIZE_32);          // transfer 32-bits at a time
  channel_config_set_read_increment(&trigger_dma_chan_config, false);                    // always read from the same address
  channel_config_set_write_increment(&trigger_dma_chan_config, false);                   // always write to the same address
  channel_config_set_dreq(&trigger_dma_chan_config, DREQ_PWM_WRAP0 + audio_pin_slice);   // transfer on PWM cycle end
  dma_channel_configure(output->trigger_dma_chan,
                        &trigger_dma_chan_config,
                        &dma_hw->ch[output->pwm_dma_chan].al3_read_addr_trig,     // write to PWM DMA channel read address trigger
                        &output->single_sample_ptr,                               // read from location containing the address of single_sample
                        REPETITION_RATE * AUDIO_BUFFER_SIZE,              // trigger once per audio sample per repetition rate
                        false                                             // don't start yet
                        );
  dma_channel_set_irq1_enabled(output->trigger_dma_chan, true);    // fire interrupt when trigger DMA channel is done
  if (!irq_has_handler(DMA_IRQ_1)) irq_set_exclusive_handler(DMA_IRQ_1, dma_handler);
  irq_set_enabled(DMA_IRQ_1, true);

  // setup sample DMA channel
  dma_channel_config sample_dma_chan_config = dma_channel_get_default_config(output->sample_dma_chan);
  channel_config_set_transfer_data_size(&sample_dma_chan_config, DMA_SIZE_8);  // transfer 8-bits at a time
  channel_config_set_read_increment(&sample_dma_chan_config, true);            // increment read address to go through audio buffer
  channel_config_set_write_increment(&sample_dma_chan_config, false);          // always write to the same address
  dma_channel_configure(output->sample_dma_chan,
                        &sample_dma_chan_config,
                        (char*)&output->single_sample + 2*audio_pin_chan,  // write to single_sample
                        &output->audio_buffers[0][0],                      // read from audio buffer
                        1,                                         // only do one transfer (once per PWM DMA completion due to chaining)
                        false                                      // don't start yet
                        );

  // clear audio buffers
  memset(output->audio_buffers[0], 128, AUDIO_BUFFER_SIZE);
  memset(output->audio_buffers[1], 128, AUDIO_BUFFER_SIZE);
  
  // kick things off with the trigger DMA channel
  dma_channel_start(output->trigger_dma_chan);
  return output;
}

/**
 * @brief Get audio buffer
 * @return Audio buffer pointer or NULL if not available
 */
uint8_t *audio_get_buffer(audio_output_t output)
{
  if (output->last_audio_buffer == output->cur_audio_buffer) {
    return NULL;
  }

  uint8_t *buf = output->audio_buffers[output->last_audio_buffer];
  output->last_audio_buffer = output->cur_audio_buffer;
  return buf;
}

/**
 * @brief Claim an unused audio source
 * @return Index of the claimed source or -1 if none available
 */
static int audio_claim_unused_source(audio_output_t output)
{
  for (int i = 0; i < AUDIO_MAX_SOURCES; i++) {
    if (! output->mixer_sources[i].active) {
      output->mixer_sources[i].active = true;
      return i;
    }
  }
  return -1;
}

bool audio_has_unused_source(audio_output_t output)
{
  for (int i = 0; i < AUDIO_MAX_SOURCES; i++) {
    if (! output->mixer_sources[i].active) {
      return true;
    }
  }
  return false;
}

/**
 * @brief Play a single audio sample
 * @param samples Sample data
 * @param len Sample length
 * @return Index of the played source or -1 if none available
 */
int audio_play_once(audio_output_t output, const uint8_t *samples, int len, audio_callback callback, void* userp)
{
  int source_id = audio_claim_unused_source(output);
  if (source_id < 0) {
    return -1;
  }
  struct MIXER_SOURCE *source = &output->mixer_sources[source_id];
  source->samples = samples;
  source->len = len;
  source->pos = 0;
  source->loop = false;
  source->volume = 256;
  source->callback = callback;
  source->callback_userp = userp;
  return source_id;
}

/**
 * @brief Play a looping audio sample
 * @param samples Sample data
 * @param len Sample length
 * @param loop_start Loop start index
 * @return Index of the played source or -1 if none available
 */
int audio_play_loop(audio_output_t output, const uint8_t *samples, int len, int loop_start, audio_callback callback, void* userp)
{
  int source_id = audio_play_once(output, samples, len, callback, userp);
  if (source_id < 0) {
    return -1;
  }
  struct MIXER_SOURCE *source = &output->mixer_sources[source_id];
  source->loop = true;
  source->loop_start = loop_start;
  return source_id;
}

/**
 * @brief Stop an audio source
 * @param source_id Index of the source to stop
 */
void audio_source_stop(audio_output_t output, int source_id)
{
  if (source_id < 0)
    for (source_id = 0; source_id < AUDIO_MAX_SOURCES; source_id++)
      output->mixer_sources[source_id].active = false;
  else
    output->mixer_sources[source_id].active = false;
}

/**
 * @brief Set the volume of an audio source
 * @param source_id Index of the source to set volume for
 * @param volume Volume value (8.8 fixed point)
 */
void audio_source_set_volume(audio_output_t output, int source_id, uint16_t volume)
{
  output->mixer_sources[source_id].volume = volume;
}

/**
 * @brief Check if an audio source is currently playing
 * @param source_id Index of the source to check
 * @return true if the source is active, false otherwise
 */
bool audio_source_is_active(audio_output_t output, int source_id)
{
  if (source_id < 0 || source_id >= AUDIO_MAX_SOURCES) {
    return false;
  }
  return output->mixer_sources[source_id].active;
}

/**
 * @brief Perform a single step of the audio mixer
 */
void audio_mixer_step(audio_output_t output)
{
  uint8_t *audio_buffer = audio_get_buffer(output);
  if (! audio_buffer) return;

  // setup 16-bit mixer buffer
  memset(output->mixer_buffer, 0, sizeof(output->mixer_buffer));

  // mix to 16-bit mixer buffer
  for (int i = 0; i < AUDIO_MAX_SOURCES; i++) {
    if (! output->mixer_sources[i].active) continue;
    struct MIXER_SOURCE *source = &output->mixer_sources[i];

    // mix source
    int mix_len = source->len - source->pos;
    if (mix_len > AUDIO_BUFFER_SIZE) {
      mix_len = AUDIO_BUFFER_SIZE;
    }
    for (int i = 0; i < mix_len; i++) {
      output->mixer_buffer[i] += ((source->samples[i+source->pos]-128) * source->volume) >> 8;
    }
    source->pos += mix_len;

    // handle source termination
    if (source->pos == source->len) {
      if (source->loop) {
        source->pos = source->loop_start;
      } else {
        source->active = false;
        if (source->callback) source->callback(output, source->samples, source->callback_userp);
      }
    }
  }

  // convert 16-bit mixer buffer to 8-bit output buffer
  for (int i = 0; i < AUDIO_BUFFER_SIZE; i++) {
    int sample = output->mixer_buffer[i] + 128;
    if (sample < 0) {
      audio_buffer[i] = 0;
    } else if (sample > 255) {
      audio_buffer[i] = 255;
    } else {
      audio_buffer[i] = sample;
    }
  }
}

void audio_close(audio_output_t output) {
  output->active = false;
  dma_channel_abort(output->trigger_dma_chan);
  dma_channel_cleanup(output->trigger_dma_chan);
  dma_channel_cleanup(output->sample_dma_chan);
  dma_channel_cleanup(output->pwm_dma_chan);
  gpio_set_function(output->audio_pin, GPIO_FUNC_NULL);
}

