#include "pebble.h"
#include "bluetooth.h"

static GBitmap *bt_icon;
static BitmapLayer *image_bt_icon_layer;

static void handle_bt_conn(bool connected) {
  layer_set_hidden(bitmap_layer_get_layer(image_bt_icon_layer), !connected);

  if (!connected) {
    vibes_double_pulse();
  }
}

void bluetooth_init(Layer *window_layer) {
  bt_icon = gbitmap_create_with_resource(RESOURCE_ID_BT_ICON_BLACK);
  image_bt_icon_layer = bitmap_layer_create(GRect(3, 3, 15, 15));
  bitmap_layer_set_bitmap(image_bt_icon_layer, bt_icon);
  bitmap_layer_set_compositing_mode(image_bt_icon_layer, GCompOpAssign);
  layer_add_child(window_layer, bitmap_layer_get_layer(image_bt_icon_layer));
  bool connected = bluetooth_connection_service_peek();
  handle_bt_conn(connected);
  bluetooth_connection_service_subscribe(handle_bt_conn);
}

void bluetooth_deinit(void) {
  bluetooth_connection_service_unsubscribe();
  layer_remove_from_parent(bitmap_layer_get_layer(image_bt_icon_layer));
  bitmap_layer_destroy(image_bt_icon_layer);
  gbitmap_destroy(bt_icon);
}
