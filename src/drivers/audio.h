/**
 * @file audio.h
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

#ifndef AUDIO_H_FILE
#define AUDIO_H_FILE

/**
 * @def AUDIO_BUFFER_SIZE
 * @brief Size of the audio buffer
 */
#define AUDIO_BUFFER_SIZE 1024

/**
 * @def AUDIO_MAX_SOURCES
 * @brief Maximum number of audio sources
 */
#define AUDIO_MAX_SOURCES 1

/**
 * @def AUDIO_MAX_OUTPUTS
 * @brief Maximum number of audio outputs
 */
#define AUDIO_MAX_OUTPUTS 2

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AUDIO_OUTPUT* audio_output_t;
typedef void (*audio_callback)(audio_output_t output, const uint8_t *samples, void* userp);

/**
 * @brief Initialize audio module
 * @param audio_pin Audio pin number
 * @param sample_freq Sample frequency
 */
audio_output_t audio_init(int audio_pin, int sample_freq);

/**
 * @brief Get audio buffer
 * @return Audio buffer pointer or NULL if not available
 */
uint8_t *audio_get_buffer(audio_output_t output);

bool audio_has_unused_source(audio_output_t output);

/**
 * @brief Play a single audio sample
 * @param samples Sample data
 * @param len Sample length
 * @return Index of the played source or -1 if none available
 */
int audio_play_once(audio_output_t output, const uint8_t *samples, int len, audio_callback callback, void* userp);

/**
 * @brief Play a looping audio sample
 * @param samples Sample data
 * @param len Sample length
 * @param loop_start Loop start index
 * @return Index of the played source or -1 if none available
 */
int audio_play_loop(audio_output_t output, const uint8_t *samples, int len, int loop_start, audio_callback callback, void* userp);

/**
 * @brief Stop an audio source
 * @param source_id Index of the source to stop
 */
void audio_source_stop(audio_output_t output, int source_id);

/**
 * @brief Set the volume of an audio source
 * @param source_id Index of the source to set volume for
 * @param volume Volume value (8.8 fixed point)
 */
void audio_source_set_volume(audio_output_t output, int source_id, uint16_t volume);

/**
 * @brief Check if an audio source is currently playing
 * @param source_id Index of the source to check
 * @return true if the source is active, false otherwise
 */
bool audio_source_is_active(audio_output_t output, int source_id);

/**
 * @brief Perform a single step of the audio mixer
 */
void audio_mixer_step(audio_output_t output);

void audio_close(audio_output_t output);

#ifdef __cplusplus
}
#endif

#endif /* AUDIO_H_FILE */

