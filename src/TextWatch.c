#include "pebble.h"
#include "num2words-tr.h"
#include "battbar.h"
#include "bluetooth.h"
#define DEBUG 1
#define BUFFER_SIZE 44
#define SETTINGS_KEY 99

static Window *window;

static TextLayer *text_layer_percentage;
static InverterLayer *inverter_layer;

static int valueRead, valueWritten;
static AppSync sync;
static uint8_t sync_buffer[128];

enum { INVERT_KEY = 0x0, NUM_CONFIG_KEYS = 0x1 };

typedef struct {
	TextLayer *currentLayer;
	TextLayer *nextLayer;	
	PropertyAnimation *currentAnimation;
	PropertyAnimation *nextAnimation;
} Line;

typedef struct persist {
        int Invert;                  // Invert colours (0/1)
} __attribute__((__packed__)) persist;

persist settings = {
        .Invert = 0
};

static Line line1;
static Line line2;
static Line line3;

static struct tm *t;
static GFont lightFont;
static GFont boldFont;

static char line1Str[2][BUFFER_SIZE];
static char line2Str[2][BUFFER_SIZE];
static char line3Str[2][BUFFER_SIZE];

// Animation handler
static void animationStoppedHandler(struct Animation *animation, bool finished, void *context)
{
	Layer *textLayer = text_layer_get_layer((TextLayer *)context);
	GRect rect = layer_get_frame(textLayer);
	rect.origin.x = 144;
	layer_set_frame(textLayer, rect);
}

// Animate line
static void makeAnimationsForLayers(Line *line, TextLayer *current, TextLayer *next)
{
	GRect fromRect = layer_get_frame(text_layer_get_layer(next));
	GRect toRect = fromRect;
	toRect.origin.x -= 144;
	
	line->nextAnimation = property_animation_create_layer_frame(text_layer_get_layer(next), &fromRect, &toRect);
	animation_set_duration((Animation *)line->nextAnimation, 400);
	animation_set_curve((Animation *)line->nextAnimation, AnimationCurveEaseOut);
	animation_schedule((Animation *)line->nextAnimation);
	
	GRect fromRect2 = layer_get_frame(text_layer_get_layer(current));
	GRect toRect2 = fromRect2;
	toRect2.origin.x -= 144;
	
	line->currentAnimation = property_animation_create_layer_frame(text_layer_get_layer(current), &fromRect2, &toRect2);
	animation_set_duration((Animation *)line->currentAnimation, 400);
	animation_set_curve((Animation *)line->currentAnimation, AnimationCurveEaseOut);
	
	animation_set_handlers((Animation *)line->currentAnimation, (AnimationHandlers) {
		.stopped = (AnimationStoppedHandler)animationStoppedHandler
	}, current);
	
	animation_schedule((Animation *)line->currentAnimation);
}

// Update line
static void updateLineTo(Line *line, char lineStr[2][BUFFER_SIZE], char *value)
{
	TextLayer *next, *current;
	
	GRect rect = layer_get_frame(text_layer_get_layer(line->currentLayer));
	current = (rect.origin.x == 0) ? line->currentLayer : line->nextLayer;
	next = (current == line->currentLayer) ? line->nextLayer : line->currentLayer;
	
	// Update correct text only
	if (current == line->currentLayer) {
		memset(lineStr[1], 0, BUFFER_SIZE);
		memcpy(lineStr[1], value, strlen(value));
		text_layer_set_text(next, lineStr[1]);
	} else {
		memset(lineStr[0], 0, BUFFER_SIZE);
		memcpy(lineStr[0], value, strlen(value));
		text_layer_set_text(next, lineStr[0]);
	}
	
	makeAnimationsForLayers(line, current, next);
}

// Check to see if the current line needs to be updated
static bool needToUpdateLine(Line *line, char lineStr[2][BUFFER_SIZE], char *nextValue)
{
	char *currentStr;
	GRect rect = layer_get_frame(text_layer_get_layer(line->currentLayer));
	currentStr = (rect.origin.x == 0) ? lineStr[0] : lineStr[1];

	if (memcmp(currentStr, nextValue, strlen(nextValue)) != 0 ||
		(strlen(nextValue) == 0 && strlen(currentStr) != 0)) {
		return true;
	}
	return false;
}

// Update screen based on new time
static void display_time(struct tm *t)
{
	// The current time text will be stored in the following 3 strings
	char textLine1[BUFFER_SIZE];
	char textLine2[BUFFER_SIZE];
	char textLine3[BUFFER_SIZE];
	
	if(t->tm_min==0){
                time_to_3words(t->tm_hour, t->tm_min, textLine2, textLine1, textLine3, BUFFER_SIZE);
        }else{
                time_to_3words(t->tm_hour, t->tm_min, textLine1, textLine2, textLine3, BUFFER_SIZE);
        }
	
	if (needToUpdateLine(&line1, line1Str, textLine1)) {
		updateLineTo(&line1, line1Str, textLine1);	
	}
	if (needToUpdateLine(&line2, line2Str, textLine2)) {
		updateLineTo(&line2, line2Str, textLine2);	
	}
	if (needToUpdateLine(&line3, line3Str, textLine3)) {
		updateLineTo(&line3, line3Str, textLine3);	
	}
}

// Update screen without animation first time we start the watchface
static void display_initial_time(struct tm *t)
{
	if(t->tm_min==0){
                time_to_3words(t->tm_hour, t->tm_min, line2Str[0], line1Str[0], line3Str[0], BUFFER_SIZE);
        }else{
                time_to_3words(t->tm_hour, t->tm_min, line1Str[0], line2Str[0], line3Str[0], BUFFER_SIZE);
        }
	
	text_layer_set_text(line1.currentLayer, line1Str[0]);
	text_layer_set_text(line2.currentLayer, line2Str[0]);
	text_layer_set_text(line3.currentLayer, line3Str[0]);
}

// Debug methods. For quickly debugging enable debug macro on top to transform the watchface into
// a standard app and you will be able to change the time with the up and down buttons
#if DEBUG

static void up_single_click_handler(ClickRecognizerRef recognizer, void *context) {
  t->tm_min += 1;
	if (t->tm_min >= 60) {
		t->tm_min = 0;
		t->tm_hour += 1;
		
		if (t->tm_hour >= 24) {
			t->tm_hour = 0;
		}
	}
	display_time(t);
}


static void down_single_click_handler(ClickRecognizerRef recognizer, void *context) {
  t->tm_min -= 1;
	if (t->tm_min < 0) {
		t->tm_min = 59;
		t->tm_hour -= 1;
	}
	display_time(t);
}

static void click_config_provider(ClickRecognizerRef recognizer, void *context) {
	window_single_click_subscribe(BUTTON_ID_UP, (ClickHandler)up_single_click_handler);
	window_single_click_subscribe(BUTTON_ID_DOWN, (ClickHandler)down_single_click_handler);
}

#endif

// Configure the first line of text
static void configureBoldLayer(TextLayer *textlayer)
{
	text_layer_set_font(textlayer, boldFont);
	text_layer_set_text_color(textlayer, GColorWhite);
	text_layer_set_background_color(textlayer, GColorClear);
	text_layer_set_text_alignment(textlayer, GTextAlignmentLeft);
}

// Configure for the 2nd and 3rd lines
static void configureLightLayer(TextLayer *textlayer)
{
	text_layer_set_font(textlayer, lightFont);
	text_layer_set_text_color(textlayer, GColorWhite);
	text_layer_set_background_color(textlayer, GColorClear);
	text_layer_set_text_alignment(textlayer, GTextAlignmentLeft);
}

// Time handler called every minute by the system
static void handle_minute_tick(struct tm *tick_time, TimeUnits units_changed) {
	t = tick_time;
  display_time(tick_time);
}

// Invert Layer
static void remove_invert() {
    if (inverter_layer != NULL) {
                layer_remove_from_parent(inverter_layer_get_layer(inverter_layer));
                inverter_layer_destroy(inverter_layer);
                inverter_layer = NULL;
    }
}

static void set_invert() {
    if (!inverter_layer) {
                inverter_layer = inverter_layer_create(GRect(0, 0, 144, 168));
                layer_add_child(window_get_root_layer(window), inverter_layer_get_layer(inverter_layer));
                layer_mark_dirty(inverter_layer_get_layer(inverter_layer));
    }
}

static void sync_tuple_changed_callback(const uint32_t key, const Tuple * new_tuple, const Tuple * old_tuple, void *context) {
    // APP_LOG(APP_LOG_LEVEL_DEBUG, "TUPLE! %lu : %d", key, new_tuple->value->uint8);
        if(new_tuple==NULL || new_tuple->value==NULL) {
                return;
        }
        switch (key) {
                case INVERT_KEY:
                        remove_invert();
                        settings.Invert = new_tuple->value->uint8;
                        if (settings.Invert) {
                                set_invert();
                        }
                        break;
        }
}

static void loadPersistentSettings() {
        valueRead = persist_read_data(SETTINGS_KEY, &settings, sizeof(settings));
}

static void savePersistentSettings() {
        valueWritten = persist_write_data(SETTINGS_KEY, &settings, sizeof(settings));
}


static void init() {

  loadPersistentSettings();

  window = window_create();
  window_stack_push(window, true);
  window_set_background_color(window, GColorBlack);

	// Custom fonts
	lightFont = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_LIGHT_42));
	boldFont = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_BOLD_42));

	// 1st line layers
	line1.currentLayer = text_layer_create(GRect(0, 16, 144, 50));
	line1.nextLayer = text_layer_create(GRect(144, 16, 144, 50));
	configureBoldLayer(line1.currentLayer);
	configureBoldLayer(line1.nextLayer);

	// 2nd layers
	line2.currentLayer = text_layer_create(GRect(0, 56, 144, 50));
	line2.nextLayer = text_layer_create(GRect(144, 56, 144, 50));
	configureLightLayer(line2.currentLayer);
	configureLightLayer(line2.nextLayer);

	// 3rd layers
	line3.currentLayer = text_layer_create(GRect(0, 95, 144, 50));
	line3.nextLayer = text_layer_create(GRect(144, 95, 144, 50));
	configureLightLayer(line3.currentLayer);
	configureLightLayer(line3.nextLayer);

	// Configure time on init
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
	display_initial_time(t);

	// Load layers
	Layer *window_layer = window_get_root_layer(window);
	layer_add_child(window_layer, text_layer_get_layer(line1.currentLayer));
	layer_add_child(window_layer, text_layer_get_layer(line1.nextLayer));
	layer_add_child(window_layer, text_layer_get_layer(line2.currentLayer));
	layer_add_child(window_layer, text_layer_get_layer(line2.nextLayer));
	layer_add_child(window_layer, text_layer_get_layer(line3.currentLayer));
	layer_add_child(window_layer, text_layer_get_layer(line3.nextLayer));

	#if DEBUG
	// Button functionality
	window_set_click_config_provider(window, (ClickConfigProvider) click_config_provider);
	#endif

	//Options for battery status bar
        BBOptions options;
        options.position = BATTBAR_POSITION_TOP;
        options.direction = BATTBAR_DIRECTION_DOWN;
        options.color = BATTBAR_COLOR_WHITE;
        options.isWatchApp = true;
        SetupBattBar(options, window_layer); /* Setup the display, subscribe to battery service */
        DrawBattBar(); /* Initial display of the bar */
	
	//For bluetooth
        bluetooth_init(window_layer);

	Tuplet initial_values[NUM_CONFIG_KEYS] = {
                TupletInteger(INVERT_KEY, settings.Invert)
        };

        app_message_open(128, 128);
        app_sync_init(&sync, sync_buffer, sizeof(sync_buffer), initial_values,
                        ARRAY_LENGTH(initial_values), sync_tuple_changed_callback, NULL, NULL);

  
  tick_timer_service_subscribe(MINUTE_UNIT, handle_minute_tick);
}

static void deinit() {
        savePersistentSettings();
        app_sync_deinit(&sync);
	tick_timer_service_unsubscribe();
	window_destroy(window);
        text_layer_destroy(text_layer_percentage);
	bluetooth_deinit();
	inverter_layer_destroy(inverter_layer);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
