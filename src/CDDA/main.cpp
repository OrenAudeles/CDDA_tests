#include <stdio.h>
#include <SDL.h>
#include <inttypes.h>
#include <vector>

#define WIDTH 640
#define HEIGHT 480

bool init();
void close();

SDL_Window* gWindow = 0;
SDL_Surface* gScreenSurface = 0;

bool init(){
	bool success = true;

	if (SDL_Init(SDL_INIT_VIDEO) < 0){
		printf("SDL_Init Error: %s\n", SDL_GetError());
		success = false;
	}
	else{
		gWindow = SDL_CreateWindow("SDL Tutorial", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, WIDTH, HEIGHT, SDL_WINDOW_SHOWN);
		if (gWindow == 0){
			printf("SDL_CreateWindow Error: Window Could not be created! [%s]\n", SDL_GetError());
			success = false;
		}
		else{
			gScreenSurface = SDL_GetWindowSurface(gWindow);
		}
	}

	return success;
}

void close(){
	SDL_DestroyWindow(gWindow);
	gWindow = 0;
	gScreenSurface = 0;

	SDL_Quit();
}

typedef void (*event_capture_fn)(SDL_Event e);

#define SDL_EV_NAME(name) ev_##name
#define SDL_EV_FUNC(name) void SDL_EV_NAME(name)(SDL_Event e)

bool quit = false;
SDL_EV_FUNC(quit){
	quit = true;
}

// Filter out repeat events
// general case: not a repeat event
SDL_EV_FUNC(keydown){
	if (!e.key.repeat){
		printf("KeyDown!        : %d <%d>\n", e.key.timestamp, e.key.state);
	}
	else{
		printf("KeyDown_Repeat! : %d <%d>\n", e.key.timestamp, e.key.state);
	}
}
// 99% of the time, we will not care at all about keyup events.
// Would be nice if it was like GLFW sending:
// KEY_DOWN [>> KEY_REPEAT] >> KEY_UP
SDL_EV_FUNC(keyup){
	printf("KeyUp!          : %d <%d, %d>\n", e.key.timestamp, e.key.state, e.key.repeat);
}
SDL_EV_FUNC(dropfile){
	printf("DropFile!       : %d\n", e.key.timestamp);
}

typedef struct {
	typeof(SDL_Event::type) type;
	event_capture_fn fn;
} event_capture_pair_t;

std::vector<event_capture_pair_t> bound_events;


void init_bound_events(void){
#define BINDEV(x, y) bound_events.push_back({x, SDL_EV_NAME(y)})
	BINDEV(SDL_KEYDOWN, keydown);
	BINDEV(SDL_KEYUP, keyup);
	BINDEV(SDL_DROPFILE, dropfile);
	BINDEV(SDL_QUIT, quit);
#undef BINDEV
}

void handle_sdl_event(SDL_Event e){
	for (auto& bind : bound_events){
		if (bind.type == e.type){
			bind.fn(e);
			return;
		}
	}
}
void handle_sdl_events(void){
	SDL_Event ev;
	while(SDL_PollEvent(&ev) != 0){
		handle_sdl_event(ev);
	}
}

int main(const int argc, const char** argv){
	if (!init()){
		printf("Failed to initialize!\n");
	}
	else{
		init_bound_events();
		// Main loop
		quit = false;
		while(!quit){
			handle_sdl_events();
			/*
			while(SDL_PollEvent(&e) != 0){
				else if (e.type == SDL_DROPFILE){
					printf(
						"Drop File Event\n"
						" Tp: %u\n"
						" Ts: %u\n"
						" Fn: %s\n"
						" Wn: %u\n",
						e.drop.type,
						e.drop.timestamp,
						e.drop.file,
						e.drop.windowID);
				}
				else if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP){
					SDL_Keysym ks = e.key.keysym;
					printf(
						"Key Event\n"
						" Tp: %u\n"
						" Ts: %u\n"
						" Wn: %u\n"
						" St: %u\n"
						" Rp: %u\n"
						" Ks: <%d, %d, %d>\n",
						e.key.type,
						e.key.timestamp,
						e.key.windowID,
						e.key.state,
						e.key.repeat,
						ks.scancode, ks.sym, ks.mod);
				}

				else{
					printf("Event Type: %X\n", e.type);
				}
			}
			*/

			SDL_UpdateWindowSurface(gWindow);
		}
	}

	close();
	return 0;
}