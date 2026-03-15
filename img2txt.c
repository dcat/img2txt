#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <wchar.h>
#include <err.h>

#include "arg.h"
#include "glyphs.h"

#define CHUNK_WIDTH 8
#define CHUNK_HEIGHT 8
#define CHUNK_SZ (CHUNK_WIDTH * CHUNK_HEIGHT)

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image.h"
#include "stb_image_resize.h"

struct rgba {
	unsigned char r : 8;
	unsigned char g : 8;
	unsigned char b : 8;
	unsigned char a : 8;
};

struct img {
	unsigned char *data;
	int w, h, x, y, chn;
};

struct cell {
	wchar_t chr;
	struct rgba fg, bg;
	bool reverse;
};


int
greyscale(struct rgba *c) {
	return (0.3 * c->r) + (0.59 * c->g) + (0.11 * c->b);
}

void
select_chr(struct cell *ret, struct rgba *buf) {
	struct rgba *p;
	struct {
		double r, g, b;
	} fg, bg;
	size_t total, threshold;
	int closest, x, i;
	int score, pscore;
	uint64_t map;
	uint8_t grey[CHUNK_SZ];

	p = (struct rgba *)buf;
	for (i = total = 0; i < CHUNK_SZ; i++, p++) {
		grey[i] = greyscale(p);
		total += grey[i];
	}

	threshold = total / CHUNK_SZ;

	map = 0;
	for (i = 0; i < CHUNK_SZ; i++)
		if (grey[i] >= threshold)
			map |= (1ULL << i);

	pscore = -1;
	closest = 0;

	for (i = 0; i < (int)(sizeof(table)/sizeof(*table)); i++) {
		score = __builtin_popcountll(~(map ^ table[i].pattern));
		if (score > pscore) {
			pscore = score;
			closest = i;
		}
	}

	x = __builtin_popcountll(map);

	memset(&fg, 0, sizeof(fg));
	memset(&bg, 0, sizeof(bg));

	p = (struct rgba *)buf;
	for (i = 0; i < CHUNK_SZ; i++, p++) {
		if ((map >> i) & 1) {
			fg.r += p->r;
			fg.g += p->g;
			fg.b += p->b;
		} else {
			bg.r += p->r;
			bg.g += p->g;
			bg.b += p->b;
		}
	}

	ret->fg.r = x ? fg.r / x : 0;
	ret->fg.g = x ? fg.g / x : 0;
	ret->fg.b = x ? fg.b / x : 0;

	ret->bg.r = (CHUNK_SZ - x) ? bg.r / (CHUNK_SZ - x) : 0;
	ret->bg.g = (CHUNK_SZ - x) ? bg.g / (CHUNK_SZ - x) : 0;
	ret->bg.b = (CHUNK_SZ - x) ? bg.b / (CHUNK_SZ - x) : 0;

	ret->reverse = table[closest].reverse;
	ret->chr = table[closest].chr;
}

struct rgba *
pos(struct img *img, int x, int y) {
	return (struct rgba*)(img->data + ((x + (y * img->w)) * img->chn));
}

struct rgba *
blkpos(struct rgba *ret, struct img *img, int x, int y) {
	int i, j;
	struct rgba *p;

	for (p = ret, j = 0; j < CHUNK_HEIGHT; j++)
		for (i = 0; i < CHUNK_WIDTH; i++)
			*p++ = *pos(img, (x * CHUNK_WIDTH) + i, (y * CHUNK_HEIGHT) + j);

	return p;
}

struct img *
resize(struct img *orig, struct img *new, int w, int h) {
	new->data = malloc(4 * w * h);
	if (new->data == NULL)
		err(1, "resize: malloc");

	new->chn = orig->chn;
	new->w = w;
	new->h = h;

	stbir_resize_uint8(orig->data, orig->w, orig->h, 0,
		new->data, new->w, new->h, 0, new->chn);

	return new;
}

int
main(int argc, char **argv) {
	int width, height;
	struct img raw;
	struct img img;
	int x, y;
	char *argv0;

	(void)setlocale(LC_ALL, "");

	width = 40;
	height = 14;

	ARGBEGIN {
	case 'w':
		width = atoi(ARGF());
		break;
	case 'h':
		height = atoi(ARGF());
		break;
	default:
		break;
	} ARGEND

	if (!argc)
		return 1;

	raw.data = stbi_load(*argv, &raw.w, &raw.h, &raw.chn, 0);
	if (raw.data == NULL)
		err(1, "stbi_load");

	resize(&raw, &img, width * CHUNK_WIDTH, height * CHUNK_HEIGHT);
	stbi_image_free(raw.data);

	for (y = 0; y < img.h / CHUNK_HEIGHT; y++) {
		for (x = 0; x < img.w / CHUNK_WIDTH; x++) {
			struct rgba blk[CHUNK_HEIGHT * CHUNK_WIDTH];
			struct cell c;

			blkpos(blk, &img, x, y);
			select_chr(&c, blk);

			printf("\033[%d8;2;%d;%d;%d;%d8;2;%d;%d;%dm%lc",
					c.reverse ? 3 : 4,
					c.bg.r, c.bg.g, c.bg.b,
					c.reverse ? 4 : 3,
					c.fg.r, c.fg.g, c.fg.b,
					c.chr
			);
		}

		puts("\033[0m");
	}

	return 0;
}
