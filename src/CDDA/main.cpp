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

term_data tdata;

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
	char message[40];
};
std::vector<log_item> log;

void push_log(const char* message, ...){
	log_item item = {0};

	va_list arg;
	va_start(arg, message);
	vsnprintf(item.message, 40, message, arg);
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

void display_main_menu(void){
	const int layer = 0;

	int spacing = 2;

	int x = 3;
	int y = 2;

	char select_open = '[';
	char select_close = ']';

	int selected;

	for (int i = 0; i < n_main_menu_options; ++i){
		selected = (main_menu_option == i);

		print_layer(layer, x, y, &select_open, selected);
		++x;
		x += print_layer(layer, x, y, main_menu_options[i], main_menu_options_len[i]);
		print_layer(layer, x, y, &select_close, selected);
		++x;

		x += spacing;
	}
}
int running;
int frames;

void display_frame_count(void){
	char buf[80];
	int buf_len = snprintf(buf, 80, "%d", frames);

	print_layer(1, 2, tdata.ty - 1, buf, buf_len);
}

int main(const int argc, const char** argv){
	frames = 0;

	tdata.tx = 120;
	tdata.ty = 24 / 2;
	tdata.layers = 3;

	init_layers(tdata.layers);
	init_main_menu_options();

	keypad(initscr(), 1);
	curs_set(0);
	timeout(-1);
	noecho();
	// ncurses mouse registration
    mousemask( BUTTON1_CLICKED | BUTTON3_CLICKED | REPORT_MOUSE_POSITION, NULL );

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
		++frames;
		// Prepare for display
		clear_layers();
		// Send in render commands
		fill_layer(0, '#');
		display_domains[domain_stack[cur_domain-1]]();

		display_frame_count();

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
KB_FUNC(mouse_action){
	push_log("Mouse Action!");
	MEVENT event;
	if (getmouse(&event) == OK){
		auto x = event.x;
		auto y = event.y;

		const char* str = [](MEVENT event){
			if (event.bstate & BUTTON1_CLICKED){
				return "BUTTON LEFT";
			}
			else if (event.bstate & BUTTON3_CLICKED){
				return "BUTTON RIGHT";
			}
			else if (event.bstate & REPORT_MOUSE_POSITION){
				return "MOUSE MOVE";
			}

			return "UNKNOWN";
		}(event);

		push_log(": %s at <%d, %d>", str, x, y);
	}
}
/*
MEVENT event;
if( getmouse( &event ) == OK ) {
    rval.type = CATA_INPUT_MOUSE;
    rval.mouse_x = event.x - VIEW_OFFSET_X;
    rval.mouse_y = event.y - VIEW_OFFSET_Y;
    if( event.bstate & BUTTON1_CLICKED ) {
        rval.add_input( MOUSE_BUTTON_LEFT );
    } else if( event.bstate & BUTTON3_CLICKED ) {
        rval.add_input( MOUSE_BUTTON_RIGHT );
    } else if( event.bstate & REPORT_MOUSE_POSITION ) {
        rval.add_input( MOUSE_MOVE );
        if( input_timeout > 0 ) {
            // Mouse movement seems to clear ncurses timeout
            set_timeout( input_timeout );
        }
    } else {
        rval.type = CATA_INPUT_ERROR;
    }
} else {
    rval.type = CATA_INPUT_ERROR;
}
*/