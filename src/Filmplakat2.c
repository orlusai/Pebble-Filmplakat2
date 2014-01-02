/* Filmplakat2 
 * - Ein deutsches Watchface für Pebble mit Kinoposter-Look -
 *
 * Originalidee "Filmplakat": 
 *    Peter Marquardt
 *    www.lastfuture.de
 *    https://github.com/lastfuture/Pebble-Watchface---Filmplakat
 *    http://www.german-pebblers.de/viewtopic.php?f=3&t=320&sid=2cb30196d78f1fe180115d9f6a387788
 *
 * Rewrite für SDK 2.0      :
 *    René Köcher <shirk@bitspin.org>
 *    www.bitspin.org
 *    https://github.com/Shirk/Pebble-Filmplakat2
 */

#include <pebble.h>
#include "movie_text_layer.h"

#define DEBUG 0

#if DEBUG
# define TRACE                  APP_LOG( APP_LOG_LEVEL_INFO, __FUNCTION__ );
# define APP_DBG( msg... )      APP_LOG( APP_LOG_LEVEL_DEBUG, ##msg )
#else
# define TRACE
# define APP_DBG( msg... )
#endif

#define TEST_DATE 0

#if TEST_DATE
static int32_t test_dates[] = {
  1387040400, /* 17:00 */
  1387040460, /* 17:01 */
  1387040940, /* 17:09 */
  1387041000, /* 17:10 */
  1387041540, /* 17:19 */
  1387041600, /* 17:20 */
  1387041660, /* 17:21 */
  1386978600, /* 23:50 */
  1386978660, /* 23:51 */
  1386979140, /* 23:59 */
  1387065600, /* 00:00 */
  1387065660, /* 00:01 */
};

static int32_t test_date_pos = 0;
static AppTimer *test_date_timer = 0;
#endif

// Gesamtzahl der Zeilen für Uhrzeit
#define NUM_ROWS 5


// Zeilenhöhen in Pixel
#define BASE_ROW_X 20
#define ROW1_HIGHT 28 + 2
#define ROW2_HIGHT 25 + 2
#define ROW3_HIGHT 28 + 2
#define DATE_HIGHT 36 + 2
#define DOTLESS_X  5

#define ROW_STD_HIGHT 40
#define ROW_MAX_HIGHT 50

#define SCREEN_HIGHT 168
#define SCREEN_WIDTH 144

#define ROW_BUF_SIZE 20

// Storage-Keys
enum PersistantSettings
{
  SETTINGS_INVERTER_STATE = 1,
  SETTINGS_STATUS_VISIBLE = 2,
  SETTINGS_ACCEL_CONFIG   = 3
};

// Status der einzelnen Zeilen
enum RowState
{
    ROW_STATE_KEEP      = 0,
    ROW_STATE_DISAPPEAR = 1,
    ROW_STATE_REAPPEAR  = 2,
    ROW_STATE_STAYDOWN  = 3
};

// Textdaten
struct DisplayData
{
  char    *str;
  char    *str_dotless;
  uint8_t is_asc;
} __attribute__((__packed__));

static const struct DisplayData TENS[] = {
  { "zwanzig" , "zwanzıg", 0 },
  { "dreissig", ""       , 1 },
  { "vierzig" , "vıerzıg", 0 },
  { "fünfzig" , ""       , 1 },
  { "sechzig" , ""       , 1 }
};

static const struct DisplayData TEENS[] = {
  { "null"    , ""        , 1 },
  { "ein"     , "eın"     , 0 },
  { "zwei"    , "zwei"    , 0 },
  { "drei"    , ""        , 1 },
  { "vier"    , "vıer"    , 0 },
  { "fünf"    , ""        , 1 },
  { "sechs"   , "sechs"   , 0 },
  { "sieben"  , "sıeben"  , 0 },
  { "acht"    , ""        , 1 },
  { "neun"    , "neun"    , 0 },
  { "zehn"    , ""        , 1 },
  { "elf"     , ""        , 1 },
  { "zwölf"   , "zwölf"   , 0 },
  { "dreizehn", ""        , 1 },
  { "vierzehn", "vıerzehn", 0 },
  { "fünfzehn", ""        , 1 },
  { "sechzehn", "sechzehn", 0 },
  { "siebzehn", "sıebzehn", 0 },
  { "achtzehn", ""        , 1 },
  { "neunzehn", "neunzehn", 0 }
};

static const char* MONTHS[] = {
  "Januar",
  "Februar",
  "März",
  "April",
  "Mai",
  "Juni",
  "Juli",
  "August",
  "September",
  "Oktober",
  "November",
  "Dezember"
};

static const char* WEEKDAYS[] = {
  "So", "Mo", "Di", "Mi", "Do", "Fr", "Sa"
};

// GUI Objekte
static Window *window = 0;
static Layer* window_layer = 0;
static InverterLayer* inverter_layer = 0;

// Zeilen
static MovieTextLayer* row[NUM_ROWS];

// Statusbalken
static Layer *status_layer = 0;
static InverterLayer *charge_layer = 0;
static GBitmap *icon_bt_on = 0, *icon_bt_off = 0;

static bool status_bluetooth_conn = false;
static BatteryChargeState status_battery_charge = { 
  .charge_percent = 100,
  .is_charging    = false,
  .is_plugged     = true
};

static GFont font_uhr, font_hour, font_minutes, font_date, font_charge;

// Puffer für aktive / alte Zeileninhalte
static char row_cur_data[NUM_ROWS][ROW_BUF_SIZE];
static uint8_t row_cur_cnt, row_old_cnt;

// aktive / alte Layerpositionen
static GPoint row_cur_pos[NUM_ROWS],
              row_old_pos[NUM_ROWS];

static uint8_t first_update = 1;

// Timer zum deaktivieren der Gestenerkennung
static AppTimer *accel_config_timer = 0;

// Konfigwerte
static AppSync app;
static uint8_t appsync_buffer[64];
static bool settings_inverter_state = false;
static bool settings_status_visible = false;
static bool settings_accel_config   = true;

static void copy_time( uint8_t *is_ascii, uint8_t *ten_and_mark )
{
  TRACE

  int hours, minutes, tens, ones;

#if TEST_DATE
  struct tm* now = localtime( &test_dates[test_date_pos++]); 

  if( test_date_pos > 10 )
  {
    test_date_pos = 0;
  }
#else
  int32_t time_val = time( NULL );
  struct tm* now = localtime( &time_val );
#endif

  row_cur_cnt = 0;

  is_ascii[row_cur_cnt] = 0;

  snprintf( row_cur_data[row_cur_cnt++], ROW_BUF_SIZE, "%s %d. %s",
            WEEKDAYS[now->tm_wday], (int)now->tm_mday, MONTHS[now->tm_mon] );

  hours   = now->tm_hour % 12;
  minutes = now->tm_min;

  if( now->tm_hour == 12 )
  {
    hours = 12;
  }

  is_ascii[row_cur_cnt] = TEENS[hours].is_asc;
  snprintf( row_cur_data[ row_cur_cnt++ ], ROW_BUF_SIZE, " %s", TEENS[hours].str );
  snprintf( row_cur_data[ row_cur_cnt++ ], ROW_BUF_SIZE, "uhr" );

  if( ten_and_mark )
  {
    *ten_and_mark = 0;
  }
  if( minutes == 0 )
  {
    // pass..
  }
  else if( minutes < 20 )
  {
    is_ascii[row_cur_cnt] = TEENS[minutes].is_asc;
    if( is_ascii[row_cur_cnt] == 0 )
    {
      strncpy( row_cur_data[row_cur_cnt], TEENS[minutes].str_dotless,
               ROW_BUF_SIZE );
    }
    else
    {
      strncpy( row_cur_data[row_cur_cnt], TEENS[minutes].str,
               ROW_BUF_SIZE );
    }
    if(minutes == 1)
    {
      strncat( row_cur_data[row_cur_cnt], "s", ROW_BUF_SIZE );
    }
    ++row_cur_cnt;
  }
  else
  {
    tens = minutes / 10;
    ones = minutes % 10;

    if( ones == 0 )
    {
      is_ascii[row_cur_cnt] = TENS[tens - 2].is_asc;
      if( is_ascii[row_cur_cnt] == 0 && tens != 2 )
      {
        strncpy( row_cur_data[row_cur_cnt++], TENS[tens-2].str_dotless,
                 ROW_BUF_SIZE );
      }
      else
      {
        strncpy( row_cur_data[row_cur_cnt++], TENS[tens-2].str,
                 ROW_BUF_SIZE );
      }
    }
    else
    {
      if( ones == 1 )
      {
        if( ten_and_mark )
        {
          *ten_and_mark = 1;
        }
      }

      is_ascii[row_cur_cnt] = TEENS[ones].is_asc;
      if( is_ascii[row_cur_cnt] == 0 )
      {
        strncpy( row_cur_data[row_cur_cnt], TEENS[ones].str_dotless,
                 ROW_BUF_SIZE );
      }
      else
      {
        strncpy( row_cur_data[row_cur_cnt], TEENS[ones].str,
                 ROW_BUF_SIZE ); 
      }
      strncat( row_cur_data[row_cur_cnt], "und", ROW_BUF_SIZE );
      ++row_cur_cnt;

      is_ascii[row_cur_cnt] = TENS[tens-2].is_asc;
      if( is_ascii[row_cur_cnt] == 0 )
      {
        strncpy( row_cur_data[row_cur_cnt++], TENS[tens-2].str_dotless,
                 ROW_BUF_SIZE );
      }
      else
      {
        strncpy( row_cur_data[row_cur_cnt++], TENS[tens-2].str,
                 ROW_BUF_SIZE ); 
      }
    }
  }
}

static void update_if_needed( MovieTextLayer *row, const char* row_buf,
                              GPoint* old_pos, GPoint* new_pos )
{
  TRACE

  if( strcmp( row_buf, movie_text_layer_get_text( row ) ) )
  {
    if( new_pos->y != old_pos->y )
    {
      movie_text_layer_set_origin( row, *new_pos, MovieTextUpdateDelay, false );
    }
    movie_text_layer_set_text( row, row_buf, MovieTextUpdateSlideThrough, false );
  }
  else
  {
      movie_text_layer_set_origin( row, *new_pos, MovieTextUpdateInstant, false );
  }
}

static void update_and_move( MovieTextLayer *row1, const char* row1_text, GPoint *row1_pos,
                             MovieTextLayer *row2, const char* row2_text, GPoint *row2_pos )
{
  TRACE

  movie_text_layer_set_origin( row1, *row1_pos, MovieTextUpdateDelay, false );
  movie_text_layer_set_text( row1, row1_text, MovieTextUpdateSlideThrough, false );

  if( row2 && row2_text && row2_pos )
  {
    movie_text_layer_set_origin( row2, *row2_pos, MovieTextUpdateDelay, false );
    movie_text_layer_set_text( row2, row2_text, MovieTextUpdateSlideThrough, false );
  }
}

static void update_rows( void )
{
  TRACE

  int base_offset_y = 0, offset_y = 0, i;
  uint8_t is_asc[NUM_ROWS], ten_and_mark = 0;

  // alte Zeileninhalte / Positionen speichern
  memcpy( row_old_pos, row_cur_pos, sizeof( GPoint ) * row_cur_cnt );
  row_old_cnt = row_cur_cnt;

  memset( row_cur_pos, 0, sizeof( row_cur_pos ) );
  memset( row_cur_data, 0, sizeof( row_cur_data ) );
  row_cur_cnt = 0;

  memset( is_asc, 0, sizeof( is_asc ) );

  copy_time( is_asc, &ten_and_mark );

  row_cur_pos[0].x = row_cur_pos[1].x = row_cur_pos[2].x = 
  row_cur_pos[3].x = row_cur_pos[4].x = BASE_ROW_X;

  /* row[1] - immer die Stunde */
  row_cur_pos[1].y = base_offset_y;
  /* row[2] - immer 'uhr' */
  row_cur_pos[2].y = ( base_offset_y += ROW1_HIGHT );

  /* row[3] / row[4] - minuten */
  if( row_cur_cnt >= 4 )
  {
    row_cur_pos[3].y = ( base_offset_y += ( ROW2_HIGHT - (is_asc[3] ? 0 : DOTLESS_X) ) );
  }
  if( row_cur_cnt == 5 )
  {
    row_cur_pos[4].y = ( base_offset_y += ( ROW3_HIGHT - (is_asc[4] ? 0 : DOTLESS_X) ) );
  }
  /* row[0] - immer das Datum */
  row_cur_pos[0].y = ( base_offset_y += DATE_HIGHT );
  
  base_offset_y += 22; // warum 22? Puffer??
  offset_y = ( SCREEN_HIGHT - base_offset_y ) / 2;

  /* finale positionen */
  for( i = 0; i < row_cur_cnt; ++i )
  {
    row_cur_pos[i].x -= row_cur_pos[i].y / 5;
    row_cur_pos[i].y += offset_y;
  }
  row_cur_pos[0].x += 4; // Datum weiter nach Rechts
  row_cur_pos[1].x -= 7; // Leerzeichen vor jeder Stunde ausgleichen

  if( first_update )
  {
    // Neustart des Watchface
    row_old_cnt = row_cur_cnt;
    memcpy( row_old_pos, row_cur_pos, sizeof( row_cur_pos ) );

    for( i = 0; i < NUM_ROWS; ++i )
    {
      if( i < row_cur_cnt )
      {
        movie_text_layer_set_origin( row[i], row_cur_pos[i], MovieTextUpdateNone, false );
      }
    }

    ten_and_mark = 0;
  }

  //
  // TextLayer setzen und animieren
  //

  // stunden, immer da
  update_if_needed( row[1], row_cur_data[1], &row_old_pos[1], &row_cur_pos[1] );
  
  // 'uhr', immer da
  if( first_update )
  {
    first_update = 0;
    movie_text_layer_set_origin( row[2], row_cur_pos[2], MovieTextUpdateDelay, false );
    movie_text_layer_set_text( row[2], row_cur_data[2], MovieTextUpdateSlideThrough, false );
  }
  else
  {
    movie_text_layer_set_text( row[2], row_cur_data[2], MovieTextUpdateInstant, false );
    movie_text_layer_set_origin( row[2], row_cur_pos[2], MovieTextUpdateInstant, false );
  }

  // Datum, immer da
  update_if_needed( row[0], row_cur_data[0], &row_old_pos[0], &row_cur_pos[0] );

  // Bewegte zeile - 3 / 4 für Minuten
  //
  // Möglich kombinationen:
  //
  // XX Uhr    -> XX Uhr  1: cnt 3 -> 4
  // XX Uhr 19 -> XX Uhr 20: cnt 4 -> 5  'einund / zwanzig'
  // XX Uhr 29 -> XX Uhr 30: cnt 5 -> 4
  // XX Uhr 59 -> XX Uhr   : cnt 5 -> 3
  // keine Änderung ->     : cnt == old_cnt
  //
  if( row_old_cnt == 3 && row_cur_cnt == 4 )
  {
    // Stunde -> Stunde + Minute
    update_and_move( row[3], row_cur_data[3], &row_cur_pos[3], NULL, NULL, NULL );
  }
  else if( row_old_cnt == 4 && row_cur_cnt == 5 )
  {
    // Stunde + Minute -> Stunde + Minuten + Zehner
    update_and_move( row[3], row_cur_data[3], &row_cur_pos[3],
                     row[4], row_cur_data[4], &row_cur_pos[4] );
  }
  else if( row_old_cnt == 5 && row_cur_cnt == 4 )
  {
    // Stunde + Minute + Zehner -> Stunde + Minute
    update_and_move( row[3], "", &row_cur_pos[3],
                     row[4], row_cur_data[3], &row_cur_pos[3] );
  }
  else if( row_old_cnt == 5 && row_cur_cnt == 3 )
  {
    // Stunde + Minute + Zehner -> Stunde
    update_and_move( row[3], "", &row_cur_pos[3],
                     row[4], "", &row_cur_pos[4] );
  }
  else if( row_old_cnt == row_cur_cnt )
  {
    // nur update, keine Zeilenänderung
    if( row_old_cnt == 3 )
    {
      movie_text_layer_set_text( row[3], "", MovieTextUpdateInstant, false );
      movie_text_layer_set_text( row[4], "", MovieTextUpdateInstant, false );
    }
    if( row_old_cnt >= 4 )
    {
      update_if_needed( row[3], row_cur_data[3], &row_old_pos[3], &row_cur_pos[3] );
    }

    if( row_old_cnt == 5 )
    {
      update_if_needed( row[4], row_cur_data[4], &row_old_pos[4], &row_cur_pos[4] );
    }
  }
}

static void update_status( struct Layer *layer, GContext *ctx )
{
  //TRACE
  char batt_text[5] = "\0\0\0\0\0";
  // FIXME: BETA3 liefert nur werte von 10-90, nie 100
  int  batt_charge = 10 + (int)status_battery_charge.charge_percent;

  GRect batt_outline = GRect( SCREEN_WIDTH - 22, 2, 20, 11 );
  GRect batt_label   = GRect( SCREEN_WIDTH - 22, 2, 20, 10 );

  GRect batt_fill    = GRect( batt_outline.origin.x + 2,
                              batt_outline.origin.y + 2,
                              16 * batt_charge / 100, 
                              batt_outline.size.h - 4 );

  GRect bluetooth_icon = GRect(2, 2, 12, 13);

  graphics_context_set_stroke_color( ctx, GColorWhite );
  graphics_context_set_fill_color( ctx, GColorBlack );
  graphics_context_set_text_color( ctx, GColorWhite );
  
  graphics_fill_rect( ctx, batt_outline, 0, GCornerNone );
  graphics_draw_rect( ctx, batt_outline );

  if( batt_charge > 0 )
  {
    snprintf( batt_text, 4, "%d%c", batt_charge, status_battery_charge.is_charging ? '+':'\0' );
    graphics_draw_text( ctx, batt_text, font_charge, batt_label,
                        GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL );
  }

  // Die "Füllung" der Batterie wird via Invertieren realisiert
  // So kann auch der Text teilinvers dargestellt werden.
  layer_set_frame( inverter_layer_get_layer( charge_layer ), batt_fill );

  graphics_draw_bitmap_in_rect(ctx, status_bluetooth_conn ? icon_bt_on
                                                          : icon_bt_off,
                                                            bluetooth_icon );
}

static void toggle_view_setting( Layer *layer, int storage_key, bool *value )
{
  TRACE

  (*value) = !(*value);

  layer_set_hidden( layer , !(*value) );
  persist_write_bool( storage_key, *value );
}

//
// Eventbearbeitung
//

#if TEST_DATE

static void on_test_date_tick( void *data __attribute__((__unused__)) )
{
  update_rows();
  test_date_timer = app_timer_register( 5000, on_test_date_tick, NULL );
}

#else

static void on_minute_tick( struct tm *time_ticks __attribute__((__unused__)),
                            TimeUnits units_changed __attribute__((__unused__)) )
{
  TRACE

  update_rows();
}

#endif

static void on_tap_gesture( AccelAxisType axis, int32_t direction )
{
  TRACE

  switch( axis )
  {
    case ACCEL_AXIS_X:
      APP_DBG( "on_tap_gesture( X, %ld );", direction );
      break;

    case ACCEL_AXIS_Y:
      APP_DBG( "on_tap_gesture( Y, %ld );", direction );
      toggle_view_setting( status_layer, SETTINGS_STATUS_VISIBLE, &settings_status_visible );
      break;

    case ACCEL_AXIS_Z:
      APP_DBG( "on_tap_gesture( Z, %ld );", direction );
      toggle_view_setting( inverter_layer_get_layer( inverter_layer ), 
                           SETTINGS_INVERTER_STATE, &settings_inverter_state );
      break;
  }
}

static void on_tap_timeout( void *data __attribute__((__unused__)) )
{
  TRACE

  accel_tap_service_unsubscribe();
}

static void on_battery_change( BatteryChargeState charge )
{
  TRACE
  
  if( settings_status_visible )
  {
    status_battery_charge = charge;
    layer_mark_dirty( status_layer );
  }
  if( !charge.is_charging && charge.charge_percent == 10 )
  {
    vibes_short_pulse();
  }
}

static void on_bluetooth_change( bool connected )
{
  TRACE

  if( settings_status_visible )
  {
    status_bluetooth_conn = connected;
    layer_mark_dirty( status_layer );
  }
  if( !connected )
  {
    vibes_double_pulse();
  }
}

//
// Remote-Konfiguration
//
static void on_conf_keys_changed( const uint32_t key, const Tuple *tp_new,
                                  const Tuple *tp_old __attribute__((__unused__)),
                                  void *ctx __attribute__((__unused__)) )
{
  TRACE

  bool value = (bool)tp_new->value->uint8;

  switch( key )
  {
    case SETTINGS_INVERTER_STATE: /* FALL_THROUGH */
      {
        settings_inverter_state = !value;
        toggle_view_setting( inverter_layer_get_layer( inverter_layer ),
                             SETTINGS_INVERTER_STATE, 
                             &settings_inverter_state );
      }
      break;

    case SETTINGS_STATUS_VISIBLE:
      {
        settings_status_visible = !value;
        toggle_view_setting( status_layer, 
                             SETTINGS_STATUS_VISIBLE,
                             &settings_status_visible );
      }
      break;

    case SETTINGS_ACCEL_CONFIG:
      {
        settings_accel_config = value;
        persist_write_bool( SETTINGS_ACCEL_CONFIG, settings_accel_config );
      }
      break;
  }
}
static void on_app_message_error( DictionaryResult dict_error __attribute__((__unused__)),
                                  AppMessageResult app_message_error,
                                  void* ctx __attribute__((__unused__)) )
{
  TRACE

  APP_LOG( APP_LOG_LEVEL_ERROR, "App error: %d", app_message_error );
}

static void app_config_send_keys( void )
{
  TRACE

  DictionaryIterator* it = NULL;
  app_message_outbox_begin( &it );

  if( it == NULL )
  {
    return;
  }

  dict_write_uint8( it, SETTINGS_INVERTER_STATE, ( settings_inverter_state ? 1 : 0 ) );
  dict_write_uint8( it, SETTINGS_STATUS_VISIBLE, ( settings_status_visible ? 1 : 0 ) );
  dict_write_uint8( it, SETTINGS_ACCEL_CONFIG  , ( settings_accel_config   ? 1 : 0 ) );

  dict_write_end( it );
  app_message_outbox_send();
}

static void app_config_init( void )
{
  TRACE

  Tuplet persistent_keys[] = {
    TupletInteger( SETTINGS_INVERTER_STATE, ( settings_inverter_state ? 1 : 0 ) ),
    TupletInteger( SETTINGS_STATUS_VISIBLE, ( settings_status_visible ? 1 : 0 ) ),
    TupletInteger( SETTINGS_ACCEL_CONFIG  , ( settings_accel_config   ? 1 : 0 ) )
  };

  app_message_open( 64, 64 );
  app_sync_init( &app, appsync_buffer, sizeof( appsync_buffer ), 
                 persistent_keys, ARRAY_LENGTH( persistent_keys ),
                 on_conf_keys_changed, on_app_message_error, NULL );

  app_config_send_keys();
}

static void app_config_deinit( void )
{
  TRACE

  app_sync_deinit( &app );
}

//
//
// Setup
//

static void window_load(Window *window)
{
  TRACE

  int i; // immer gut ein 'i' zu haben
  
  window_layer = window_get_root_layer( window );
  GRect window_frame = layer_get_frame( window_layer );
  GRect row_frame = GRect( 0, 0, SCREEN_WIDTH, ROW_MAX_HIGHT );
  GRect status_bar_rect = GRect( 0, 0, SCREEN_WIDTH, 20 );

  // Datumszeilen
  GFont font_map[] = {
    font_date,
    font_hour,
    font_uhr,
    font_minutes,
    font_minutes
  };

  for( i = 0; i < NUM_ROWS; ++i )
  {
    row[i] = movie_text_layer_create( row_frame.origin, ROW_STD_HIGHT );

    movie_text_layer_set_text_color( row[i], GColorWhite );
    movie_text_layer_set_background_color( row[i], GColorClear );
    movie_text_layer_set_font( row[i], font_map[i] );

    layer_add_child( window_layer, movie_text_layer_get_layer( row[i] ) );
  }

  // Inverter, Statusbalken & Ladezustandslayer  
  status_layer = layer_create( status_bar_rect );  
  charge_layer = inverter_layer_create( GRectZero );

  layer_set_update_proc( status_layer, update_status );
  layer_set_hidden( status_layer, !settings_status_visible );
  layer_add_child( status_layer, inverter_layer_get_layer( charge_layer ) );
  layer_add_child( window_layer, status_layer );

  // Inverter als letztes (und somit kein layer_insert_below_sibling calls)
  inverter_layer = inverter_layer_create( window_frame );
  layer_set_hidden( inverter_layer_get_layer( inverter_layer), !settings_inverter_state );
  layer_add_child( window_layer, inverter_layer_get_layer( inverter_layer ) );

  if( settings_accel_config )
  {
    accel_config_timer = app_timer_register( 5000, on_tap_timeout, NULL );
    accel_tap_service_subscribe( on_tap_gesture );
  }

  battery_state_service_subscribe( on_battery_change );
  bluetooth_connection_service_subscribe( on_bluetooth_change );

#if TEST_DATE
  test_date_timer = app_timer_register( 5000, on_test_date_tick, NULL );
#else
  tick_timer_service_subscribe( MINUTE_UNIT, on_minute_tick );
#endif
}

static void window_unload(Window *window)
{
  TRACE

  int i;

  inverter_layer_destroy( inverter_layer );
  inverter_layer_destroy( charge_layer );
  layer_destroy( status_layer );

  for( i = 0; i < NUM_ROWS; ++i )
  {
    if( row[i] != NULL )
    {
      movie_text_layer_destroy( row[i] );
      row[i] = NULL;
    }
  }

  fonts_unload_custom_font( font_hour );
  fonts_unload_custom_font( font_minutes );
  fonts_unload_custom_font( font_date );
  fonts_unload_custom_font( font_uhr );
  fonts_unload_custom_font( font_charge );

  gbitmap_destroy( icon_bt_on );
  gbitmap_destroy( icon_bt_off );
}

static void init(void)
{
  TRACE

  first_update = 1;
  if( persist_exists( SETTINGS_INVERTER_STATE ) )
  {
    settings_inverter_state = persist_read_bool( SETTINGS_INVERTER_STATE );
  }

  if( persist_exists( SETTINGS_STATUS_VISIBLE ) )
  {
    settings_status_visible = persist_read_bool( SETTINGS_STATUS_VISIBLE );
  }

  if( persist_exists( SETTINGS_ACCEL_CONFIG ) )
  {
    settings_accel_config = persist_read_bool( SETTINGS_ACCEL_CONFIG );
  }

  window = window_create();

  window_set_fullscreen( window, true );
  window_set_background_color( window, GColorBlack );
  window_set_window_handlers( window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });

  font_hour    = fonts_load_custom_font( resource_get_handle( RESOURCE_ID_FONT_ROBOTO_BOLD_35 ) );
  font_minutes = fonts_load_custom_font( resource_get_handle( RESOURCE_ID_FONT_ROBOTO_REGULAR_33 ) );
  font_uhr     = fonts_load_custom_font( resource_get_handle( RESOURCE_ID_FONT_ROBOTO_LIGHT_30 ) );
  font_date    = fonts_load_custom_font( resource_get_handle( RESOURCE_ID_FONT_ROBOTO_REGULAR_13 ) );
  font_charge  = fonts_load_custom_font( resource_get_handle( RESOURCE_ID_FONT_ROBOTO_REGULAR_9 ) );

  icon_bt_on  = gbitmap_create_with_resource( RESOURCE_ID_IMAGE_BT_ON_ICON );
  icon_bt_off = gbitmap_create_with_resource( RESOURCE_ID_IMAGE_BT_OFF_ICON );

  memset( row_cur_data, 0, sizeof( row_cur_data ) );
  memset( row_cur_pos, 0, sizeof( row_cur_pos ) );
  memset( row_old_pos, 0, sizeof( row_old_pos ) );

  status_battery_charge = battery_state_service_peek();
  status_bluetooth_conn = bluetooth_connection_service_peek();

  app_config_init();

  // als letztes damit alle resourcen initialisiert sind
  window_stack_push( window, true /*animated*/ );
}

static void deinit(void)
{
  TRACE

  app_config_deinit();

  accel_tap_service_unsubscribe();
  battery_state_service_unsubscribe();
  bluetooth_connection_service_unsubscribe();

#if !TEST_DATE
  tick_timer_service_unsubscribe();
#endif

  window_destroy( window );
}

int main(void) 
{
  TRACE

  init();
  app_event_loop();
  deinit();
}
