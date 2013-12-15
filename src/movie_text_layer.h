/* Copyright (c) 2013, René Köcher <shirk@bitspin.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/* /movie_text_layer.h, created 2013-12-15 / */

#ifndef __MOVIE_TEXT_LAYER_H
#define __MOVIE_TEXT_LAYER_H

#include <pebble.h>

typedef struct Layer MovieTextLayer;

typedef enum
{
  MovieTextUpdateNone,          // Don't update now, wait for redraw
  MovieTextUpdateInstant,       // Update now with animation
  MovieTextUpdateSlideLeft,     // slide-to-left animation
  MovieTextUpdateSlideRight,    // slide-to-right animation
  MovieTextUpdateDelay          // Delay update until next animation (only origin)
} MovieTextUpdateMode;

MovieTextLayer* movie_text_layer_create( GPoint origin, int16_t hight );
void movie_text_layer_destroy( MovieTextLayer* layer );
Layer* movie_text_layer_get_layer( MovieTextLayer* layer );

void movie_text_layer_set_text_color( MovieTextLayer* layer, GColor color );
void movie_text_layer_set_background_color( MovieTextLayer* layer, GColor color );
void movie_text_layer_set_text( MovieTextLayer* layer, const char* text, MovieTextUpdateMode mode, bool delay );
void movie_text_layer_set_origin( MovieTextLayer* layer, GPoint origin, MovieTextUpdateMode mode, bool delay );
void movie_text_layer_set_font( MovieTextLayer* layer, GFont font );

GColor movie_text_layer_get_text_color( MovieTextLayer* layer );
GColor movie_text_layer_get_background_color( MovieTextLayer* layer );
const char* movie_text_layer_get_text( MovieTextLayer* layer );
GPoint movie_textLayer_get_origin( MovieTextLayer* layer );
GFont movie_text_layer_get_font( MovieTextLayer* layer );

#endif
