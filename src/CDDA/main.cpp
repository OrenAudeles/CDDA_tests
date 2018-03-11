#include <time.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "split_mix64.h"
#include "prng.h"
#include "voronoi.h"

#include "array.h"
#include "hash.h"
#include "memory.h"
#include "file.h"
#include "murmur_hash.h"

template <typename T>
T wrap_range(T min, T max, T val){
	T range = max - min;

	T out_val = val % range;
	return out_val + min;
}

VORDIST_FUNC(sqr2);
VORDIST_FUNC(manhattan);
VORDIST_FUNC(cheb);

struct site_owner_grid{
	typedef uint16_t owner_id;

	owner_id* grid;
	uint32_t grid_side_length;
};

site_owner_grid site_grid_make(uint32_t side_length){
	site_owner_grid result;
	result.grid_side_length = side_length;
	result.grid = new site_owner_grid::owner_id[side_length * side_length];

	memset(result.grid, 0, sizeof(site_owner_grid::owner_id) * side_length * side_length);
	return result;
}
void site_grid_destroy(site_owner_grid& grid){
	grid.grid_side_length = 0;
	delete[] grid.grid;
	grid.grid = nullptr;
}

struct distance_caller{
	VorDistFn fn;

	double call(double x0, double y0, double x1, double y1){
		double dx = x0 - x1;
		double dy = y0 - y1;

		dx = dx > 0 ? dx : -dx;
		dy = dy > 0 ? dy : -dy;

		return fn(dx, dy);
	}
};

struct wrapped_distance_caller{
	VorDistFn fn;

	double call(double x0, double y0, double x1, double y1, double w, double h){
		double dx = x0 - x1;
		double dy = y0 - y1;

		dx = dx > 0 ? dx : -dx;
		dy = dy > 0 ? dy : -dy;

		if (dx > w*0.5){
			dx = w - dx;
		}
		if (dy > h*0.5){
			dy = h - dy;
		}

		return fn(dx, dy);
	}
};

/**
 * - sites MUST be non-null
 * - nsites MUST be >0
 */
site_owner_grid::owner_id nearest_site(const vor_site* sites, uint32_t nsites, vor_site::coord_t x, vor_site::coord_t y, distance_caller caller){
	assert(sites != nullptr);
	assert(nsites > 0);

	site_owner_grid::owner_id result = 0;
	double result_dist = caller.call(sites[0].x, sites[0].y, x, y);

	for (uint32_t i = 1; i < nsites; ++i){
		double idist = caller.call(sites[i].x, sites[i].y, x, y);
		if (result_dist > idist){
			result_dist = idist;
			result = i;
		}
	}

	return result;
}

site_owner_grid::owner_id nearest_site_wrapped(const vor_site* sites, uint32_t nsites, vor_site::coord_t x, vor_site::coord_t y, wrapped_distance_caller caller, vor_site::coord_t max_coord){
	assert(sites != nullptr);
	assert(nsites > 0);
	
	site_owner_grid::owner_id result = 0;
	double result_dist = caller.call(sites[0].x, sites[0].y, x, y, max_coord, max_coord);

	for (uint32_t i = 1; i < nsites; ++i){
		// calculate minimum distance if the entire thing is wrapped in on itself
		double idist = caller.call(sites[i].x, sites[i].y, x, y, max_coord, max_coord);
		if (result_dist > idist){
			result_dist = idist;
			result = i;
		}
	}

	return result;
}

void populate_site_owner_grid(site_owner_grid& grid, VorDistFn dist, const vor_site* sites, uint32_t nsites){
	if (sites == nullptr || nsites == 0){
		return;
	}

	distance_caller caller = {dist};

	for (uint32_t y = 0, ndx = 0; y < grid.grid_side_length; ++y){
		for (uint32_t x = 0; x < grid.grid_side_length; ++x, ++ndx){
			grid.grid[ndx] = nearest_site(sites, nsites, x, y, caller);
		}
	}
}

void populate_site_owner_grid_wrapped(site_owner_grid& grid, VorDistFn dist, const vor_site* sites, uint32_t nsites){
	if (sites == nullptr || nsites == 0){
		return;
	}

	wrapped_distance_caller caller = {dist};

	for (uint32_t y = 0, ndx = 0; y < grid.grid_side_length; ++y){
		for (uint32_t x = 0; x < grid.grid_side_length; ++x, ++ndx){
			grid.grid[ndx] = nearest_site_wrapped(sites, nsites, x, y, caller, grid.grid_side_length);
		}
	}
}

void print_site_owner_grid_region(const site_owner_grid& grid, uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1){
	printf("SITE OWNER GRID REGION <%u, %u> - <%u, %u>\n", x0, y0, x1, y1);

	for (uint32_t y = y0; y < y1; ++y){
		printf("   ");
		for (uint32_t x = x0; x < x1; ++x){
			printf("%02X", grid.grid[x + y * grid.grid_side_length]);
		}
		printf("\n");
	}
}

void populate_site_owner_grid_region(site_owner_grid& grid, VorDistFn dist, const vor_site* sites, uint32_t nsites, uint32_t rx, uint32_t ry, uint32_t rx1, uint32_t ry1){
	if (sites == nullptr || nsites == 0){
		return;
	}

	printf("Populating Sites: <%u, %u> - <%u, %u>\n", rx, ry, rx1, ry1);

	distance_caller caller = {dist};

	for (uint32_t y = ry; y < ry1; ++y){
		uint32_t yy = y * grid.grid_side_length;
		for (uint32_t x = rx; x < rx1; ++x){
			grid.grid[x + yy] = nearest_site(sites, nsites, x, y, caller);
		}
	}

	//print_site_owner_grid_region(grid, rx, ry, rx1, ry1);
}

void print_site_owner_grid(const site_owner_grid& grid, const char* header){
	printf("SITE OWNER GRID: %s\n", header);
	for (uint32_t y = 0, ndx = 0; y < grid.grid_side_length; ++y){
		printf("   ");
		for (uint32_t x = 0; x < grid.grid_side_length; ++x, ++ndx){
			printf("%02X", grid.grid[ndx]);
		}
		printf("\n");
	}
}

struct color3{
	uint8_t c[3];
};

void write_image(const site_owner_grid& grid, const vor_site* sites, const color3* colors, const uint32_t nsites, const char* filename){
	/** stretchy */ char* img_data = nullptr;
	
	// Put header into img_data
	char header[128] = {0};
	int header_len = snprintf(header, 128, "P6\n%d %d\n255\n", grid.grid_side_length, grid.grid_side_length);

	array_grow(img_data, header_len);
	array_header(img_data)->size = header_len;
	memcpy(img_data, header, header_len);

	// Generate image data

	color3* pixels = (color3*)(img_data + header_len);
	// Grow img_data to support n pixels
	int pixel_bytes = sizeof(color3) * grid.grid_side_length * grid.grid_side_length;
	array_grow(img_data, pixel_bytes);
	array_header(img_data)->size += pixel_bytes;

	for (uint32_t y = 0, ndx = 0; y < grid.grid_side_length; ++y){
		for (uint32_t x = 0; x < grid.grid_side_length; ++x, ++ndx){
			pixels[ndx] = colors[grid.grid[ndx]];
		}
	}

	color3 blk = {0};

	for (uint32_t ndx = 0; ndx < nsites; ++ndx){
		const vor_site site = sites[ndx];

		int yy, xx;
		for (int y = -1; y < 2; ++y){
			yy = site.y + y;
			
			for (int x = -1; x < 2; ++x){
				xx = site.x + x;
				
				pixels[xx + yy * grid.grid_side_length] = blk;
			}
		}
	}

	// Write image!
	file_write(filename, img_data, array_header(img_data)->size);

	array_free(img_data);
}

struct image_region{
	/* stretchy */ char* data;
	uint32_t x, y;
	uint32_t w, h;
};

struct image_bin{
	/* stretchy */ char* data;
	color3* pixel_start;

	uint32_t w, h;
};

struct image_blk{
	color3* pixel;
	uint32_t w, h;
	uint32_t stride;
};



image_bin image_create(uint32_t w, uint32_t h){
	image_bin result = {0};

	result.w = w;
	result.h = h;

	// add header
	char header[128] = {0};
	int header_len = snprintf(header, 128, "P6\n%d %d\n255\n", w, h);

	array_grow(result.data, header_len);
	array_header(result.data)->size = header_len;
	memcpy(result.data, header, header_len);

	result.pixel_start = (color3*)(result.data + array_header(result.data)->size);
	array_grow(result.data, sizeof(color3) * w * h);
	array_header(result.data)->size += sizeof(color3) * w * h;
	memset(result.pixel_start, 0, sizeof(color3) * w * h);

	return result;
}
void image_destroy(image_bin& img){
	printf("Image Destroy: %p\n", (void*)array_header(img.data));
	array_free(img.data);
	img = {0};
}

void image_write(const image_bin& img, const char* filename){
	file_write(filename, img.data, array_header(img.data)->size);
}

image_blk image_blk_create(const image_bin img, uint32_t x, uint32_t y, uint32_t w, uint32_t h){
	image_blk result = {0};

	result.stride = img.w - w;
	result.w = w;
	result.h = h;

	result.pixel = img.pixel_start + (x + y * img.w);

	return result;
}

image_region image_region_make(const site_owner_grid& grid, const vor_site* sites, const color3* colors, const uint32_t nsites, const uint32_t x, const uint32_t y, const uint32_t w, const uint32_t h){
	image_region result = {0};

	result.x = x;
	result.y = y;
	result.w = w;
	result.h = h;

	// Grow to fit pixels
	array_grow(result.data, sizeof(color3) * result.w * result.h);
	array_header(result.data)->size += result.w * result.h * sizeof(color3);

	color3* pixels = (color3*)result.data;
	for (uint32_t yy = 0, ndx = 0; yy < h; ++yy){
		for (uint32_t xx = 0; xx < w; ++xx, ++ndx){
			pixels[ndx] = colors[grid.grid[x + xx + (y + yy) * grid.grid_side_length]];
		}
	}

	return result;
}
void image_region_destroy(image_region& region){
	array_free(region.data);
	region.data = nullptr;
}

void image_region_write(const image_region* regions, uint32_t nregions, const char* filename, const uint32_t w, const uint32_t h){
	printf("Image Write <%s>\n", filename);
	/** stretchy */ char* img_data = nullptr;
	
	// Put header into img_data
	char header[128] = {0};
	int header_len = snprintf(header, 128, "P6\n%d %d\n255\n", w, h);

	array_grow(img_data, header_len);
	array_header(img_data)->size = header_len;
	memcpy(img_data, header, header_len);

	color3* pixels = (color3*)(img_data + header_len);
	memset(pixels, 0, sizeof(color3) * w * h);

	// Grow array to hold pixel data
	int total_size = w * h * sizeof(color3);
	array_grow(img_data, total_size);
	array_header(img_data)->size += total_size;

	// Sadly can't just lump copy data in :/
	// Gotta copy line-by-line
	for (uint32_t r = 0; r < nregions; ++r){
		const image_region& region = regions[r];
		printf("   Writing Region: <%d, %d>\n", region.x, region.y);

		uint32_t x = region.x;
		color3* rcolor = (color3*)region.data;

		for (uint32_t y = 0; y < region.h; ++y){
			uint32_t ndx = x + (y + region.y) * w;
			memcpy(pixels + ndx, rcolor + y * w, sizeof(color3) * region.w);
		}
	}

	// Write image!
	file_write(filename, img_data, array_header(img_data)->size);

	array_free(img_data);
}

void full_torus(split_mixer mixer){
	printf("Full Torus\n");
	auto prng = prng_make(split_mix_next(&mixer), split_mix_next(&mixer));

	const int N_SITES = 32;

	const int MAX_COORD = 512;
	const int COORD_SPACING = 8;
	const int COORD_POS_MAX = (MAX_COORD / COORD_SPACING);

	const vor_site::coord_t COORD_MIN = 5;
	const vor_site::coord_t COORD_MAX = COORD_POS_MAX - COORD_MIN;

	vor_site sites[N_SITES];
	color3 colors[N_SITES];

	// Set sites
	for (int i = 0; i < N_SITES; ++i){
		sites[i].x = wrap_range<uint32_t>(COORD_MIN, COORD_MAX, prng_next(&prng)) * COORD_SPACING;
		sites[i].y = wrap_range<uint32_t>(COORD_MIN, COORD_MAX, prng_next(&prng)) * COORD_SPACING;

		color3 col = {0};
		col.c[0] = (uint8_t)wrap_range<uint16_t>(0, 256, sites[i].x);
		col.c[1] = (uint8_t)wrap_range<uint16_t>(0, 256, sites[i].y);
		col.c[2] = (uint8_t)wrap_range<uint16_t>(0, 256, prng_next(&prng));
		colors[i] = col;
	}

	// Print sites
	printf("Sites:\n");
	for (int i = 0; i < N_SITES; ++i){
		printf("   [%03d] : <%3d, %3d>\n", i, sites[i].x, sites[i].y);
	}
	printf("Colors:\n");
	for (int i = 0; i < N_SITES; ++i){
		printf("   [%03d] : <%02X, %02X, %02X>\n", i, colors[i].c[0], colors[i].c[1], colors[i].c[2]);
	}

	// I guess I should make a 'map' object to hold all of the data?
	site_owner_grid grid = site_grid_make(MAX_COORD);
	site_owner_grid grid_wrapped = site_grid_make(MAX_COORD);

	populate_site_owner_grid(grid, VORDIST_NAME(sqr2), sites, N_SITES);
	//print_site_owner_grid(grid, "Square Dist");
	write_image(grid, sites, colors, N_SITES, "./output/sqr_dist.pnm");

	populate_site_owner_grid(grid, VORDIST_NAME(manhattan), sites, N_SITES);
	//print_site_owner_grid(grid, "Manhattan Dist");
	write_image(grid, sites, colors, N_SITES, "./output/man_dist.pnm");

	populate_site_owner_grid(grid, VORDIST_NAME(cheb), sites, N_SITES);
	//print_site_owner_grid(grid, "Cheb Dist");
	write_image(grid, sites, colors, N_SITES, "./output/cheb_dist.pnm");

	// Wrapped
	populate_site_owner_grid_wrapped(grid_wrapped, VORDIST_NAME(sqr2), sites, N_SITES);
	//print_site_owner_grid(grid_wrapped, "Square Dist (Wrapped)");
	write_image(grid_wrapped, sites, colors, N_SITES, "./output/sqr_dist_w.pnm");

	populate_site_owner_grid_wrapped(grid_wrapped, VORDIST_NAME(manhattan), sites, N_SITES);
	//print_site_owner_grid(grid_wrapped, "Manhattan Dist (Wrapped)");
	write_image(grid_wrapped, sites, colors, N_SITES, "./output/man_dist_w.pnm");

	populate_site_owner_grid_wrapped(grid_wrapped, VORDIST_NAME(cheb), sites, N_SITES);
	//print_site_owner_grid(grid_wrapped, "Cheb Dist (Wrapped)");
	write_image(grid_wrapped, sites, colors, N_SITES, "./output/cheb_dist_w.pnm");

	site_grid_destroy(grid);
	site_grid_destroy(grid_wrapped);
}

struct Point2{
	int x, y;
};
template <uint32_t SIZE>
struct RegionSite{
	vor_site sites[SIZE];
	color3 colors[SIZE];

	Point2 offset;
};

struct RegionPoint{
	uint16_t x, y;
};
struct RegionData{
	uint32_t first_site, size;
};
struct TorSite{
	uint32_t x, y;
};
struct TorOffsetSite{
	int32_t x, y;
};

RegionData create_region_data(xorshiro128plus& prng, TorSite** sites, color3** colors, uint32_t size, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t ox, uint32_t oy){
	RegionData result = {0};
	result.size = size;
	result.first_site = array_size(*sites);

	/** Generate sites in range [x, x+w], [y, y+h] */
	for (uint32_t n = 0; n < size; ++n){
		TorSite new_site = {0};
		new_site.x = wrap_range<uint32_t>(x, x + w, prng_next(&prng));
		new_site.y = wrap_range<uint32_t>(y, y + h, prng_next(&prng));

		array_push(*sites, new_site);

		color3 new_color = {0};
		/*
		new_color.c[0] = wrap_range<uint16_t>(0, 256, new_site.x + ox);
		new_color.c[1] = wrap_range<uint16_t>(0, 256, new_site.y + oy);
		new_color.c[2] = wrap_range<uint16_t>(0, 256, prng_next(&prng));
		*/
		new_color.c[0] = wrap_range<uint16_t>(64, 64 + 128, result.first_site + n);
		new_color.c[1] = new_color.c[0];
		new_color.c[2] = new_color.c[0];

		array_push(*colors, new_color);
	}

	return result;
}

RegionData get_region_data_or_make(xorshiro128plus& prng, hash_t* region_store, RegionData** regions, RegionPoint rpt, TorSite** sites, color3** colors, uint32_t nsites, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t ox, uint32_t oy){
	RegionData result = {0};
	
	const uint64_t key = murmur_hash_64(&rpt, sizeof(RegionPoint), 0xFEDCBA9876543210);

	const uint64_t lookup_result = hash_lookup(region_store, key, (uint64_t)-1);
	if (lookup_result == (uint64_t)-1){
		printf("GetRegion: Creating Region Data for <%u, %u>\n", rpt.x, rpt.y);
		result = create_region_data(prng, sites, colors, nsites, x, y, w, h, ox, oy);
		array_push(*regions, result);
		hash_add(region_store, key, array_size(*regions) - 1);
	}
	else{
		result = (*regions)[lookup_result];
	}

	return result;
}
uint32_t get_nearest_site_index(const TorOffsetSite* sites, uint32_t nsites, distance_caller caller, uint32_t x, uint32_t y){
	uint32_t result = 0;
	double result_dist = caller.call(sites[0].x, sites[0].y, x, y);

	for (uint32_t i = 1; i < nsites; ++i){
		double idist = caller.call(sites[i].x, sites[i].y, x, y);
		if (result_dist > idist){
			result_dist = idist;
			result = i;
		}
	}
	return result;
}
void generate_region_image_data(image_blk& blk, VorDistFn dist, TorOffsetSite* sites, uint32_t* ids, color3* colors, uint32_t nSites){
	distance_caller caller = {dist};

	color3* cur = blk.pixel;
	for (uint32_t y = 0; y < blk.h; ++y){
		for (uint32_t x = 0; x < blk.w; ++x){
			*cur++ = colors[ids[get_nearest_site_index(sites, nSites, caller, x, y)]];
		}
		cur += blk.stride;
	}
}

void torus_region(split_mixer head_mixer, const int split_factor, const int nSites, const int max_coord, VorDistFn dist){
	const int SITE_SPLIT_FACTOR = split_factor;
	const int NSITE_PER_REGION = (nSites / (SITE_SPLIT_FACTOR * SITE_SPLIT_FACTOR));
	const int NSITE_PER_ITER = NSITE_PER_REGION * 9; // full neighborhood

	const int MAX_COORD = max_coord;
	const int MAX_COORD_REGION = (MAX_COORD / SITE_SPLIT_FACTOR);

	/** stretchy */ TorSite* sites = nullptr;
	/** stretchy */ color3*   colors = nullptr;
	/** stretchy */ RegionData* region_data = nullptr;

	/** stretchy */ uint32_t* iter_site_ids = nullptr;
	/** stretchy */ TorOffsetSite* iter_sites = nullptr;

	array_grow(iter_site_ids, NSITE_PER_ITER); // We'll only ever have this many ids
	array_grow(iter_sites, NSITE_PER_ITER);

	typedef hash_t RegionStore;
	RegionStore regions = hash_create();

	image_bin image = image_create(MAX_COORD, MAX_COORD);
	/** stretchy */ image_blk* blocks = nullptr;

	RegionPoint rpt;
	RegionData rdata;


	const int8_t region_offsets[9][2] = {
		{-1, -1},
		{ 0, -1},
		{+1, -1},

		{-1,  0},
		{ 0,  0},
		{+1,  0},

		{-1, +1},
		{ 0, +1},
		{+1, +1}
	};
	struct{
		uint64_t mixin;
		RegionPoint point;
	} hash_obj;
	hash_obj.mixin = split_mix_next(&head_mixer);

	for (int y = 0; y < SITE_SPLIT_FACTOR; ++y){
		rpt.y = y;
		for (int x = 0; x < SITE_SPLIT_FACTOR; ++x){
			rpt.x = x;

			array_header(iter_site_ids)->size = 0;
			array_header(iter_sites)->size = 0;

			TorOffsetSite offset;
			offset.x = 0;//x * MAX_COORD_REGION;
			offset.y = 0;//y * MAX_COORD_REGION;

			for (int i = 0; i < 9; ++i){
				RegionPoint lrpt;
				lrpt.x = rpt.x + region_offsets[i][0];
				lrpt.y = rpt.y + region_offsets[i][1];

				lrpt.x = wrap_range<uint32_t>(0, SITE_SPLIT_FACTOR, lrpt.x);
				lrpt.y = wrap_range<uint32_t>(0, SITE_SPLIT_FACTOR, lrpt.y);

				hash_obj.point = lrpt;
				auto mixer = split_mix_make(murmur_hash_64(&hash_obj, sizeof(hash_obj), 0xFEDCBA9876543210));
				auto region_prng = prng_make(split_mix_next(&mixer), split_mix_next(&mixer));

				// Get region data and insert ids into set
				//rdata = get_region_data_or_make(region_prng, &regions, &region_data, lrpt, &sites, &colors, NSITE_PER_REGION, lrpt.x * MAX_COORD_REGION, lrpt.y * MAX_COORD_REGION, MAX_COORD_REGION, MAX_COORD_REGION);
				rdata = get_region_data_or_make(region_prng, &regions, &region_data, lrpt, &sites, &colors, NSITE_PER_REGION, 0, 0, MAX_COORD_REGION, MAX_COORD_REGION, x * MAX_COORD_REGION, y * MAX_COORD_REGION);

				// Push all IDs into iteration ids
				for (uint32_t ndx = 0; ndx < rdata.size; ++ndx){
					array_push(iter_site_ids, rdata.first_site + ndx);
				}
				// Push sites to iter_sites
				for (uint32_t ndx = 0; ndx < rdata.size; ++ndx){
					auto site = sites[rdata.first_site + ndx];
					TorOffsetSite osite;
					osite.x = site.x - offset.x + (region_offsets[i][0] * MAX_COORD_REGION);
					osite.y = site.y - offset.y + (region_offsets[i][1] * MAX_COORD_REGION);

					array_push(iter_sites, osite);
				}
			}

			// Print out sites and indices
			if (0){
				printf("[%2d, %2d] Sites and Indices\n", x, y);
				for (int i = 0; i < NSITE_PER_ITER; ++i){
					printf("   (%3d) - I[%3u]: S(%3d, %3d)\n", i, iter_site_ids[i], iter_sites[i].x, iter_sites[i].y);
				}
			}

			// Make the image block
			image_blk blk = image_blk_create(image, x * MAX_COORD_REGION, y * MAX_COORD_REGION, MAX_COORD_REGION, MAX_COORD_REGION);
			// Process block
//void generate_region_image_data(image_blk& blk, VorDistFn dist, TorOffsetSite* sites, uint32_t* ids, color3* colors, uint32_t nSites){
			generate_region_image_data(blk, dist, iter_sites, iter_site_ids, colors, array_size(iter_sites));
		}
	}

	if (0){
		// Print out all Sites and Colors
		const int nsites = array_size(sites);
		for (int i = 0; i < nsites; ++i){
			auto site = sites[i];
			auto color= colors[i];

			printf("- [%2d] S(%3u, %3u) C(%02X, %02X, %02X)\n", i, site.x, site.y, color.c[0], color.c[1], color.c[2]);
		}
	}


	image_write(image, "./output/region_sqr_dist.pnm");
	image_destroy(image);

	(void)NSITE_PER_REGION;
	(void)NSITE_PER_ITER;
	(void)MAX_COORD_REGION;
	(void)rpt;
	(void)rdata;

	hash_destroy(&regions);
	array_free(sites);
	array_free(colors);
	array_free(region_data);
	array_free(iter_site_ids);
	array_free(iter_sites);
	array_free(blocks);
}

// There is a bug 'somewhere' in here...
void torus_quarter(split_mixer head_mixer){
	printf("Quarter Torus\n");

	const int SITE_SPLIT_FACTOR = 2;
	const int SITES_PER_QUARTER = (16/(SITE_SPLIT_FACTOR * SITE_SPLIT_FACTOR));
	const int SITES_PER_ITERATION= SITES_PER_QUARTER * 9;

	const int MAX_COORD = 256;
	const int QUARTER_MAX_COORD = (MAX_COORD / SITE_SPLIT_FACTOR);
	const int COORD_SPACING = 1;
	const int COORD_POS_MAX = (QUARTER_MAX_COORD / COORD_SPACING);

	const vor_site::coord_t COORD_MIN = 2;
	const vor_site::coord_t COORD_MAX = COORD_POS_MAX - COORD_MIN;

	printf("   Sites per Quarter: %d\n   Sites per Iteration: %d\n   MAX_COORD = %d\n   QUARTER_MAX_COORD = %d\n", SITES_PER_QUARTER, SITES_PER_ITERATION, MAX_COORD, QUARTER_MAX_COORD);
	// Each quarter, fill 9 Site sets with data
	// Keep only the output data for sites within the quarter being generated...
	// How tho?
	RegionSite<SITES_PER_QUARTER> region_sites[9];

	vor_site iteration_sites[SITES_PER_ITERATION];
	color3   iteration_colors[SITES_PER_ITERATION];

	site_owner_grid grid = site_grid_make(MAX_COORD);

	image_region img_region[SITE_SPLIT_FACTOR * SITE_SPLIT_FACTOR];

	const int8_t region_offsets[9][2] = {
		{-1, -1},
		{ 0, -1},
		{+1, -1},

		{-1,  0},
		{ 0,  0},
		{+1,  0},

		{-1, +1},
		{ 0, +1},
		{+1, +1}
	};
	struct{
		uint64_t mixin;
		Point2 point;
	} hash_obj;
	hash_obj.mixin = split_mix_next(&head_mixer);

	for (int y = 0; y < SITE_SPLIT_FACTOR; ++y){
		for (int x = 0; x < SITE_SPLIT_FACTOR; ++x){
			// Fill region neighborhood data
			printf("<%d, %d>:\n", x, y);
			for (int r = 0; r < 9; ++r){
				Point2 region;
				region.x = wrap_range<uint32_t>(0, SITE_SPLIT_FACTOR, x + region_offsets[r][0]);
				region.y = wrap_range<uint32_t>(0, SITE_SPLIT_FACTOR, y + region_offsets[r][1]);

				region_sites[r].offset.x = QUARTER_MAX_COORD * region_offsets[r][0];
				region_sites[r].offset.y = QUARTER_MAX_COORD * region_offsets[r][1];

				hash_obj.point = region;

				auto mixer = split_mix_make(murmur_hash_64(&hash_obj, sizeof(hash_obj), 0xFEDCBA9876543210));
				printf("   O<%2d, %2d> R<%d, %d> @ <%4d, %4d> : PRNG[%016lX]\n", region_offsets[r][0], region_offsets[r][1], region.x, region.y, region_sites[r].offset.x, region_sites[r].offset.y, mixer.s);
				auto region_prng = prng_make(split_mix_next(&mixer), split_mix_next(&mixer));


				for (uint32_t ndx = 0; ndx < SITES_PER_QUARTER; ++ndx){
					// make sites
					region_sites[r].sites[ndx].x = wrap_range<uint32_t>(COORD_MIN, COORD_MAX, prng_next(&region_prng)) * COORD_SPACING;
					region_sites[r].sites[ndx].y = wrap_range<uint32_t>(COORD_MIN, COORD_MAX, prng_next(&region_prng)) * COORD_SPACING;

					//Point2 site_pos = {(int)(region_sites[r].sites[ndx].x + region_sites[r].offset.x), (int)(region_sites[r].sites[ndx].y + region_sites[r].offset.y)};

					region_sites[r].colors[ndx].c[0] = (uint8_t)wrap_range<uint16_t>(0, 256, region_sites[r].sites[ndx].x);
					region_sites[r].colors[ndx].c[1] = (uint8_t)wrap_range<uint16_t>(0, 256, region_sites[r].sites[ndx].y);
					region_sites[r].colors[ndx].c[2] = (uint8_t)wrap_range<uint16_t>(0, 256, prng_next(&region_prng));
				}
				// Print sites
				if (0){
					for (uint32_t ndx = 0; ndx < SITES_PER_QUARTER; ++ndx){
						auto site = region_sites[r].sites[ndx];
						auto color= region_sites[r].colors[ndx];
						printf("      [%3u] S<%3d, %3d> C<%02X, %02X, %02X>\n", ndx, site.x, site.y, color.c[0], color.c[1], color.c[2]);
					}
				}
			}

			// Fill sites and colors
			for (int r = 0; r < 9; ++r){
				auto offset = region_sites[r].offset;
				for (int ndx = 0; ndx < SITES_PER_QUARTER; ++ndx){
					auto site = region_sites[r].sites[ndx];
					auto color = region_sites[r].colors[ndx];

					if (0){
						printf("   <%3d, %3d> + <%3d, %3d> = <%3d, %3d>\n", site.x, site.y, offset.x, offset.y, site.x + offset.x, site.y + offset.y);
					}
					iteration_sites[ndx + r * SITES_PER_QUARTER].x = site.x + offset.x;
					iteration_sites[ndx + r * SITES_PER_QUARTER].y = site.y + offset.y;

					iteration_colors[ndx + r * SITES_PER_QUARTER] = color;
				}
			}
			if (0){
				for (uint32_t ndx = 0; ndx < SITES_PER_ITERATION; ++ndx){
					auto site = iteration_sites[ndx];
					auto color= iteration_colors[ndx];

					printf("      [%3u] S<%3d, %3d> C<%02X, %02X, %02X>\n", ndx, site.x, site.y, color.c[0], color.c[1], color.c[2]);
				}
			}

			populate_site_owner_grid_region(grid, VORDIST_NAME(sqr2), iteration_sites, SITES_PER_ITERATION, x * QUARTER_MAX_COORD, y * QUARTER_MAX_COORD, x * QUARTER_MAX_COORD + QUARTER_MAX_COORD, y * QUARTER_MAX_COORD +QUARTER_MAX_COORD);
			// Fill current quarter of site grid
			img_region[x + y * 2] = image_region_make(grid, iteration_sites, iteration_colors, SITES_PER_ITERATION, x * QUARTER_MAX_COORD, y * QUARTER_MAX_COORD, QUARTER_MAX_COORD, QUARTER_MAX_COORD);
		}
	}

	// Write image regions!
	if (MAX_COORD <= 64){
		print_site_owner_grid(grid, "Patch");
	}
	image_region_write(img_region, (SITE_SPLIT_FACTOR * SITE_SPLIT_FACTOR), "./output/patch_1_sqr_dist.pnm", MAX_COORD, MAX_COORD);

	site_grid_destroy(grid);
	for (int i = 0; i < (SITE_SPLIT_FACTOR * SITE_SPLIT_FACTOR); ++i){
		image_region_destroy(img_region[i]);
	}
}

int main(const int argc, const char** argv){
	alloc_main_memory(1024*1024*32); // 32 MiB of memory to play with!
	srand(time(NULL));

	auto mixer = split_mix_make((uint64_t)rand() | ((uint64_t)rand() << 32));

	auto mixer_copy = mixer;
	auto prng = prng_make(split_mix_next(&mixer_copy), split_mix_next(&mixer_copy));


	printf("First 10 values of prng\n");
	for (int i = 0; i < 10; ++i){
		printf("   [%02d] : %016lX\n", i, prng_next(&prng));
	}

	//full_torus(mixer);
	//torus_quarter(mixer);
	// This actually works! (>O.o)>---<x>
	torus_region(mixer, 16, 1024, 2048, VORDIST_NAME(sqr2));

	// reset prng
	mixer_copy = mixer;
	prng = prng_make(split_mix_next(&mixer_copy), split_mix_next(&mixer_copy));

	free_main_memory();
	return 0;
}


VORDIST_FUNC(sqr2){
	return (x * x) + (y * y);
}
VORDIST_FUNC(manhattan){
	x = x > 0 ? x : -x;
	y = y > 0 ? y : -y;
	return (x + y);
}
VORDIST_FUNC(cheb){
	x = x > 0 ? x : -x;
	y = y > 0 ? y : -y;
	return x > y ? x : y;
}