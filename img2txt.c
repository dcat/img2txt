#include <stdbool.h>
#include <locale.h>
#include <wchar.h>
#include <err.h>


#include "arg.h"
#include "chars.h"

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
	/* employ otsu's algorithm */
	int map[16];
	struct rgba *p;
	struct {
		double r, g, b;
	} fg, bg;
	long total, threshold;
	int closest, x, i;
	int score, pscore;

	p = (struct rgba *)buf;
	for (i = total = 0; i < 15; i++, p++)
		total += greyscale(p);

	threshold = total / 16;

	p = (struct rgba *)buf;
	for (i = 0; i < 15; i++, p++)
		map[i] = greyscale(p) >= threshold;

	score = pscore = 0;

	for (i = score = pscore = 0; i < sizeof(table)/sizeof(*table); i++) {
		bzero(map, 16);
		score = 0;

		for (x = 0; x < 15; x++)
			if (map[x] == table[i].pattern[x])
				score++;

		if (score > pscore) {
			pscore = score;
			closest = i;
		}
	}

	for (i = x = 0; i < 15; i++)
		if (map[i])
			x++;

	if (x <= 2) {
		closest = 0;

		for (i = 0; i < 15; i++)
			map[i] = i < 8;

		x = 8;
	}

	bzero(&fg, sizeof(fg));
	bzero(&bg, sizeof(bg));

	p = (struct rgba *)buf;
	for (i = 0; i < 15; i++, p++) {
		if (map[i]) {
			fg.r += p->r;
			fg.g += p->g;
			fg.b += p->b;
		} else {
			bg.r += p->r;
			bg.g += p->g;
			bg.b += p->b;
		}
	}

	ret->fg.r = fg.r / x;
	ret->fg.g = fg.g / x;
	ret->fg.b = fg.b / x;

	ret->bg.r = bg.r / (15 - x);
	ret->bg.g = bg.g / (15 - x);
	ret->bg.b = bg.b / (15 - x);

	ret->reverse = table[closest].reverse;
	ret->chr = table[closest].chr;
	return;
}

struct rgba *
pos(struct img *img, int x, int y) {
	return (struct rgba*)(img->data + ((x + (y * img->w)) * img->chn));
}

struct rgba *
blkpos(struct rgba *ret, struct img *img, int x, int y) {
	int i, j, n;
	struct rgba *p;

	for (p = ret, n = j = 0; j < 4; j++)
		for (i = 0; i < 4; i++, n++)
			*p++ = *pos(img, (x * 4) + i, (y * 4) + j);

	return p;
}

struct img *
resize(struct img *orig, struct img *new, int w, int h) {
	new->data = malloc(orig->chn * w * h);
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

	resize(&raw, &img, width * 4, height * 4);

	for (y = 0; y < img.h / 4; y++) {
		for (x = 0; x < img.w / 4; x++) {
			struct rgba blk[16];
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
