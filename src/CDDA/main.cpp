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

struct color3{
	uint8_t c[3];
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

RegionData create_region_data(xorshiro128plus& prng, TorSite** sites, uint32_t size, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t ox, uint32_t oy){
	RegionData result = {0};
	result.size = size;
	result.first_site = array_size(*sites);

	/** Generate sites in range [x, x+w], [y, y+h] */
	for (uint32_t n = 0; n < size; ++n){
		TorSite new_site = {0};
		new_site.x = wrap_range<uint32_t>(x, x + w, prng_next(&prng));
		new_site.y = wrap_range<uint32_t>(y, y + h, prng_next(&prng));

		array_push(*sites, new_site);
	}

	return result;
}

RegionData get_region_data_or_make(xorshiro128plus& prng, hash_t* region_store, RegionData** regions, RegionPoint rpt, TorSite** sites, uint32_t nsites, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t ox, uint32_t oy){
	RegionData result = {0};
	
	const uint64_t key = murmur_hash_64(&rpt, sizeof(RegionPoint), 0xFEDCBA9876543210);

	const uint64_t lookup_result = hash_lookup(region_store, key, (uint64_t)-1);
	if (lookup_result == (uint64_t)-1){
		printf("GetRegion: Creating Region Data for <%u, %u>\n", rpt.x, rpt.y);
		result = create_region_data(prng, sites, nsites, x, y, w, h, ox, oy);
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

void generate_colors(color3** color, uint32_t nColors, xorshiro128plus prng){
	for (uint32_t i = 0; i < nColors; ++i){
		uint64_t rngval = prng_next(&prng);
		color3 col;
		col.c[0] = (rngval >> 0) & 0xFF;
		col.c[1] = (rngval >> 8) & 0xFF;
		col.c[2] = (rngval >> 16) & 0xFF;

		array_push(*color, col);
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

	{
		auto mixer = split_mix_make(hash_obj.mixin);
		auto color_prng = prng_make(split_mix_next(&mixer), split_mix_next(&mixer));

		generate_colors(&colors, nSites, color_prng);
	}

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
				rdata = get_region_data_or_make(region_prng, &regions, &region_data, lrpt, &sites, NSITE_PER_REGION, 0, 0, MAX_COORD_REGION, MAX_COORD_REGION, x * MAX_COORD_REGION, y * MAX_COORD_REGION);

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
			// Should change this to not always output an image?
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

	hash_destroy(&regions);
	array_free(sites);
	array_free(colors);
	array_free(region_data);
	array_free(iter_site_ids);
	array_free(iter_sites);
	array_free(blocks);
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

	torus_region(mixer, 8, 256, 512, VORDIST_NAME(sqr2));

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