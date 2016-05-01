#include <pebble.h>

#define KEY_TEMPERATURE 0
#define KEY_CONDITIONS 1
#define KEY_FREQUENCY_UPDATE_WEATHER 2

static Window *s_main_window;
static TextLayer *s_time_layer_hours;
static TextLayer *s_time_layer_minutes;
#if defined(PBL_BW)
  static GBitmap *s_bitmap_bw;
#elif defined(PBL_COLOR)
  static GBitmap *s_bitmap;
#endif
static BitmapLayer *s_layer;
static GBitmap *s_bitmap_bt;
static BitmapLayer *s_layer_bt;
static TextLayer *s_battery_layer;
static TextLayer *s_date_layer;
static TextLayer *s_weather_layer;

static char spanish_language [5] = "es_ES";

static GFont s_font_temperature;
static GFont s_font_time;

int freq_update_weather;

static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
  // Almacena la informacion entrante
  static char temperature_buffer[8];
  static char conditions_buffer[32];
  static char weather_layer_buffer[32];
  
  // Lee las tuplas para obtener los datos
  Tuple *temp_tuple = dict_find(iterator, KEY_TEMPERATURE);
  Tuple *conditions_tuple = dict_find(iterator, KEY_CONDITIONS);
  Tuple *freq_update_t = dict_find(iterator, KEY_FREQUENCY_UPDATE_WEATHER);
  
  // Si todos los datos estan disponibles, se usan
  if(temp_tuple && conditions_tuple) {
  snprintf(temperature_buffer, sizeof(temperature_buffer), "%d°", (int)temp_tuple->value->int32);
  }
  
  // Se concatenan las cadenas de caracteres y se muestran
  snprintf(weather_layer_buffer, sizeof(weather_layer_buffer), "%s", temperature_buffer);
  text_layer_set_text(s_weather_layer, weather_layer_buffer);
  
  if(freq_update_t) {
   freq_update_weather = freq_update_t->value->int32;
  }
  
}


static void inbox_dropped_callback(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped!");
}

static void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed!");
}

static void outbox_sent_callback(DictionaryIterator *iterator, void *context) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Outbox send success!");
}

static void bluetooth_callback(bool connected) {
  // Muestra el icono cuando esta desconectado
  layer_set_hidden(bitmap_layer_get_layer(s_layer_bt), connected);

  if(!connected) {
    // Vibracion larga
    vibes_long_pulse();
  }
}

static void battery_callback(BatteryChargeState state) {
  static char s_battery_buffer [6];
  snprintf(s_battery_buffer, 5, "%d%%", state.charge_percent);

  text_layer_set_text(s_battery_layer, s_battery_buffer);
  
}

static void my_layer_draw(Layer *layer, GContext *ctx) {
  // Crea un nuevo rectangulo con los limites indicados por el "layer"
  GRect bounds = layer_get_bounds(layer);
  
  // Obtiene el centro del rectangulo (para indicar el inicio del segundo)
  GPoint center = grect_center_point(&bounds);
  
  // El tamanyo del rectangulo es la mitad del espacio de la "watchface"
  bounds.size.w /= 2;

  // Establece la segunda mitad de la ventana para el segundo rectangulo
  GRect bounds2 = GRect(center.x, 0, bounds.size.w, bounds.size.h);
  
  // Pinta la mitad del fondo de blanco
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  // Pinta la otra mitad de azul
  graphics_context_set_fill_color(ctx, GColorFromHEX(0x007cb2));
  graphics_fill_rect(ctx, bounds2 , 0, GCornerNone);
  
  
}


static void update_time() {
  time_t temp = time(NULL); 
  struct tm *tick_time = localtime(&temp);

  static char s_buffer_hours[4];
  static char s_buffer_minutes[4];
  static char date_buffer[16];
  
  // Obtencion de las cadenas de caracteres con las horas y los minutos, respectivamente
  strftime(s_buffer_hours, sizeof(s_buffer_hours), clock_is_24h_style() ?
                                                      "%H" : "%I", tick_time);
  strftime(s_buffer_minutes, sizeof(s_buffer_minutes), "%M", tick_time);
  strftime(date_buffer, sizeof(date_buffer), PBL_IF_ROUND_ELSE("%a %d%n %b", "%a %d %b"), tick_time);
  
  text_layer_set_text(s_time_layer_hours, s_buffer_hours);
  text_layer_set_text(s_time_layer_minutes, s_buffer_minutes);
  text_layer_set_text(s_date_layer, date_buffer);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_time();
  
  // Actualiza el tiempo atmosferico cada cierto numero de minutos
  if(tick_time->tm_min % 45 == 0) {
    // Se inicializa el diccionario
    DictionaryIterator *iter;
    app_message_outbox_begin(&iter);
  
    // Se agrega un par clave-valor
    dict_write_uint8(iter, 0, 0);
  
    // Se envia el mensaje
    app_message_outbox_send();
  }
}

static void main_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  char y_offset_time_layer = PBL_IF_ROUND_ELSE(14, 8);
  char x_offset_battery_layer = PBL_IF_ROUND_ELSE(17, 17);
  char y_offset_battery_layer = PBL_IF_ROUND_ELSE((bounds.size.h / 2) - 11, bounds.size.h - 22);
  char x_offset_date_layer = PBL_IF_ROUND_ELSE(bounds.size.w - 62, bounds.size.w - 64);
  char y_offset_date_layer = PBL_IF_ROUND_ELSE((bounds.size.h / 2) - 12 , bounds.size.h - 18);
  char x_offset_weather_layer = PBL_IF_ROUND_ELSE(bounds.size.w - 70, bounds.size.w - 56);
  char y_offset_weather_layer = PBL_IF_ROUND_ELSE((bounds.size.h / 2) + 22, (bounds.size.h / 2) - 18);
  GColor color_blue = GColorFromHEX(0x007cb2);
                                                  
  layer_set_update_proc(window_layer, my_layer_draw);
    
  // Creacion del layer con las horas
  s_time_layer_hours = text_layer_create(
      GRect(0, y_offset_time_layer, (bounds.size.w / 2 - 4), 52));
  
  // Creacion del layer con los minutos
  s_time_layer_minutes = text_layer_create(
      GRect((bounds.size.w / 2) + 4, y_offset_time_layer, (bounds.size.w / 2) - 4, 52));

  // Creacion del layer para el nivel de bateria
  s_battery_layer = text_layer_create(
      GRect(x_offset_battery_layer, y_offset_battery_layer, 40, 18));
  
  // Creacion del layer para la fecha
  s_date_layer = text_layer_create(
      GRect(x_offset_date_layer, y_offset_date_layer , 64, PBL_IF_ROUND_ELSE(36, 18)));
  
  // Creacion del layer para mostrar el tiempo atmosferico
  s_weather_layer = text_layer_create(
    GRect(x_offset_weather_layer, y_offset_weather_layer, 72, 72));
  
  // Estilo del layer del tiempo atmosferico
  text_layer_set_background_color(s_weather_layer, GColorClear);
  text_layer_set_text_color(s_weather_layer, GColorWhite);
  text_layer_set_font(s_weather_layer, s_font_temperature);
  text_layer_set_text(s_weather_layer, "--");
  text_layer_set_text_alignment(s_weather_layer, GTextAlignmentCenter);
  
  // Estilo del layer con la hora
  text_layer_set_background_color(s_time_layer_hours, GColorClear);
  text_layer_set_text_color(s_time_layer_hours, color_blue);
  text_layer_set_text(s_time_layer_hours, "--");
  text_layer_set_font(s_time_layer_hours, s_font_time);
  text_layer_set_text_alignment(s_time_layer_hours, GTextAlignmentRight);
  
  // Estilo del layer con los minutos
  text_layer_set_background_color(s_time_layer_minutes, GColorClear);
  text_layer_set_text_color(s_time_layer_minutes, GColorWhite);
  text_layer_set_text(s_time_layer_minutes, "--");
  text_layer_set_font(s_time_layer_minutes, s_font_time);
  text_layer_set_text_alignment(s_time_layer_minutes, GTextAlignmentLeft);
  
  // Estilo del layer con el porcentaje de bateria
  text_layer_set_background_color(s_battery_layer, GColorClear);
  text_layer_set_text_color(s_battery_layer, color_blue);
  text_layer_set_text(s_battery_layer, "---");
  text_layer_set_font(s_battery_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_alignment(s_battery_layer, GTextAlignmentLeft);
  
  // Estilo del layer con la fecha
  text_layer_set_background_color(s_date_layer, GColorClear);
  text_layer_set_text_color(s_date_layer, GColorWhite);
  text_layer_set_text(s_date_layer, "---");
  text_layer_set_font(s_date_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD));
  text_layer_set_text_alignment(s_date_layer, GTextAlignmentCenter);

  GSize text_size = text_layer_get_content_size(s_time_layer_hours);

  // Adicion de los layers a la ventana principal
  layer_add_child(window_layer, text_layer_get_layer(s_time_layer_hours));
  layer_add_child(window_layer, text_layer_get_layer(s_time_layer_minutes));
  layer_add_child(window_layer, text_layer_get_layer(s_battery_layer));
  layer_add_child(window_layer, text_layer_get_layer(s_date_layer));
  layer_add_child(window_layer, text_layer_get_layer(s_weather_layer));
  
  #if defined(PBL_COLOR)
    // Se carga la imagen del escudo en color
    s_bitmap = gbitmap_create_with_resource(RESOURCE_ID_ESCUDO_PONFE);
    GSize image_size = gbitmap_get_bounds(s_bitmap).size;
    char image_offsetX = (bounds.size.w - image_size.w) / 2; 
    GRect image_frame = GRect(image_offsetX, text_size.h + y_offset_time_layer + 6, image_size.w, image_size.h);
    s_layer = bitmap_layer_create(image_frame);
  
    bitmap_layer_set_bitmap(s_layer, s_bitmap);
    bitmap_layer_set_compositing_mode(s_layer, GCompOpSet);
    bitmap_layer_set_alignment(s_layer, GAlignBottom);
 #elif defined(PBL_BW)
    // Se carga la imagen del escudo en blanco y negro
    s_bitmap_bw = gbitmap_create_with_resource(RESOURCE_ID_ESCUDO_BN);
    GSize image_size = gbitmap_get_bounds(s_bitmap_bw).size;
    char image_offsetX = (bounds.size.w - image_size.w) / 2; 
    GRect image_frame = GRect(image_offsetX, text_size.h + y_offset_time_layer + 6, image_size.w, image_size.h);
    s_layer = bitmap_layer_create(image_frame);
  
    bitmap_layer_set_bitmap(s_layer, s_bitmap_bw);
    bitmap_layer_set_compositing_mode(s_layer, GCompOpSet);
    bitmap_layer_set_alignment(s_layer, GAlignBottom);
  #endif
  
  // Create the Bluetooth icon GBitmap
  s_bitmap_bt = gbitmap_create_with_resource(RESOURCE_ID_BT_ICON);

  // Create the BitmapLayer to display the GBitmap
  s_layer_bt = bitmap_layer_create(GRect(PBL_IF_ROUND_ELSE(22, 8), PBL_IF_ROUND_ELSE((bounds.size.h / 2) + 18, bounds.size.h / 2), 24, 24));
  bitmap_layer_set_bitmap(s_layer_bt, s_bitmap_bt);
  bitmap_layer_set_compositing_mode(s_layer_bt, GCompOpSet);
  bitmap_layer_set_alignment(s_layer_bt, GAlignLeft);
  
  layer_add_child(window_layer, bitmap_layer_get_layer(s_layer_bt));
  layer_add_child(window_layer, bitmap_layer_get_layer(s_layer));
}

static void main_window_unload(Window *window) {
  bitmap_layer_destroy(s_layer);
  bitmap_layer_destroy(s_layer_bt);
  text_layer_destroy(s_time_layer_hours);
  text_layer_destroy(s_time_layer_minutes);
  #if defined(PBL_BW)
    gbitmap_destroy(s_bitmap_bw);
  #elif defined(PBL_COLOR)
    gbitmap_destroy(s_bitmap);
  #endif
  gbitmap_destroy(s_bitmap_bt);
  text_layer_destroy(s_battery_layer);
  text_layer_destroy(s_date_layer);
  text_layer_destroy(s_weather_layer);
}


static void init() {
  
  // Idioma local fijado (Ingles o Español)
  if(!strcmp(i18n_get_system_locale(), spanish_language))
    setlocale(LC_ALL, "es_ES");
  else
    setlocale(LC_ALL, "en_US");
  
  // Inicializacion de las fuentes personalizadas (horas y minutos, y temperatura)
  s_font_temperature = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_TEMPERATURE_21));
  s_font_time = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_TIME_42));
  
  // Creacion de la ventana principal y asignacion a un puntero
  s_main_window = window_create();

  // Creacion de handlers para gestionar los elementos en la ventana 
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });

  // Muestra la ventana en el reloj
  window_stack_push(s_main_window, true);

  // La hora se muestra desde el inicio
  update_time();
  
  // El nivel de bateria se muestra desde el inicio
  battery_callback(battery_state_service_peek());
  
  // Estado de la conexion Bluetooth mostrado desde el principio
  bluetooth_callback(connection_service_peek_pebble_app_connection());
  
  // Se establece un registro con el servicio Tick Timer
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  
  // Se establece un registro con el servicio Battery State
  battery_state_service_subscribe(battery_callback);
  
  // Se establece un registro con el servicio de conexion Bluetooth
  connection_service_subscribe((ConnectionHandlers) {
  .pebble_app_connection_handler = bluetooth_callback
});
  
  // Registro de los callbacks para obtener el tiempo atmosferico
  app_message_register_inbox_received(inbox_received_callback);
  // Abre el mensaje de la aplicacion
  const int inbox_size = 128;
  const int outbox_size = 128;
  app_message_open(inbox_size, outbox_size);
  app_message_register_inbox_dropped(inbox_dropped_callback);
  app_message_register_outbox_failed(outbox_failed_callback);
  app_message_register_outbox_sent(outbox_sent_callback);
  
}

static void deinit() {
  // Se destruye la ventana
  window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}