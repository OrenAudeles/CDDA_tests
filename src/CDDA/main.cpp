#include <curses.h>
#include <inttypes.h>

#include <vector>
#include <string.h>

#include <stdio.h>

#define ESC 27
#define ENTER 10

struct curses_draw_cmd{
	int32_t x : 12;
	int32_t y : 12;
	int32_t c : 8;
};

struct term_data{
	uint16_t tx, ty;
	uint16_t layers;
};

enum mouse_button_state{
	MB_UP = 0,
	MB_CLICK,
	MB_DOWN
};
enum mouse_button_id{
	MB_LEFT = 0,
	MB_RIGHT = 1,
	NUM_MOUSE_BUTTONS
};

struct mouse_interact_data{
	mouse_button_state button[NUM_MOUSE_BUTTONS];

	int16_t cx, cy;
};

term_data tdata;
mouse_interact_data mdata;

#define IN_RANGE(a, b, v) ((((v)-(a)) >= 0) - (((v)-(b)) < 0))

bool mouse_in_region(int x, int y, int w, int h){
	bool ix = 0 == IN_RANGE(x, x+w, mdata.cx);
	bool iy = 0 == IN_RANGE(y, y+h, mdata.cy);

	return ix & iy;
}

typedef std::vector<curses_draw_cmd> draw_layer;
draw_layer* layers = nullptr;

void render_layer(const draw_layer& layer){
	for (const auto& data : layer){
		mvaddch(data.y, data.x, data.c);
	}
}
void init_layers(int16_t nlayers){
	layers = new draw_layer[nlayers];
	tdata.layers = nlayers;
}
void destroy_layers(void){
	delete[] layers;
	layers = nullptr;
	tdata.layers = 0;
}

void render(void){
	// Clear screen
	clear();

	// Assume layers initialized
	for (int i = 0; i < tdata.layers; ++i){
		render_layer(layers[i]);
	}

	// update screen
	refresh();
}

void layer_push(draw_layer& l, int x, int y, int c){
	// test range
	const uint32_t rx = (uint32_t)x;
	const uint32_t ry = (uint32_t)y;
	if ((rx < tdata.tx) & (ry < tdata.ty)){
		curses_draw_cmd cmd;
		cmd.x = x;
		cmd.y = y;
		cmd.c = c;

		l.push_back(cmd);
	}
}


void fill_layer(int layer, int fill, int x, int y, int w, int h){
	if (layer >= 0 && layer < tdata.layers){
		auto& dlayer = layers[layer];

		for (int yy = 0; yy < h; ++yy){
			for (int xx = 0; xx < w; ++xx){
				layer_push(dlayer, x + xx, y + yy, fill);
			}
		}
	}
}
void fill_layer(int layer, int fill){
	fill_layer(layer, fill, 0, 0, tdata.tx, tdata.ty);
}

int print_layer(int layer, int x, int y, const char* text, const int text_len){
	int result = 0;
	if (layer >= 0 && layer < tdata.layers){
		auto& dlayer = layers[layer];

		for (int i = 0; i < text_len; ++i){
			layer_push(dlayer, x + i, y, text[i]);
		}
		result = text_len;
	}
	return result;
}

void clear_layers(void){
	for (int i = 0; i < tdata.layers; ++i){
		layers[i].clear();
	}
}

#define KB_FUNC(name) void name(void* data)
typedef void (*callback_t)(void*);

#define DISPLAY_FUNC(name) void name(void);
typedef void (*display_t)(void);

KB_FUNC(main_sel_inc);
KB_FUNC(main_sel_dec);
KB_FUNC(main_sel_sel);
KB_FUNC(exit);

KB_FUNC(resize);
KB_FUNC(mouse_action);

int main_menu_option;
const int n_main_menu_options = 4;

const char* main_menu_options[n_main_menu_options];
int main_menu_options_len[n_main_menu_options];

callback_t main_menu_option_cb[n_main_menu_options];

struct log_item{
	char message[80];
};
std::vector<log_item> log;

void push_log(const char* message, ...){
	log_item item = {0};

	va_list arg;
	va_start(arg, message);
	vsnprintf(item.message, 80, message, arg);
	va_end(arg);

	log.push_back(item);
}
void dump_log(void){
	for (log_item& item : log){
		printf("LOG >> %s\n", item.message);
	}
}

std::vector<display_t> display_domains;
int domain_stack[32];
int cur_domain = 0;

void push_domain(int dom){
	domain_stack[cur_domain++] = dom;
	push_log("Domain PUSH");
}
void pop_domain(void){
	--cur_domain;
	push_log("Domain POP");
}

void init_main_menu_options(void){
	main_menu_options[0] = "New";
	main_menu_options[1] = "Load";
	main_menu_options[2] = "Options";
	main_menu_options[3] = "Exit";

	for (int i = 0; i < n_main_menu_options; ++i){
		main_menu_options_len[i] = strlen(main_menu_options[i]);
	}

	main_menu_option_cb[0] = [](void* data){
		push_log("New Game");
	};
	main_menu_option_cb[1] = [](void* data){
		push_log("Load Game");
	};
	main_menu_option_cb[2] = [](void* data){
		push_log("Options");
	};
	main_menu_option_cb[3] = [](void* data){
		push_log("Exit Game");
		pop_domain();
	};
}

struct {
	int32_t next, active;
	int32_t hover, next_hover;
	int32_t active_layer, next_hover_layer;
} ui_data;

int32_t get_next(void){
	return ++ui_data.next;
}
bool is_active(int32_t id){
	return ui_data.active == id;
}
bool is_hover(int32_t id){
	return (id > 0) && (ui_data.hover == id);
}
void set_active(int32_t id){
	ui_data.active = id;
}
void set_inactive(void){
	ui_data.active = 0;
}
void set_hover(int32_t id){
	ui_data.hover = id;
}
void set_next_hover(int32_t id){
	if (ui_data.next_hover_layer <= ui_data.active_layer){
		ui_data.next_hover = id;
		ui_data.next_hover_layer = ui_data.active_layer;
	}
}
bool mouse_state_eq(int button, mouse_button_state state){
	return mdata.button[button] == state;
}

void prepare_ui_frame(void){
	ui_data.next = 0;
	ui_data.next_hover = 0;
	ui_data.next_hover_layer = 0;
	ui_data.active_layer = 0;
}
void complete_ui_frame(void){
	set_hover(ui_data.next_hover);

	switch(mdata.button[MB_LEFT]){
		case MB_UP:{ set_active(0); break; }
		case MB_CLICK:{ mdata.button[MB_LEFT] = MB_DOWN; }
		case MB_DOWN:{
			if (is_active(0)){
				set_active(-1);
			}
			break;
		}
	}

	switch(mdata.button[MB_RIGHT]){
		case MB_UP:{ break; }
		case MB_CLICK:{ mdata.button[MB_RIGHT] = MB_DOWN; }
		case MB_DOWN:{ break; }
	}
}

// Assumes left-click presses buttons
bool button(int x, int y, int w, int h){
	auto self = get_next();

	bool result = false;

	if (is_active(self)){
		push_log("Button <%d>:", self);
		push_log(": Active");
		if (mouse_state_eq(0, MB_UP)){
			push_log("  : Released");
			if (is_hover(self)){
				push_log("    : Activating");
				result = true;
			}
			set_inactive();
		}
	}
	if (is_hover(self)){
		push_log("Button <%d>:", self);
		push_log(": Hovering");
		if (mouse_state_eq(0, MB_CLICK)){
			push_log("  : Making Active");
			set_active(self);
		}
	}

	if (mouse_in_region(x, y, w, h)){
		push_log("Button <%d>:", self);
		push_log(": Setting Hover");
		set_next_hover(self);
	}

	return result;
}

void display_main_menu(void){
	const int layer = 0;

	int spacing = 2;

	int x = 3;
	int y = 2;

	char select_open = '[';
	char select_close = ']';

	int selected;
	int printed;

	for (int i = 0; i < n_main_menu_options; ++i){
		selected = (main_menu_option == i);

		print_layer(layer, x, y, &select_open, selected);
		++x;
		printed = print_layer(layer, x, y, main_menu_options[i], main_menu_options_len[i]);

		if (button(x, y, printed, 1)){
			// force selection
			main_menu_option = i;

			// call selectionm
			main_sel_sel(0);
		}

		x += printed;
		print_layer(layer, x, y, &select_close, selected);
		++x;

		x += spacing;
	}
}
int running;
int frames;

void display_frame_data(void){
	char buf[80];
	int buf_len = snprintf(buf, 80, "%d : %d <%d, %d, n%d> [%d, %d]", frames, main_menu_option, ui_data.active, ui_data.hover, ui_data.next, mdata.cx, mdata.cy);

	print_layer(1, 2, tdata.ty - 1, buf, buf_len);

}

void set_mouse_mask(void){
	mmask_t mask =
		BUTTON1_PRESSED | BUTTON1_RELEASED | BUTTON1_CLICKED |
		//BUTTON3_PRESSED | BUTTON3_RELEASED | BUTTON3_CLICKED |
		REPORT_MOUSE_POSITION;

	mousemask(mask, NULL);
}

void dump_mouse_api(void){
	#define DATA(x) #x
	const char* api_text[28] = {
		#include "api_dat.h"
	};
	#undef DATA
	#define DATA(x) x
	const int api_val[28] = {
		#include "api_dat.h"
	};
	#undef DATA

	push_log("MOUSE API VALUES:");
	for (int i = 0; i < 28; ++i){
		push_log(" [%2d] %*.*s %d", i, 30, 30, api_text[i], api_val[i]);
	}
}

int main(const int argc, const char** argv){
	push_log("NCurses Mouse API V%d", NCURSES_MOUSE_VERSION);
	dump_mouse_api();
	frames = 0;

	tdata.tx = 120;
	tdata.ty = 24 / 2;
	tdata.layers = 3;

	init_layers(tdata.layers);
	init_main_menu_options();

	keypad(initscr(), 1);
	curs_set(0);
	//timeout(50);
	timeout(-1);
	noecho();
	// ncurses mouse registration
    set_mouse_mask();

	int c = 0;

	struct binding{
		int c;
		int domain;
		callback_t fn;
	};

	std::vector<binding> bindings;
	std::vector<binding> base_bindings;

	bindings.push_back({KEY_LEFT, 0, main_sel_dec});
	bindings.push_back({KEY_RIGHT, 0, main_sel_inc});
	bindings.push_back({ENTER, 0, main_sel_sel});
	bindings.push_back({ESC, 0, exit});

	base_bindings.push_back({KEY_RESIZE, -1, resize});
	base_bindings.push_back({KEY_MOUSE, -1, mouse_action});

	auto kb_hit = [&](int c, void* data){
		for (auto& binding : bindings){
			if (binding.domain == domain_stack[cur_domain] && binding.c == c){
				binding.fn(data);
				return;
			}
		}
		for (auto& binding : base_bindings){
			if (binding.c == c){
				binding.fn(data);
				return;
			}
		}
	};

	display_domains.push_back(display_main_menu);
	push_domain(0);

	do{
		prepare_ui_frame();
		++frames;
		// Prepare for display
		clear_layers();
		// Send in render commands
		fill_layer(0, '#');
		display_domains[domain_stack[cur_domain-1]]();
		display_frame_data();

		complete_ui_frame();
		// Render what we have
		render();
		
		// test for key hit
		c = getch();
		kb_hit(c, 0);
		
	}while(cur_domain > 0);

	auto endwin_result = endwin();
	destroy_layers();
	dump_log();

	return endwin_result;
}

KB_FUNC(main_sel_inc){
	(void)data;
	main_menu_option = (main_menu_option + 1) % n_main_menu_options;
}
KB_FUNC(main_sel_dec){
	(void)data;
	main_menu_option = (main_menu_option + n_main_menu_options - 1) % n_main_menu_options;
}
KB_FUNC(main_sel_sel){
	main_menu_option_cb[main_menu_option](data);
}
KB_FUNC(exit){
	pop_domain();
}

KB_FUNC(resize){
	push_log("Resizing!");
}

bool mouse_event_test(mmask_t bstate, mmask_t test){
	return bstate & test;
}

void set_mouse_data(int button, int x, int y, mouse_button_state state){
	push_log(": Update Mouse UI State <%d = %d @ %d, %d>", button, state, x, y);
	mdata.button[button] = state;
	mdata.cx = x;
	mdata.cy = y;
}

void generate_mouse_events(MEVENT event){
	push_log("MOUSE_MOVE: %d, %d", event.x, event.y);
	int button_ev_result = 0;
/* macros to extract single event-bits from masks */
// #define	BUTTON_RELEASE(e, x)		((e) & NCURSES_MOUSE_MASK(x, 001))
// #define	BUTTON_PRESS(e, x)		((e) & NCURSES_MOUSE_MASK(x, 002))
// #define	BUTTON_CLICK(e, x)		((e) & NCURSES_MOUSE_MASK(x, 004))
// #define	BUTTON_DOUBLE_CLICK(e, x)	((e) & NCURSES_MOUSE_MASK(x, 010))
// #define	BUTTON_TRIPLE_CLICK(e, x)	((e) & NCURSES_MOUSE_MASK(x, 020))
// #define	BUTTON_RESERVED_EVENT(e, x)	((e) & NCURSES_MOUSE_MASK(x, 040))

	if (BUTTON_PRESS(event.bstate, 1) | BUTTON_CLICK(event.bstate, 1)){
		push_log("MOUSE_BUTTON(%d) CLICK", MB_LEFT);
		++button_ev_result;
	}
	if (BUTTON_RELEASE(event.bstate, 1) | BUTTON_CLICK(event.bstate, 1)){
		push_log("MOUSE_BUTTON(%d) RELEASE", MB_LEFT);
		++button_ev_result;
	}

	// switch(event.bstate){
	// 	case BUTTON1_PRESSED:
	// 	case BUTTON1_CLICKED:{
	// 		push_log("MOUSE_BUTTON(%d) CLICK", MB_LEFT);
	// 		++button_ev_result;
	// 		break;
	// 	}
	// 	case BUTTON3_PRESSED:
	// 	case BUTTON3_CLICKED:{
	// 		push_log("MOUSE_BUTTON(%d) CLICK", MB_RIGHT);
	// 		++button_ev_result;
	// 		break;
	// 	}
	// }

	// switch(event.bstate){
	// 	case BUTTON1_RELEASED:
	// 	case BUTTON1_CLICKED:{
	// 		push_log("MOUSE_BUTTON(%d) RELEASE", MB_LEFT);
	// 		++button_ev_result;
	// 		break;
	// 	}
	// 	case BUTTON3_RELEASED:
	// 	case BUTTON3_CLICKED:{
	// 		push_log("MOUSE_BUTTON(%d) RELEASE", MB_RIGHT);
	// 		++button_ev_result;
	// 		break;
	// 	}
	// }

	if (button_ev_result == 0){
		push_log("Unknown bstate registered: %d", event.bstate);
	}
}

KB_FUNC(mouse_action){
	// Push events:
	// mouse_move_event
	// mouse_button_event [& mouse_button_event]
	// ^ depends on if it's a click event or not

	// This requires having a timeout that will
	// process one event per 'frame'

	// next best alternative is to track rising/falling edges
	// and always process all events each frame

	MEVENT event;
	if (getmouse(&event) == OK){
		generate_mouse_events(event);
	}
}