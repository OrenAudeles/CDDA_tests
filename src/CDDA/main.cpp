#include <curses.h>
#include <inttypes.h>

#include <vector>
#include <string.h>

#include <stdio.h>

#include "log.h"

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

struct mouse_button_edge{
	uint8_t press : 1;
	uint8_t release : 1;
	uint8_t held : 1;

	uint8_t pad : 5;
};

struct mouse_interact_data{
	mouse_button_edge edge[NUM_MOUSE_BUTTONS];
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
		push_log("- CurDomain: %d", cur_domain);
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

bool mouse_down(int button){
	return mdata.edge[button].press | mdata.edge[button].held;
}
bool mouse_up(int button){
	return mdata.edge[button].release | !mdata.edge[button].held;
}
bool mouse_down_this_frame(int button){
	return mdata.edge[button].press;
}
bool mouse_up_this_frame(int button){
	return mdata.edge[button].release;
}

void set_mouse_down(int button){
	mdata.edge[button].press = 1;
}
void set_mouse_up(int button){
	mdata.edge[button].release = 1;
}

void set_mouse_position(int x, int y){
	mdata.cx = x;
	mdata.cy = y;
}

void reset_mouse_frame(void){
	mdata.edge[MB_LEFT].held |= mdata.edge[MB_LEFT].press; // if it was pressed we want to stay held
	mdata.edge[MB_LEFT].held &= !mdata.edge[MB_LEFT].release; // if it was released we want to always clear state

	mdata.edge[MB_RIGHT].held |= mdata.edge[MB_RIGHT].press; // if it was pressed we want to stay held
	mdata.edge[MB_RIGHT].held &= !mdata.edge[MB_RIGHT].release; // if it was released we want to always clear state

	// mdata.edge[MB_LEFT].held = !mouse_up(MB_LEFT);
	// mdata.edge[MB_RIGHT].held = !mouse_up(MB_RIGHT);

	mdata.edge[MB_LEFT].press = mdata.edge[MB_LEFT].release = 0;
	mdata.edge[MB_RIGHT].press = mdata.edge[MB_RIGHT].release = 0;
}

void prepare_ui_frame(void){
	ui_data.next = 0;
	ui_data.next_hover = 0;
	ui_data.next_hover_layer = 0;
	ui_data.active_layer = 0;
}
void complete_ui_frame(void){
	reset_mouse_frame();
	set_hover(ui_data.next_hover);

	// if (mouse_down_this_frame(MB_LEFT)){
	// 	mdata.button[MB_LEFT] = MB_CLICK;
	// }
	// else if (mouse_up_this_frame(MB_LEFT)){
	// 	mdata.button[MB_LEFT] = MB_UP;
	// }


	// switch(mdata.button[MB_LEFT]){
	// 	case MB_UP:{ set_active(0); break; }
	// 	case MB_CLICK:{ mdata.button[MB_LEFT] = MB_DOWN; }
	// 	case MB_DOWN:{
	// 		if (is_active(0)){
	// 			set_active(-1);
	// 		}
	// 		break;
	// 	}
	// }
	

	// switch(mdata.button[MB_RIGHT]){
	// 	case MB_UP:{ break; }
	// 	case MB_CLICK:{ mdata.button[MB_RIGHT] = MB_DOWN; }
	// 	case MB_DOWN:{ break; }
	// }
	
}

// Assumes left-click presses buttons
bool button(int x, int y, int w, int h){
	auto self = get_next();

    bool result = false;

    if (is_active(self)){
        if (mouse_up(MB_LEFT)){
            if (is_hover(self)){
                result = true;
            }
            set_inactive();
        }
    }
    if (is_hover(self)){
        if (mouse_down_this_frame(MB_LEFT)){
            set_active(self);
        }
    }

    if (mouse_in_region(x, y, w, h)){
        set_next_hover(self);
    }
    return result;
}

void display_main_menu(void){
	push_log("MAIN MENU");
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
			push_log("- Selecting: %d [%.*s]", i, printed, main_menu_options[i]);
			// force selection
			main_menu_option = i;

			// call selectionm
			main_sel_sel(0);
			push_log("- Selection Executed");
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
	int buf_len = snprintf(buf, 80, "%d : %d <%d, %d, n%d> [%d, %d] (%d, %d, %d)", frames, main_menu_option, ui_data.active, ui_data.hover, ui_data.next, mdata.cx, mdata.cy, mdata.edge[0].press, mdata.edge[0].release, mdata.edge[0].held);

	print_layer(1, 2, tdata.ty - 1, buf, buf_len);

}

void set_mouse_mask(void){
	mmask_t mask =
		BUTTON1_PRESSED | BUTTON1_RELEASED | BUTTON1_CLICKED |
		BUTTON3_PRESSED | BUTTON3_RELEASED | BUTTON3_CLICKED |
		REPORT_MOUSE_POSITION;

	mousemask(mask, NULL);
}

void dump_mouse_api(void){
// Commented out so you don't need to have the include file

/*
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
		push_log(" [%2d] %*.*s %X", i, 30, 30, api_text[i], api_val[i]);
	}
*/
}
enum mouse_event_type{
	ME_NULL = 0,
	ME_MOUSE_MOVE,
	ME_MOUSE_BUTTON_PRESS,
	ME_MOUSE_BUTTON_RELEASE,
	NUM_MOUSE_EVENT_TYPES
};

struct mouse_event_data{
	union{
		struct {
			int x, y;
		} position;

		struct {
			int id;
		} button;
	};
};

struct mouse_event{
	mouse_event_type type;
	mouse_event_data data;
};

std::vector<mouse_event> mouse_events;

typedef void (*mouse_event_fn)(mouse_event_data data);

mouse_event_fn mouse_event_cb[NUM_MOUSE_EVENT_TYPES];

void init_mouse_functions(void){
	mouse_event_cb[ME_NULL] = [](mouse_event_data _unused){
		(void)_unused;
	};
	mouse_event_cb[ME_MOUSE_MOVE] = [](mouse_event_data data){
		//push_log("MouseMove <%d, %d>", data.position.x, data.position.y);
		set_mouse_position(data.position.x, data.position.y);
	};
	mouse_event_cb[ME_MOUSE_BUTTON_PRESS] = [](mouse_event_data data){
		push_log("MousePress <%d>", data.button.id);
		set_mouse_down(data.button.id);
	};
	mouse_event_cb[ME_MOUSE_BUTTON_RELEASE] = [](mouse_event_data data){
		push_log("MouseRelease <%d>", data.button.id);
		set_mouse_up(data.button.id);
	};
}

void process_mouse_events(void){
	const int sz = mouse_events.size();
	for (int i = 0; i < sz; ++i){
		mouse_event_cb[mouse_events[i].type](mouse_events[i].data);
	}
	mouse_events.clear();
}

void enable_mouse_tracking(void){
	printf("\033[?1003h\n");
}
void disable_mouse_tracking(void){
	printf("\033[?1003l\n"); // Disable mouse movement events, as l = low
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
	timeout(100);
	//timeout(-1);
	noecho();
	// ncurses mouse registration
    set_mouse_mask();
    init_mouse_functions();
    mouseinterval(10);
    enable_mouse_tracking();

    // {
    // 	int x = 0, y = 0;
    // 	getyx(curscr, y, x);
    // 	push_log("Startup Cursor Position: %d, %d", x, y);
    // }

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

		//push_log("kb_hit: unbound input (%d)", c);
	};

	display_domains.push_back(display_main_menu);
	push_domain(0);

	auto ui_render = [&](){
		clear_layers();
		// Send in render commands
		fill_layer(0, ' ');
		display_domains[domain_stack[cur_domain-1]]();
		//display_domains[domain_stack[cur_domain-1]]();
	};

	//int ungetmouse(MEVENT *event);
	// {
	// 	MEVENT event;
	// 	event.x = event.y = event.z = 0;
	// 	event.bstate = REPORT_MOUSE_POSITION;
	// 	ungetmouse(&event);
	// }

	ui_render();
	display_frame_data();
	render();

	do{
		prepare_ui_frame();
		process_mouse_events();
		++frames;
		// Prepare for display
		ui_render();

		complete_ui_frame();
		display_frame_data();
		render();

		// poll
		c = getch();
		kb_hit(c, 0);
	}while(cur_domain > 0);

	disable_mouse_tracking();
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

void mouse_move_event(int x, int y){
	mouse_event ev;
	ev.type = ME_MOUSE_MOVE;
	ev.data.position.x = x;
	ev.data.position.y = y;

	mouse_events.push_back(ev);
}
void mouse_button_up_event(int button){
	mouse_event ev;
	ev.type = ME_MOUSE_BUTTON_RELEASE;
	ev.data.button.id = button;

	mouse_events.push_back(ev);
}
void mouse_button_down_event(int button){
	mouse_event ev;
	ev.type = ME_MOUSE_BUTTON_PRESS;
	ev.data.button.id = button;

	mouse_events.push_back(ev);
}

void generate_mouse_events(MEVENT event){
	//push_log("MOUSE_MOVE: %d, %d", event.x, event.y);
	//mouse_move_event(event.x, event.y);

	int button_ev_result = 0;
/* macros to extract single event-bits from masks */
// #define	BUTTON_RELEASE(e, x)		((e) & NCURSES_MOUSE_MASK(x, 001))
// #define	BUTTON_PRESS(e, x)		((e) & NCURSES_MOUSE_MASK(x, 002))
// #define	BUTTON_CLICK(e, x)		((e) & NCURSES_MOUSE_MASK(x, 004))
// #define	BUTTON_DOUBLE_CLICK(e, x)	((e) & NCURSES_MOUSE_MASK(x, 010))
// #define	BUTTON_TRIPLE_CLICK(e, x)	((e) & NCURSES_MOUSE_MASK(x, 020))
// #define	BUTTON_RESERVED_EVENT(e, x)	((e) & NCURSES_MOUSE_MASK(x, 040))
	mouse_move_event(event.x, event.y);

	if (BUTTON_PRESS(event.bstate, 1) | BUTTON_CLICK(event.bstate, 1)){
		//push_log("MOUSE_BUTTON(%d) CLICK", MB_LEFT);
		++button_ev_result;
		mouse_button_down_event(MB_LEFT);
	}
	if (BUTTON_RELEASE(event.bstate, 1) | BUTTON_CLICK(event.bstate, 1)){
		//push_log("MOUSE_BUTTON(%d) RELEASE", MB_LEFT);
		++button_ev_result;
		mouse_button_up_event(MB_LEFT);
	}

    if (BUTTON_PRESS(event.bstate, 3) | BUTTON_CLICK(event.bstate, 3)){
        //push_log("MOUSE_BUTTON(%d) CLICK", MB_RIGHT);
        ++button_ev_result;
        mouse_button_down_event(MB_RIGHT);
    }
    if (BUTTON_RELEASE(event.bstate, 3) | BUTTON_CLICK(event.bstate, 3)){
        //push_log("MOUSE_BUTTON(%d) RELEASE", MB_RIGHT);
        ++button_ev_result;
        mouse_button_up_event(MB_RIGHT);
    }

	if (event.bstate & REPORT_MOUSE_POSITION){
		++button_ev_result;
	}

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