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

/* /movie_text_layer.c, created 2013-12-15 / */

#include "movie_text_layer.h"

typedef struct _MovieTextLayerData
{
  GFont  font;
  GColor fg, bg;
  GPoint origin;

  char *buf_r;
  char buf_l[20];

  bool animating_in;
  bool animating_out;
  bool animating_offset;

  MovieTextUpdateMode animation_mode;
  PropertyAnimation *animation;

} __attribute__((__packed__)) MovieTextLayerData;

#define with_movie_layer( l, n, code... ) \
  if( (l) ) { MovieTextLayerData* n = (MovieTextLayerData*)layer_get_data( (Layer*)(l) ); code; }

#define SCREEN_WIDTH 144

static void _animation_started( struct Animation* animation, void* context )
{
}

static void _animation_stopped( struct Animation* animation, bool finished, void* context )
{
  Layer* layer = (Layer*)context;
  with_movie_layer( layer, data,
  {
    GRect base = {
      .origin = data->origin,
      .size   = { .h = layer_get_frame( layer ).size.h, .w = SCREEN_WIDTH }
    };

    GRect off_screen = {
      .origin = { .x = -SCREEN_WIDTH, .y = data->origin.y },
      .size   = { .h = layer_get_frame( layer ).size.h, .w = SCREEN_WIDTH }
    };

    switch( data->animation_mode )
    {
      case MovieTextUpdateNone:
      case MovieTextUpdateInstant:
      case MovieTextUpdateDelay:
      case MovieTextUpdateSlideRight:
        off_screen.origin.x = SCREEN_WIDTH;
      break;

      case MovieTextUpdateSlideLeft:
      {
        if( data->animating_out )
        {
          if( data->buf_r )
          {
            strncpy( data->buf_l, data->buf_r, sizeof( data->buf_l ) );
          }
        }
        break;
      }

    }

    if( data->animating_out && finished )
    {

      data->animating_out = false;
      data->animating_in  = true;

      data->animation->values.from.grect = off_screen;
      data->animation->values.to.grect   = base;

      layer_set_frame( (Layer*)layer, off_screen );
      animation_set_delay( (Animation*)data->animation, 0 );
      animation_set_duration( (Animation*)data->animation, 500 );
      animation_schedule( (Animation*)data->animation );
    }
    else
    {
      layer_set_frame( layer, base );

      data->animating_in  = false;
      data->animating_out = false;
      data->animation_mode = MovieTextUpdateInstant;
    }
  } )
}

static void _update_layer( Layer* layer, GContext* ctx )
{
  with_movie_layer( layer, data,
  {
    GRect frame = {
      .origin = GPointZero,
      .size   = layer_get_frame( (Layer*)layer ).size
    };

    if( data->animating_out || data->animating_in )
    {
      frame.size.w = SCREEN_WIDTH;
    }

    graphics_context_set_fill_color( ctx, data->bg );
    graphics_context_set_text_color( ctx, data->fg );
    graphics_context_set_stroke_color( ctx, data->fg );

    graphics_draw_text( ctx, data->buf_l, data->font, frame,
                        GTextOverflowModeTrailingEllipsis,
                        GTextAlignmentLeft,
                        NULL );
  })
}

MovieTextLayer* movie_text_layer_create( GPoint origin, int16_t height )
{
  GRect frame = {
    .origin = origin,
    .size   = { .h = height, .w = SCREEN_WIDTH }
  };

  Layer* layer = layer_create_with_data( frame, sizeof( MovieTextLayerData ) );

  with_movie_layer( layer, data,
  {
    data->fg = GColorBlack;
    data->bg = GColorWhite;
    data->font = fonts_get_system_font( FONT_KEY_GOTHIC_14_BOLD );
    data->origin = frame.origin;
    data->animating_out = false;
    data->animating_in  = false;
    data->animation_mode = MovieTextUpdateNone;
    data->animation = property_animation_create_layer_frame( layer, &frame, &frame );

    animation_set_handlers( (Animation*)data->animation,
                            (AnimationHandlers){ 
                              .started = _animation_started, 
                              .stopped = _animation_stopped
                            },
                            (void*)layer );
    animation_set_curve( (Animation*)data->animation, AnimationCurveEaseInOut );

    memset( data->buf_l, 0, sizeof( data->buf_l ) );
    data->buf_r = NULL;

    layer_set_update_proc( layer, _update_layer );
  } )

  return layer;
}

void movie_text_layer_destroy( MovieTextLayer* layer )
{
  with_movie_layer( layer, data,
  {
    property_animation_destroy( data->animation );
    layer_destroy( layer );
  } )
}

Layer* movie_text_layer_get_layer( MovieTextLayer* layer )
{
  return (Layer*)layer;
}

void movie_text_layer_set_text_color( MovieTextLayer* layer, GColor color )
{
  with_movie_layer( layer, data, { data->fg = color; } );
}

void movie_text_layer_set_background_color( MovieTextLayer* layer, GColor color )
{
  with_movie_layer( layer, data, { data->bg = color; } );
}

void movie_text_layer_set_text( MovieTextLayer* layer, const char* text, 
                                MovieTextUpdateMode mode, bool delay )
{
  with_movie_layer( layer, data,
  {
    GRect base = layer_get_frame( (Layer*)layer );
    base.size.w = SCREEN_WIDTH;

    GRect offset_left = {
      .origin = { .x = base.origin.x - SCREEN_WIDTH, .y = base.origin.y },
      .size   = { .h = base.size.h  , .w = base.size.w }
    };

    GRect offset_right = {
      .origin = { .x = base.origin.x + SCREEN_WIDTH, .y = base.origin.y },
      .size   = { .h = base.size.h  , .w = base.size.w }
    };

    if( data->animating_out || data->animating_in )
    {
      animation_unschedule( (Animation*)data->animation );
    }

    data->animation_mode = mode;
    switch( mode )
    {
      case MovieTextUpdateNone:
      case MovieTextUpdateDelay:
        {
          strncpy( data->buf_l, text, sizeof( data->buf_l ) );

          layer_set_frame( (Layer*)layer, base );
        }
        break;

      case MovieTextUpdateInstant:
        {
          strncpy( data->buf_l, text, sizeof( data->buf_l ) );

          layer_set_frame( (Layer*)layer, base );
          layer_mark_dirty( (Layer*)layer );
        }
        break;

      case MovieTextUpdateSlideRight:
        {
          // buf_l links außerhalb des fensters platzieren, dann "einfliegen"
          // buf_l => neuer text
          // buf_r => alter text
          data->buf_r = (char*)text;

          // take changed origin into account
          data->animating_out = true;
          data->animation->values.from.grect = base;
          data->animation->values.to.grect   = offset_right;

          //layer_set_frame( (Layer*)layer, offset_left );
          animation_set_delay( (Animation*)data->animation, delay ? 100 : 0 );
          animation_set_duration( (Animation*)data->animation, 500 );
          animation_schedule( (Animation*)data->animation );
        }
        break;

      case MovieTextUpdateSlideLeft:
        {
          // buf_r rechts außerhalb des fensters platzieren, dann "einfliegen"
          // buf_l => alter text
          // buf_r => neuer text
          data->buf_r = (char*)text;

          data->animating_out = true;
          data->animation->values.from.grect = base;
          data->animation->values.to.grect   = offset_left;

          //layer_set_frame( (Layer*)layer, offset_right );
          animation_set_delay( (Animation*)data->animation, delay ? 100 : 0 );
          animation_set_duration( (Animation*)data->animation, 500 );
          animation_schedule( (Animation*)data->animation );
        }
        break;
    }
  } )
}

void movie_text_layer_set_origin( MovieTextLayer* layer, GPoint origin, 
                                  MovieTextUpdateMode mode, bool delay )
{
  //FIXME: this creates a permanent allocaton of 28B.. no Idea how or where..
  with_movie_layer( layer, data,
  {
    GRect base = {
      .origin = origin,
      .size = { .h = layer_get_frame( (Layer*)layer ).size.h, .w = SCREEN_WIDTH }
    };

    if( ( data->animating_out || data->animating_in ) && mode != MovieTextUpdateDelay )
    {
      animation_unschedule( (Animation*)data->animation );
    }

    data->origin = origin;
    data->animation_mode = MovieTextUpdateNone;

    switch( mode )
    {
      case MovieTextUpdateNone:
      {
        layer_set_frame( (Layer*)layer, base );
        layer_mark_dirty( (Layer*)layer );
        break;
      }

      case MovieTextUpdateInstant:
      case MovieTextUpdateSlideLeft:
      case MovieTextUpdateSlideRight:
      {
        data->animating_in = true;
        data->animating_out = false;
        data->animation->values.from.grect = layer_get_frame( (Layer*)layer );
        data->animation->values.to.grect   = base;

        animation_set_delay( (Animation*)data->animation, delay ? 100 : 0 );
        animation_set_duration( (Animation*)data->animation, 1000 );
        animation_schedule( (Animation*)data->animation );
        break;
      }

      case MovieTextUpdateDelay:
        data->origin = origin;
        break;
    }
  } );
}

void movie_text_layer_set_font( MovieTextLayer* layer, GFont font )
{
  with_movie_layer( layer, data, { data->font = font; } );
}

GColor movie_text_layer_get_text_color( MovieTextLayer* layer )
{
  with_movie_layer( layer, data, { return data->fg; } );
  return GColorClear;
}

GColor movie_text_layer_get_background_color( MovieTextLayer* layer )
{
  with_movie_layer( layer, data, { return data->bg; } );
  return GColorClear;
}

const char* movie_text_layer_get_text( MovieTextLayer* layer )
{
  with_movie_layer( layer, data,
  {
    if( data->animating_out || data->animating_in )
    {
      switch( data->animation_mode )
      {
        case MovieTextUpdateNone:
        case MovieTextUpdateDelay:
        case MovieTextUpdateInstant:
        case MovieTextUpdateSlideLeft:
        case MovieTextUpdateSlideRight:
          return data->buf_r;
      }
    }
    return data->buf_l;
  } )
  return "(null)";
}

GPoint movie_textLayer_get_origin( MovieTextLayer* layer )
{
  with_movie_layer( layer, data, { return data->origin; } );
  return GPoint( -1, -1 );
}

GFont movie_text_layer_get_font( MovieTextLayer* layer )
{
  with_movie_layer( layer, data, { return data->font; } );
  return fonts_get_system_font( FONT_KEY_GOTHIC_14_BOLD );
}
