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

uint64_t murmur_hash_64(void* key, uint32_t len, uint64_t seed = 0xFEDCBA9876543210);

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

// Key-Action-Mod
// Since the size of this is already 8 Bytes, could just store in a packed U64 and be able to
// do a direct comparison
struct kam_t{
	uint32_t key; // Can't really get around this... Needs to be able to go up to 0x4000011A [31 bits used]
	uint8_t action : 2;
	uint8_t mods   : 6;
};

// Packs up the structure, but harder to do == tests since there is no intrinsic comparators
// outside of memcmp. Not sure which is faster, but IF there are a lot of kam stored this provides
// a small memory use optimization
struct kam_packed_t{
	// First 4 bytes belong to the key
	uint8_t key[4];
	// Last byte belongs to action (PRESS/RELEASE/REPEAT) and mods
	uint8_t action : 2;
	uint8_t mods   : 6;
};

struct kam_blocked_t{
	union{
		uint64_t val;
		struct {
			uint64_t key : 32;
			uint64_t action : 2;
			uint64_t mods : 6;
			uint64_t padding : 24;
		} data;
	};
};

typedef struct {
	uint64_t name;
	command_binding_fn fn;
} command_pair_t;

COMMAND_FUNC(nop){ (void)data; }

bool quit = false;
COMMAND_FUNC(quit){
	quit = true;
}
SDL_EV_FUNC(quit){
	COMMAND_NAME(quit)(0);
}

#define KAM_ACTION_RELEASED 0
#define KAM_ACTION_PRESSED  1
#define KAM_ACTION_REPEAT   2

typedef struct {
	uint64_t kam_key;
	uint64_t bind_name;
} kam_bind_pair_t;

std::vector<kam_bind_pair_t> kam_bindings;
std::vector<command_pair_t> command_bindings;

#define KAM_MOD_SHIFT_SHIFT 0
#define KAM_MOD_CTR_SHIFT   1
#define KAM_MOD_ALT_SHIFT   2
#define KAM_MOD_GUI_SHIFT   3
#define KAM_MOD_MODE_SHIFT  4

#define KAM_MOD_NONE 0
#define KAM_MOD_SHIFT (1 << KAM_MOD_SHIFT_SHIFT)
#define KAM_MOD_CTR   (1 << KAM_MOD_CTR_SHIFT)
#define KAM_MOD_ALT   (1 << KAM_MOD_ALT_SHIFT)
#define KAM_MOD_GUI   (1 << KAM_MOD_GUI_SHIFT)
#define KAM_MOD_MODE  (1 << KAM_MOD_MODE_SHIFT)


int calc_mod_val(uint16_t mods){
	int result = KAM_MOD_NONE;

	result |= (!!(mods & KMOD_SHIFT))<< KAM_MOD_SHIFT_SHIFT;
	result |= (!!(mods & KMOD_CTRL)) << KAM_MOD_CTR_SHIFT;
	result |= (!!(mods & KMOD_ALT) ) << KAM_MOD_ALT_SHIFT;
	result |= (!!(mods & KMOD_GUI) ) << KAM_MOD_GUI_SHIFT;
	result |= (!!(mods & KMOD_MODE)) << KAM_MOD_MODE_SHIFT;

	return result;
}

kam_blocked_t make_kam_block(int key, int action, uint16_t mods){
	kam_blocked_t kam = {0};
	kam.data.key = key;
	kam.data.action = action;
	kam.data.mods = calc_mod_val(mods);
	printf("   KAM: (%d, %d, %d) <%lu, %lu, %lu> | %lu\n", key, action, mods, kam.data.key, kam.data.action, kam.data.mods, kam.val);

	return kam;
}
kam_blocked_t make_kam_block_ex(int key, int action, int mods){
	kam_blocked_t kam = {0};

	kam.data.key = key;
	kam.data.action = action;
	kam.data.mods = mods;

	printf("   KAM_EX: <%lu, %lu, %lu> | %lu\n", kam.data.key, kam.data.action, kam.data.mods, kam.val);
	
	return kam;
}

int get_bound_kam_ndx(uint64_t kam){
	const int sz = kam_bindings.size();
	for (int i = 0; i < sz; ++i){
		if (kam_bindings[i].kam_key == kam){
			return i;
		}
	}
	return sz;
}
int get_bound_command_ndx(uint64_t name){
	const int sz = command_bindings.size();
	for (int i = 0; i < sz; ++i){
		if (command_bindings[i].name == name){
			return i;
		}
	}
	return sz;
}

uint64_t get_bound_kam_or_fail(uint64_t key, uint64_t deffault){
	for (const auto& pair : kam_bindings){
		if (pair.kam_key == key){
			return pair.bind_name;
		}
	}

	return deffault;
}

command_binding_fn get_bound_command_or_fail(uint64_t name, command_binding_fn deffault){
	for (const auto& pair : command_bindings){
		if (pair.name == name){
			return pair.fn;
		}
	}
	return deffault;
}

// Will bind or rebind a kam to a name
void bind_kam(uint64_t kam, uint64_t name){
	printf("Binding KAM[%016lX] to name [%016lX]\n", kam, name);
	size_t i = 0;
	if ((i = get_bound_kam_ndx(kam)) < kam_bindings.size()){
		kam_bindings[i].bind_name = name;
	}
	else{
		kam_bind_pair_t pair;
		pair.kam_key = kam;
		pair.bind_name = name;

		kam_bindings.push_back(pair);		
	}
}



void call_kam(uint64_t kam, void* data = 0){
#if 0
	auto name = get_bound_kam_or_fail(kam, 0);
	if (name > 0){
		printf("Calling KAM[%016lX] bound to name: %016lX\n", kam, name);
		auto fn = get_bound_command_or_fail(name, COMMAND_NAME(nop));

		fn(data);
	}
#else
	get_bound_command_or_fail(get_bound_kam_or_fail(kam, 0), COMMAND_NAME(nop))(data);
#endif
}

// Filter out repeat events
// general case: not a repeat event
// SDL_KEYDOWN
SDL_EV_FUNC(keydown){
	kam_blocked_t kam = {0};

	if (!e.key.repeat){
		printf("KeyDown!        : %d <%d>\n", e.key.timestamp, e.key.state);

		kam = make_kam_block(e.key.keysym.sym, KAM_ACTION_PRESSED, e.key.keysym.mod);

		// This should map into key-bindings, not calling directly
		// if (e.key.keysym.scancode == SDL_SCANCODE_ESCAPE){
		// 	SDL_EV_NAME(quit)(e);
		// }
	}
	else{
		printf("KeyDown_Repeat! : %d <%d>\n", e.key.timestamp, e.key.state);
		kam = make_kam_block(e.key.keysym.sym, KAM_ACTION_PRESSED | KAM_ACTION_REPEAT, e.key.keysym.mod);
	}

	// Call kam
	call_kam(kam.val);
}
// 99% of the time, we will not care at all about keyup events.
// Would be nice if it was like GLFW sending:
// KEY_DOWN [>> KEY_REPEAT] >> KEY_UP
// SDL_KEYUP
SDL_EV_FUNC(keyup){
	printf("KeyUp!          : %d <%d, %d>\n", e.key.timestamp, e.key.state, e.key.repeat);
	kam_blocked_t kam = make_kam_block(e.key.keysym.sym, KAM_ACTION_RELEASED, e.key.keysym.mod);
	call_kam(kam.val);
}
// SDL_DROPFILE
SDL_EV_FUNC(dropfile){
	printf("DropFile!       : %d : %s\n", e.common.timestamp, e.drop.file);
	SDL_free(e.drop.file);
}

// SDL_MOUSEMOTION
SDL_EV_FUNC(mouse_motion){
	printf("%5d: %*.*s | {%s}\n", e.common.timestamp, -30, 30, __FUNCTION_NAME__, "");
}
// SDL_MOUSEBUTTONDOWN
SDL_EV_FUNC(mouse_button_down){
	printf("%5d: %*.*s | {%s}\n", e.common.timestamp, -30, 30, __FUNCTION_NAME__, "");
}
// SDL_MOUSEBUTTONUP
SDL_EV_FUNC(mouse_button_up){
	printf("%5d: %*.*s | {%s}\n", e.common.timestamp, -30, 30, __FUNCTION_NAME__, "");
}
// SDL_MOUSEWHEEL
SDL_EV_FUNC(mouse_wheel){
	printf("%5d: %*.*s | {%s}\n", e.common.timestamp, -30, 30, __FUNCTION_NAME__, "");
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

uint64_t name_hash(const char* str){
	return murmur_hash_64((void*)str, strlen(str));
}

void init_bound_kams(void){
#define BINDEV(k, a, m, n) bind_kam(make_kam_block_ex(k, a, m).val, name_hash(n))
	BINDEV(SDLK_ESCAPE, KAM_ACTION_PRESSED, KAM_MOD_NONE, "quit");
#undef BINDEV
}

void bind_command(uint64_t name, command_binding_fn fn){
	printf("Binding command to name[%016lX]\n", name);
	size_t i = 0;
	if ((i = get_bound_command_ndx(name)) < command_bindings.size()){
		command_bindings[i].fn = fn;
	}
	else{
		command_pair_t pair;
		pair.name = name;
		pair.fn = fn;

		command_bindings.push_back(pair);
	}
}

void init_bound_functions(void){
#define BINDEV(n, fn) bind_command(name_hash(n), COMMAND_NAME(fn))
	BINDEV("quit", quit);
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
	printf("Sizeof(kam_t) = %zu\n", sizeof(kam_t));
	printf("Sizeof(kam_packed_t) = %zu\n", sizeof(kam_packed_t));



	if (!init()){
		printf("Failed to initialize!\n");
	}
	else{
		init_bound_events();
		init_bound_kams();
		init_bound_functions();
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


uint64_t murmur_hash_64(void* key, uint32_t len, uint64_t seed){
	const uint64_t m = 0xc6a4a7935bd1e995ULL;
	const uint32_t r = 47;

	uint64_t h = seed ^ (len * m);

	const uint64_t * data = (const uint64_t *)key;
	const uint64_t * end = data + (len/8);

	while(data != end)
	{
		#ifdef PLATFORM_BIG_ENDIAN
			uint64 k = *data++;
			char *p = (char *)&k;
			char c;
			c = p[0]; p[0] = p[7]; p[7] = c;
			c = p[1]; p[1] = p[6]; p[6] = c;
			c = p[2]; p[2] = p[5]; p[5] = c;
			c = p[3]; p[3] = p[4]; p[4] = c;
		#else
			uint64_t k = *data++;
		#endif

		k *= m;
		k ^= k >> r;
		k *= m;
		
		h ^= k;
		h *= m;
	}

	const unsigned char * data2 = (const unsigned char*)data;

	switch(len & 7)
	{
	case 7: h ^= uint64_t(data2[6]) << 48;
	case 6: h ^= uint64_t(data2[5]) << 40;
	case 5: h ^= uint64_t(data2[4]) << 32;
	case 4: h ^= uint64_t(data2[3]) << 24;
	case 3: h ^= uint64_t(data2[2]) << 16;
	case 2: h ^= uint64_t(data2[1]) << 8;
	case 1: h ^= uint64_t(data2[0]);
			h *= m;
	};
	
	h ^= h >> r;
	h *= m;
	h ^= h >> r;

	return h;
}