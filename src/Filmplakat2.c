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

#define DEBUG 0

#if DEBUG
# define TRACE                  APP_LOG( APP_LOG_LEVEL_INFO, __FUNCTION__ );
# define APP_DBG( msg... )      APP_LOG( APP_LOG_LEVEL_DEBUG, ##msg )
#else
# define TRACE
# define APP_DBG( msg... )
#endif

// Gesamtzahl der Zeilen für Uhrzeit
#define NUM_ROWS       5
// Anzahl der beweglichen Zeilen
#define NUM_SHIFT_ROWS 4

// Zeilenhöhen in Pixel
#define BASE_ROW_X 20
#define ROW1_HIGHT 28
#define ROW2_HIGHT 25
#define ROW3_HIGHT 28
#define DATE_HIGHT 36
#define DOTLESS_X  5

#define ROW_STD_HIGHT 36
#define ROW_MAX_HIGHT 50

#define SCREEN_HIGHT 168
#define SCREEN_WIDTH 144

#define ROW_BUF_SIZE 20

// Storage-Keys
enum PersistantSettings
{
  SETTINGS_INVERTER_STATE = 1,
  SETTINGS_STATUS_VISIBLE = 2
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
static TextLayer* row[NUM_ROWS + NUM_SHIFT_ROWS];
static PropertyAnimation* animations[NUM_ROWS + NUM_SHIFT_ROWS];

// Statusbalken
static Layer *status_layer = 0;
static InverterLayer *charge_layer = 0;
static GBitmap *icon_bt_on = 0, *icon_bt_off = 0;

static GFont fontUhr, fontHour, fontMinutes, fontDate, fontCharge;

// Puffer für aktive / alte Zeileninhalte
static char row_cur_data[NUM_ROWS][ROW_BUF_SIZE],
            row_old_data[NUM_ROWS][ROW_BUF_SIZE];

static uint8_t row_cur_cnt, row_old_cnt;

// aktive / alte Layerpositionen
static GPoint row_cur_pos[NUM_ROWS],
              row_old_pos[NUM_ROWS];

static uint8_t first_update = 1;

// Timer zum deaktivieren der Gestenerkennung
static AppTimer *accel_config_timer = 0;

// Konfigwerte
static bool settings_inverter_state = false;
static bool settings_status_visible = false;

static PropertyAnimation* setup_animation( Layer* layer, GRect *old_rect, GRect *new_rect )
{
  TRACE

  int i;
  for( i = 0; i < NUM_ROWS + NUM_SHIFT_ROWS; ++i )
  {
    if( animations[i] == NULL )
    {
      animations[i] = property_animation_create_layer_frame( layer, old_rect, new_rect );
      return animations[i];
    }
  }

  return NULL;
}

static void schedule_animations( void )
{
  TRACE

  int i, cnt;
  for( i = 0, cnt = 0; i < NUM_ROWS + NUM_SHIFT_ROWS; ++i )
  {
    if( animations[i] != NULL )
    {
      animation_schedule( &animations[i]->animation );
      ++cnt;
    }
  }

  APP_DBG( "scheduled %d animations.", cnt );
}

static void cleanup_animations( void )
{
  TRACE

  int i, cnt;
  for( i = 0, cnt = 0; i < NUM_ROWS + NUM_SHIFT_ROWS; ++i )
  {
    if( animations[i] != NULL )
    {
      property_animation_destroy( animations[i] );
      animations[i] = NULL;
      ++cnt;
    }
  }

  APP_DBG( "deleted %d animations.", cnt );
}

static void setup_text_layer(TextLayer* row, GPoint new_pos, GPoint old_pos, 
                             GFont font, int state, int delay, int black)
{
  TRACE

  int speed       = 500,
      distance    = ( old_pos.y - new_pos.y ),
      rect_height = ( ( black ) ? ROW_STD_HIGHT : ROW_MAX_HIGHT );

  GRect start_rect, target_rect;

  text_layer_set_text_color( row, GColorWhite );
  text_layer_set_background_color( row, ( black ) ? GColorBlack : GColorClear );
  text_layer_set_font( row, font );

  layer_add_child( window_layer, text_layer_get_layer( row ) );
  layer_insert_below_sibling( text_layer_get_layer( row ), inverter_layer_get_layer( inverter_layer ) );

  if( distance < 0 )
  {
    distance *= -1;
  }

  if( first_update )
  {
    speed = 600;
  }
  else if( new_pos.x == -SCREEN_WIDTH )
  {
    speed = 1400;
  }
  else if( old_pos.x == SCREEN_WIDTH )
  {
    speed = 1000;
  }

  switch( state )
  {
    case ROW_STATE_KEEP:
      start_rect = GRect( old_pos.x, old_pos.y, SCREEN_WIDTH - old_pos.x - 1, rect_height );
      target_rect = GRect( new_pos.x, new_pos.y, SCREEN_WIDTH - new_pos.x - 1, rect_height );
      break;

    case ROW_STATE_DISAPPEAR:
      start_rect = GRect( old_pos.x, old_pos.y, SCREEN_WIDTH - old_pos.x - 1, rect_height );
      target_rect = GRect( -SCREEN_WIDTH, old_pos.y, SCREEN_WIDTH - new_pos.x - 1, rect_height );
      break;

    case ROW_STATE_REAPPEAR:
      start_rect = GRect( SCREEN_WIDTH, new_pos.y, SCREEN_WIDTH - new_pos.x - 1, rect_height );
      target_rect = GRect( new_pos.x, new_pos.y, SCREEN_WIDTH - new_pos.x - 1, rect_height );
      break;

    case ROW_STATE_STAYDOWN:
      start_rect = GRect( 0, 0, 0, 0 );
      target_rect = GRect( 0, 0, 0, 0 );
      speed = 1;
      break;
  }

  if( ROW_STATE_STAYDOWN != state )
  {
    PropertyAnimation *animation = NULL;

    layer_set_frame( text_layer_get_layer( row ), start_rect );
    
    if( memcmp( &start_rect, &target_rect, sizeof( GRect ) ) == 0 )
    {
      APP_DBG( "skipping row animation." );
      return;
    }

    animation = setup_animation( text_layer_get_layer( row ), NULL, &target_rect );

    if( animation != NULL )
    {
      animation_set_duration( &animation->animation, speed );
      animation_set_curve( &animation->animation, AnimationCurveEaseInOut );

      if( delay )
      {
        animation_set_delay( &animation->animation, 100 );
      }
    }
    else
    {
      //FIXME: OOM
      APP_LOG( APP_LOG_LEVEL_ERROR, "no free animation objects!");
    }
  }
}

static void copy_time( uint8_t *is_ascii, uint8_t *ten_and_mark )
{
  TRACE

  int hours, minutes, tens, ones;

  int32_t time_val = time( NULL );
  struct tm* now = localtime( &time_val );

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

static void update_rows( void )
{
  TRACE

#define CONTENTS_CHANGED( row_num ) ( strcmp( row_cur_data[( row_num )], row_old_data[( row_num )] ) )

  int base_offset_y = 0, offset_y = 0, row_state, i;
  uint8_t is_asc[NUM_ROWS], ten_and_mark = 0;

  // alte Zeileninhalte / Positionen speichern
  memcpy( row_old_pos, row_cur_pos, sizeof( row_old_pos ) );
  memcpy( row_old_data, row_cur_data, sizeof( row_old_data ) );
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
  offset_y = (SCREEN_HIGHT/*166*/ - base_offset_y) / 2;

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
    // Neustart des Watchface, Zeilen abwechselnd von links / rechts einblenden
    int side_pos = -SCREEN_WIDTH;
    row_old_cnt = row_cur_cnt;
   
    memset( row_old_data[3], 0, sizeof( row_old_data[3] ) );
    memset( row_old_data[4], 0, sizeof( row_old_data[4] ) );
    memcpy( row_old_data[3], row_cur_data[3], sizeof( row_old_data[3] ) );
    memcpy( row_old_data[4], row_cur_data[4], sizeof( row_old_data[4] ) );

    for( i = 0; i < row_old_cnt; ++i )
    {
      row_old_pos[i].x = ( side_pos *= -1 );
      row_old_pos[i].y = row_cur_pos[i].y;
    }

    ten_and_mark = 0;
  }
  
  //
  // TextLayer setzen und animieren
  //
  row_state = ROW_STATE_KEEP;

  // "Uhr" Zeile, konstant
  setup_text_layer( row[2], row_cur_pos[2], row_old_pos[2], fontUhr, ROW_STATE_KEEP, 0, 0 );
  text_layer_set_text( row[2], row_cur_data[2] );

  // Stunden
  if( CONTENTS_CHANGED( 1 ) && !first_update )
  {
    // "normales" Update
    setup_text_layer( row[1], GPoint( -SCREEN_WIDTH, row_old_pos[1].y ), row_old_pos[1],
                              fontHour, ROW_STATE_KEEP, 0, 1 );
    setup_text_layer( row[6], row_cur_pos[1], GPoint( SCREEN_WIDTH, row_cur_pos[1].y ),
                              fontHour, ROW_STATE_KEEP, 1, 1 );

    text_layer_set_text( row[1], row_old_data[1] );
    text_layer_set_text( row[6], row_cur_data[1] );
  }
  else
  {
    // initiales update
    setup_text_layer( row[1], row_cur_pos[1], row_old_pos[1], fontHour, ROW_STATE_KEEP, 1, 1);

    text_layer_set_text( row[1], row_cur_data[1] );
    text_layer_set_text( row[6], " " );
  }

  // Minuten - Zeile 3
  if( row_cur_cnt - 1 >= 3 )
  {
    row_state = ( row_cur_cnt == row_old_cnt ) ? ROW_STATE_KEEP
                                               : ROW_STATE_REAPPEAR;
  }
  else
  {
    row_state = ( row_cur_cnt == row_old_cnt ) ? ROW_STATE_STAYDOWN
                                               : ROW_STATE_DISAPPEAR;
  }

  if( ROW_STATE_KEEP == row_state )
  {
    if( CONTENTS_CHANGED( 3 ) )
    {
      if( ten_and_mark )
      {
        setup_text_layer( row[3], row_cur_pos[3], GPoint( SCREEN_WIDTH, row_cur_pos[3].y ),
                                  fontMinutes, row_state, 1, 0 );

        text_layer_set_text( row[3], row_cur_data[3] );
        text_layer_set_text( row[7], " " );
      }
      else
      {
        setup_text_layer( row[3], GPoint( -SCREEN_WIDTH, row_old_pos[3].y ), row_old_pos[3],
                                  fontMinutes, row_state, 0, 0 );
        setup_text_layer( row[7], row_cur_pos[3], GPoint( SCREEN_WIDTH, row_cur_pos[3].y ),
                                  fontMinutes, row_state, 1, 0 );

        text_layer_set_text( row[3], row_old_data[3] );
        text_layer_set_text( row[7], row_cur_data[3] );
      }
    }
    else
    {
      // Position korrigieren wenn nötig
      setup_text_layer( row[3], row_cur_pos[3], row_old_pos[3], fontMinutes, row_state, 0, 0 );

      text_layer_set_text( row[3], row_cur_data[3] );
      text_layer_set_text( row[7], " " );
    }
  }
  else
  {
    // Ein- / Ausblenden
    setup_text_layer( row[3], row_cur_pos[3], row_old_pos[3], fontMinutes, row_state, 0, 0 );

    text_layer_set_text( row[3], ( row_state == ROW_STATE_DISAPPEAR ) ? row_old_data[3]
                                                                      : row_cur_data[3] );
    text_layer_set_text( row[7], " " );
  }

  // Minuten - Zeile 4
  if( row_cur_cnt - 1 >= 4 )
  {
    row_state = ( row_cur_cnt == row_old_cnt ) ? ROW_STATE_KEEP
                                               : ROW_STATE_REAPPEAR;
  }
  else
  {
    row_state = ( row_cur_cnt == row_old_cnt ) ? ROW_STATE_STAYDOWN
                                               : ROW_STATE_DISAPPEAR;
  }

  if( ten_and_mark )
  {
    // Einblenden
    setup_text_layer( row[4], row_cur_pos[4], row_old_pos[3], fontMinutes, ROW_STATE_KEEP, 0, 0);

    text_layer_set_text( row[4], row_cur_data[4] );
    text_layer_set_text( row[8], " " );
  }
  else if( ROW_STATE_KEEP == row_state )
  {
    if( CONTENTS_CHANGED( 4 ) )
    {
        setup_text_layer( row[4], GPoint( -SCREEN_WIDTH, row_old_pos[4].y ), row_old_pos[4],
                                  fontMinutes, row_state, 0, 0 );
        setup_text_layer( row[8], row_cur_pos[4], GPoint( SCREEN_WIDTH, row_cur_pos[4].y ),
                                  fontMinutes, row_state, 1, 0 );

        text_layer_set_text( row[4], row_old_data[4] );
        text_layer_set_text( row[8], row_cur_data[4] );
      }
      else
      {
        setup_text_layer( row[4], row_cur_pos[4], row_old_pos[4],
                                  fontMinutes, row_state, 0, 0 );

        text_layer_set_text( row[4], row_cur_data[4] );
        text_layer_set_text( row[8], " " );
      }
  }
  else
  {
    // Ausblenden / korrigieren
    setup_text_layer( row[4], row_cur_pos[4], row_old_pos[4], fontMinutes, row_state, 0, 0 );

    text_layer_set_text( row[4], ( row_state == ROW_STATE_DISAPPEAR ) ? row_old_data[4]
                                                                      : row_cur_data[4] );
    text_layer_set_text( row[8], " " );
  }

  // Datumszeile
  if( CONTENTS_CHANGED( 0 ) && !first_update )
  {
    // "normales" Update
    setup_text_layer( row[0], GPoint( -SCREEN_WIDTH, row_old_pos[0].y ), row_old_pos[0],
                              fontDate, ROW_STATE_KEEP, 0, 0 );
    setup_text_layer( row[5], row_cur_pos[0], GPoint( SCREEN_WIDTH, row_cur_pos[0].y ),
                              fontDate, row_state, 1, 0 );

    text_layer_set_text( row[0], row_old_data[0] );
    text_layer_set_text( row[5], row_cur_data[0] );
  }
  else
  {
    // initales update
    setup_text_layer( row[0], row_cur_pos[0], row_old_pos[0], fontDate, ROW_STATE_KEEP, 1, 0);

    text_layer_set_text( row[0], row_cur_data[0] );
    text_layer_set_text( row[5], " " );
  }

  first_update = 0;

#undef CONTENTS_CHANGED
}

static void update_status( struct Layer *layer, GContext *ctx )
{
  //TRACE
  BatteryChargeState charge = battery_state_service_peek();

  char batt_text[5] = "\0\0\0\0\0";
  // FIXME: BETA2 liefert nur werte von 10-90, nie 100
  int  batt_charge = 10 + (int)charge.charge_percent;

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

  graphics_fill_rect( ctx, layer_get_frame( layer ), 0, GCornerNone );
  
  graphics_fill_rect( ctx, batt_outline, 0, GCornerNone );
  graphics_draw_rect( ctx, batt_outline );

  if( batt_charge > 0 )
  {
    snprintf( batt_text, 4, "%d%c", batt_charge, charge.is_charging ? '+':'\0' );
    graphics_draw_text( ctx, batt_text, fontCharge, batt_label,
                        GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL );
  }

  // Die "Füllung" der Batterie wird via Invertieren realisiert
  // So kann auch der Text teilinvers dargestellt werden.
  layer_add_child( layer, inverter_layer_get_layer( charge_layer ) );
  layer_set_frame( inverter_layer_get_layer( charge_layer ), batt_fill );

  graphics_draw_bitmap_in_rect(ctx, bluetooth_connection_service_peek() ? icon_bt_on
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

static void on_minute_tick( struct tm *time_ticks __attribute__((__unused__)),
                            TimeUnits units_changed __attribute__((__unused__)) )
{
  TRACE

  cleanup_animations();
  update_rows();
  schedule_animations();
}

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

  if( accel_config_timer )
  {
    accel_tap_service_unsubscribe();

    app_timer_cancel( accel_config_timer );
    accel_config_timer = NULL;
  }
}

static void on_battery_change( BatteryChargeState charge __attribute__((__unused__)) )
{
  TRACE
  
  if( settings_status_visible )
  {
    layer_mark_dirty( status_layer );
  }
}

static void on_bluetooth_change( bool connected __attribute__((__unused__)) )
{
  TRACE

  if( settings_status_visible )
  {
    layer_mark_dirty( status_layer );
  }
}

static void window_load(Window *window)
{
  TRACE

  int i; // immer gut ein 'i' zu haben
  
  window_layer = window_get_root_layer( window );
  GRect window_frame = layer_get_frame( window_layer );
  GRect row_frame = GRect( 0, 0, SCREEN_WIDTH, ROW_MAX_HIGHT );
  GRect status_bar_rect = GRect( 0, 0, SCREEN_WIDTH, 20 );

  for( i = 0; i < NUM_ROWS + NUM_SHIFT_ROWS; ++i )
  {
    row[i] = text_layer_create( row_frame );
  }

  // Inverter, Statusbalken & Ladezustandslayer
  inverter_layer = inverter_layer_create( window_frame );
  layer_set_hidden( inverter_layer_get_layer( inverter_layer), !settings_inverter_state );
  layer_add_child( window_layer, inverter_layer_get_layer( inverter_layer ) );

  status_layer = layer_create( status_bar_rect );  
  charge_layer = inverter_layer_create( GRectZero );

  layer_set_update_proc( status_layer, update_status );
  layer_set_hidden( status_layer , !settings_status_visible );
  layer_add_child( window_layer, status_layer );
  layer_insert_below_sibling( status_layer, inverter_layer_get_layer( inverter_layer ) );

  accel_config_timer = app_timer_register( 5000, on_tap_timeout, NULL );

  accel_tap_service_subscribe( on_tap_gesture );
  battery_state_service_subscribe( on_battery_change );
  bluetooth_connection_service_subscribe( on_bluetooth_change );

  tick_timer_service_subscribe( MINUTE_UNIT, on_minute_tick );
}

static void window_unload(Window *window)
{
  TRACE

  int i;
  cleanup_animations();

  inverter_layer_destroy( inverter_layer );
  inverter_layer_destroy( charge_layer );
  layer_destroy( status_layer );

  for( i = 0; i < NUM_ROWS + NUM_SHIFT_ROWS; ++i )
  {
    if( row[i] != NULL )
    {
      text_layer_destroy( row[i] );
      row[i] = NULL;
    }
  }

  fonts_unload_custom_font( fontHour );
  fonts_unload_custom_font( fontMinutes );
  fonts_unload_custom_font( fontDate );
  fonts_unload_custom_font( fontUhr );
  fonts_unload_custom_font( fontCharge );

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

  window = window_create();

  window_set_fullscreen( window, true );
  window_set_background_color( window, GColorBlack );
  window_set_window_handlers( window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });

  window_stack_push( window, true /*animated*/ );

  fontHour    = fonts_load_custom_font( resource_get_handle( RESOURCE_ID_FONT_ROBOTO_BOLDITALIC_35 ) );
  fontMinutes = fonts_load_custom_font( resource_get_handle( RESOURCE_ID_FONT_ROBOTO_ITALIC_33 ) );
  fontUhr     = fonts_load_custom_font( resource_get_handle( RESOURCE_ID_FONT_ROBOTO_LIGHTITALIC_30 ) );
  fontDate    = fonts_load_custom_font( resource_get_handle( RESOURCE_ID_FONT_ROBOTO_ITALIC_13 ) );
  fontCharge  = fonts_load_custom_font( resource_get_handle( RESOURCE_ID_FONT_ROBOTO_REGULAR_9 ) );

  icon_bt_on  = gbitmap_create_with_resource( RESOURCE_ID_IMAGE_BT_ON_ICON );
  icon_bt_off = gbitmap_create_with_resource( RESOURCE_ID_IMAGE_BT_OFF_ICON );

  memset( row_cur_data, 0, sizeof( row_cur_data ) );
  memset( row_old_data, 0, sizeof( row_old_data ) );
  memset( animations, 0, sizeof( animations ) );
}

static void deinit(void)
{
  TRACE

  if( accel_config_timer )
  {
    app_timer_cancel( accel_config_timer );
    accel_config_timer = NULL;
  }

  accel_tap_service_unsubscribe();
  battery_state_service_unsubscribe();
  bluetooth_connection_service_unsubscribe();

  tick_timer_service_unsubscribe();

  window_destroy( window );
}

int main(void) 
{
  TRACE

  init();
  app_event_loop();
  deinit();
}
