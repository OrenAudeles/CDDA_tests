#include <stdio.h>
#include <SDL.h>
#include <inttypes.h>
#include <vector>

#ifndef __FUNCTION_NAME__
    #ifdef WIN32   //WINDOWS
        #define __FUNCTION_NAME__   __FUNCTION__  
    #else          //*NIX
        #define __FUNCTION_NAME__   __func__ 
    #endif
#endif

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
typedef void (*command_binding_fn)(void* data);

#define SDL_EV_NAME(name) ev_##name
#define SDL_EV_FUNC(name) void SDL_EV_NAME(name)(SDL_Event e)

#define COMMAND_NAME(name) cmd_##name
#define COMMAND_FUNC(name) void COMMAND_NAME(name)(void* data)


typedef struct {
	typeof(SDL_Event::type) type;
	event_capture_fn fn;
} event_capture_pair_t;

struct key_bind_t{
	// Key
	SDL_Keycode key;
	// Action
	// PRESS/RELEASE/REPEAT only
	uint8_t action;
	// Modifiers
	// Can be a U8 since we can test against:
	// * KMOD_CTRL   (KMOD_LCTRL|KMOD_RCTRL)
	// * KMOD_SHIFT  (KMOD_LSHIFT|KMOD_RSHIFT)
	// * KMOD_ALT    (KMOD_LALT|KMOD_RALT)
	// * KMOD_GUI    (KMOD_LGUI|KMOD_RGUI)
	// If we don't care about caps/num-lock or mode (AltGr)
	// then we can squish this down to 4 bits
	uint16_t mods;
};

typedef struct {
	key_bind_t binding;
	command_binding_fn fn;
} command_pair_t;

bool quit = false;
COMMAND_FUNC(quit){
	quit = true;
}
SDL_EV_FUNC(quit){
	COMMAND_NAME(quit)(0);
}


// Filter out repeat events
// general case: not a repeat event
// SDL_KEYDOWN
SDL_EV_FUNC(keydown){
	if (!e.key.repeat){
		printf("KeyDown!        : %d <%d>\n", e.key.timestamp, e.key.state);

		// This should map into key-bindings, not calling directly
		if (e.key.keysym.scancode == SDL_SCANCODE_ESCAPE){
			SDL_EV_NAME(quit)(e);
		}
	}
	else{
		printf("KeyDown_Repeat! : %d <%d>\n", e.key.timestamp, e.key.state);
	}
}
// 99% of the time, we will not care at all about keyup events.
// Would be nice if it was like GLFW sending:
// KEY_DOWN [>> KEY_REPEAT] >> KEY_UP
// SDL_KEYUP
SDL_EV_FUNC(keyup){
	printf("KeyUp!          : %d <%d, %d>\n", e.key.timestamp, e.key.state, e.key.repeat);
}
// SDL_DROPFILE
SDL_EV_FUNC(dropfile){
	printf("DropFile!       : %d : %s\n", e.common.timestamp, e.drop.file);
	SDL_free(e.drop.file);
}

// SDL_MOUSEMOTION
SDL_EV_FUNC(mouse_motion){
	printf("%*.*s: %d\n", -30, 30, __FUNCTION_NAME__, e.common.timestamp);
}
// SDL_MOUSEBUTTONDOWN
SDL_EV_FUNC(mouse_button_down){
	printf("%*.*s: %d\n", -30, 30, __FUNCTION_NAME__, e.common.timestamp);
}
// SDL_MOUSEBUTTONUP
SDL_EV_FUNC(mouse_button_up){
	printf("%*.*s: %d\n", -30, 30, __FUNCTION_NAME__, e.common.timestamp);
}
// SDL_MOUSEWHEEL
SDL_EV_FUNC(mouse_wheel){
	printf("%*.*s: %d\n", -30, 30, __FUNCTION_NAME__, e.common.timestamp);
}

std::vector<event_capture_pair_t> bound_events;


void init_bound_events(void){
#define BINDEV(x, y) bound_events.push_back({x, SDL_EV_NAME(y)})
	BINDEV(SDL_KEYDOWN, keydown);
	BINDEV(SDL_KEYUP, keyup);
	BINDEV(SDL_DROPFILE, dropfile);
	BINDEV(SDL_QUIT, quit);

	BINDEV(SDL_MOUSEMOTION, mouse_motion);
	BINDEV(SDL_MOUSEBUTTONDOWN, mouse_button_down);
	BINDEV(SDL_MOUSEBUTTONUP, mouse_button_up);
	BINDEV(SDL_MOUSEWHEEL, mouse_wheel);
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