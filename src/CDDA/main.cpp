#include <stdio.h>
#include <string.h>

#include <queue>

// Recursive Shadowcasting FOV
#define DIM 121

char map[DIM][DIM] = {0};
unsigned char view[DIM][DIM] = {0};


void put_block(int x, int y, int val = 1){
	map[x][y] = val;
}
void set_view(int x, int y, int val = 255){
	view[x][y] = val;
}
void clear_block(void){
	memset(map, 0, sizeof(map));
}
void clear_view(void){
	memset(view, 0, sizeof(view));
}

bool in_range(int x, int y){
	return ((unsigned)x < DIM) & ((unsigned)y < DIM);
}
unsigned char in_view(int x, int y){
	return view[x][y];
}

bool blocks(int x, int y){
	return map[x][y] > 0;
}

#define MAX(a, b) (a > b ? a : b)
#define MIN(a, b) (a < b ? a : b)

int dist(int x0, int y0, int x1, int y1){
	int dx = MAX(x0, x1) - MIN(x0, x1);
	int dy = MAX(y0, y1) - MIN(y0, y1);

	return MAX(dx, dy);
}

int recurse_depth = 0;
void cast_octant_recursive(int row, int radius, float high, float low, int xx, int xy, int yx, int yy, int startx = 0, int starty = 0){
	float newStart = 0;
	if (high < low){
		return;
	}

	bool blocked = false;
	for (int d = row; d <= radius && !blocked; ++d){
		int dy = -d;
		int set = 0;
		for (int dx = -d; dx <= 0; ++dx){
			int cx = startx + dx * xx + dy * xy;
			int cy = starty + dx * yx + dy * yy;

			float left = (dx - 0.5f) / (dy + 0.5f);
			float right= (dy + 0.5f) / (dy - 0.5f);

			if ((high < right) || !in_range(cx, cy)){ continue; }
			else if (low > left){ break; }

			// Check dist
			int cdist = dist(0, 0, dx, dy);
			if (cdist <= radius){
				int brightness = 255 - (int)(255 * cdist) / radius;
				set_view(cx, cy, brightness);
				++set;
			}

			if (blocked){ // previous cell was blocked
				if (blocks(cx, cy)){
					newStart = right;
					continue;
				}
				else{
					blocked = false;
					high = newStart;
				}
			}
			else if (blocks(cx, cy) && d < radius){
				blocked = true;
				cast_octant_recursive(d + 1, radius, high, left, xx, xy, yx, yy, startx, starty);
				newStart = right;
			}
		}
	}
}


void cast_octant_stackful(int row, int radius, float high, float low, int xx, int xy, int yx, int yy, int startx = 0, int starty = 0){
	// Need to make an artificial "stack frame" to process
	struct frame_t{
		int row;
		float high;
		float low;
	};

	std::queue<frame_t> frames;
	frames.push({row, high, low});

	do{
		frame_t frame = frames.front();
		frames.pop();
		
		float newStart = 0;
		if (frame.high < frame.low){
			continue;
		}

		bool blocked = false;
		for (int d = frame.row; d <= radius && !blocked; ++d){
			int dy = -d;
			for (int dx = -d; dx <= 0; ++dx){
				int cx = startx + dx * xx + dy * xy;
				int cy = starty + dx * yx + dy * yy;

				float left = (dx - 0.5f) / (dy + 0.5f);
				float right= (dy + 0.5f) / (dy - 0.5f);

				if ((frame.high < right) || !in_range(cx, cy)){ continue; }
				else if (frame.low > left){ break; }

				// Check dist
				int cdist = dist(0, 0, dx, dy);
				if (cdist <= radius){
					int brightness = 255 - (int)(255 * cdist) / radius;
					set_view(cx, cy, brightness);
				}

				if (blocked){ // previous cell was blocked
					if (blocks(cx, cy)){
						newStart = right;
						continue;
					}
					else{
						blocked = false;
						frame.high = newStart;
					}
				}
				else if (blocks(cx, cy) && d < radius){
					blocked = true;
					// push a new frame for the recursive section
					frames.push({d + 1, frame.high, left});

					newStart = right;
				}
			}
		}
	}while(frames.size() > 0);
}
void cast_octant_stackful2(int row, int radius, float high, float low, int xx, int xy, int yx, int yy, int startx = 0, int starty = 0){
	// Need to make an artificial "stack frame" to process
	struct frame_t{
		int row;
		float high;
		float low;
	};

	std::queue<frame_t> frames;
	frames.push({row, high, low});

	do{
		frame_t frame = frames.front();
		frames.pop();
		
		float newStart = 0;
		if (frame.high < frame.low){
			continue;
		}

		bool blocked = false;

		for (int d = frame.row; d <= radius && !blocked; ++d){
			int dx = d;
			for (int dy = d; dy >= 0; --dy){
				int cx = startx + dx * xx + dy * xy;
				int cy = starty + dx * yx + dy * yy;

				float left = (dy + 0.5f) / (dx - 0.5f);
				float right= (dy - 0.5f) / (dx + 0.5f);


				if ((frame.high < right) || !in_range(cx, cy)){ continue; }
				else if (frame.low > left){ break; }

				// Check dist
				int cdist = dist(0, 0, dx, dy);
				if (cdist <= radius){
					int brightness = 255 - (int)(255 * cdist) / radius;
					set_view(cx, cy, brightness);
				}

				if (blocked){ // previous cell was blocked
					if (blocks(cx, cy)){
						newStart = right;
						continue;
					}
					else{
						blocked = false;
						frame.high = newStart;
					}
				}
				else if (blocks(cx, cy) && d < radius){
					blocked = true;
					// push a new frame for the recursive section
					frames.push({d + 1, frame.high, left});

					newStart = right;
				}
			}
		}
	}while(frames.size() > 0);
}

void cast_octant_stackful2_int(int row, int radius, int hx, int hy, int lx, int ly, int xx, int xy, int yx, int yy, int startx = 0, int starty = 0){
	// fractions
	struct fraction{
		int y, x;
		fraction(int y, int x):y(y), x(x){}
		fraction(void):y(0),x(0){}
	};
	// Need to make an artificial "stack frame" to process
	struct frame_t{
		int row;
		fraction high, low;
	};

	auto frac_sign = [](fraction a, fraction b){
		auto cross = b.x*a.y - a.x*b.y;

		return (cross > 0) - (cross < 0);
	};

	std::queue<frame_t> frames;
	frames.push({row, fraction(hy, hx), fraction(ly, lx)});

	do{
		frame_t frame = frames.front();
		frames.pop();
		
		fraction newStart;
		if (frac_sign(frame.high, frame.low) < 0){
			continue;
		}

		bool blocked = false;

		for (int d = frame.row; d <= radius && !blocked; ++d){
			int dx = d;
			for (int dy = d; dy >= 0; --dy){
				int cx = startx + dx * xx + dy * xy;
				int cy = starty + dx * yx + dy * yy;

				fraction left(dy*2 + 1, dx*2 - 1);
				fraction right(dy*2 - 1, dx*2 + 1);

				if ((frac_sign(frame.high, right) < 0) || !in_range(cx, cy)){ continue; }
				else if (frac_sign(frame.low, left) > 0){ break; }

				// Check dist
				int cdist = dist(0, 0, dx, dy);
				if (cdist <= radius){
					int brightness = 255 - (int)(255 * cdist) / radius;
					set_view(cx, cy, MAX(brightness, 0));
				}

				if (blocked){ // previous cell was blocked
					if (blocks(cx, cy)){
						newStart = right;
						continue;
					}
					else{
						blocked = false;
						frame.high = newStart;
					}
				}
				else if (blocks(cx, cy) && d < radius){
					blocked = true;
					// push a new frame for the recursive section
					frames.push({d + 1, frame.high, left});

					newStart = right;
				}
			}
		}
	}while(frames.size() > 0);
}

void cast_view(int x, int y, int r){
	const float matrices [8][4] = {
		{ 0, 1, 1, 0},
		{ 1, 0, 0, 1},
		{ 0,-1, 1, 0},
		{-1, 0, 0, 1},
		{ 0, 1,-1, 0},
		{ 1, 0, 0,-1},
		{ 0,-1,-1, 0},
		{-1, 0, 0,-1}
	};
	set_view(x, y, 255);
	for (int i = 0; i < 8; ++i){
		//cast_octant_recursive(1, r, 1, 0, matrices[i][0], matrices[i][1], matrices[i][2], matrices[i][3], x, y);
		//cast_octant_stackful(1, r, 1, 0, matrices[i][0], matrices[i][1], matrices[i][2], matrices[i][3], x, y);
		//cast_octant_stackful2(1, r, 1, 0, matrices[i][0], matrices[i][1], matrices[i][2], matrices[i][3], x, y);
		cast_octant_stackful2_int(1, r, 1, 1, 1, 0, matrices[i][0], matrices[i][1], matrices[i][2], matrices[i][3], x, y);
	}
}

void print_view(int x, int y, char o, char b, char f, char h){
	const char* hex = "0123456789ABCDEF";
	for (int dy = 0; dy < DIM; ++dy){
		printf("   ");
		for (int dx = 0; dx < DIM; ++dx){
			char c = h;
			if (in_view(dx, dy) > 0){
				c = hex[view[dx][dy] / 16];
				if (blocks(dx, dy)){
					c = b;
				}
			}
			if (dx == x && dy == y){
				c = o;
			}
			printf("%c", c);
		}
		printf("\n");
	}
}

void vline(int a, int b, int c, int val){
	// x, y0, y1
	for (int y = b; y <= c; ++y){
		if (in_range(a, y)){
			put_block(a, y, val);
		}
	}
}
void hline(int a, int b, int c, int val){
	// y, x0, x1
	for (int x = b; x <= c; ++x){
		if (in_range(x, a)){
			put_block(x, a, val);
		}
	}
}
void building(int x, int y, int r){
	hline(y+r, x-r, x+r, 1);
	hline(y-r, x-r, x+r, 1);
	vline(x-r, y-r, y+r, 1);
	vline(x+r, y-r, y+r, 1);

	hline(y+r, x-1, x+1, 0);
	hline(y-r, x-1, x+1, 0);
	vline(x-r, y-1, y+1, 0);
	vline(x+r, y-1, y+1, 0);
}

int main(const int argc, const char** argv){
	int ox = DIM/2 - 7, oy = DIM/2, r = DIM/2;
	
	clear_view();
	clear_block();
	building(ox + 7, oy, r / 4);	

	for (int i = 0; i < 1; ++i){
		cast_view(ox, oy, r);
		print_view(ox, oy, '@', '#', ' ', '-');
	}

	return 0;
}