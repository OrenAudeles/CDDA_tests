#include <curses.h>
#include <inttypes.h>

#include <vector>

#define ESC 27

struct curses_draw_cmd{
	int32_t x : 12;
	int32_t y : 12;
	int32_t c : 8;
};

struct term_data{
	int16_t tx, ty;
	int16_t layers;
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
	curses_draw_cmd cmd;
	cmd.x = x;
	cmd.y = y;
	cmd.c = c;

	l.push_back(cmd);
}

void fill_layer(int layer, int fill){
	if (layer >= 0 && layer < tdata.layers){
		auto& dlayer = layers[layer];

		for (int y = 0; y < tdata.ty; ++y){
			for (int x = 0; x < tdata.tx; ++x){
				layer_push(dlayer, x, y, fill);
			}
		}
	}
}
void clear_layers(void){
	for (int i = 0; i < tdata.layers; ++i){
		layers[i].clear();
	}
}

int main(const int argc, const char** argv){
	tdata.tx = 80 / 2;
	tdata.ty = 24 / 2;
	tdata.layers = 2;

	init_layers(tdata.layers);

	keypad(initscr(), 1);
	curs_set(0);

	int c = 0;

	do{
		clear_layers();

		fill_layer(0, '#');
		
		render();
	}while((ESC != (c = getch())));



	auto endwin_result = endwin();
	destroy_layers();

	return endwin_result;
}