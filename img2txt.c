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


uint64_t
greyscale(struct rgba *c) {
	return (0.3 * c->r) + (0.59 * c->g) + (0.11 * c->b);
}

int
cmpfunc(const void *a, const void *b) {
	return *(int*)a - *(int *)b;
}

void
select_chr(struct cell *ret, struct rgba *buf) {
	/* employ otsu's algorithm */
	long total = 0;
	int i;
	long threshold;
	int map[16];
	struct rgba *p, *search;
	int closest;
	int x;
	int fsum, bsum;
	int n = 0;
	uint64_t medf[16], medb[16];
	struct {
		double r, g, b;
	} fg, bg;

	search = p = (struct rgba *)buf;

	for (i = 0; i < 15; i++, p++) {
		//total += p->r + p->g + p->b;
		total += greyscale(p);
	}

	threshold = total / 15;
	p = (struct rgba *)buf;

	for (i = 0; i < 15; i++, p++)
		map[i] = greyscale(p) >= threshold;

	int score, pscore;
	score = pscore = 0;

	for (i = 0; i < sizeof(table)/sizeof(*table); i++) {
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
		if  (map[i])
			x++;

	//if (pscore < 2) {
	if (x <= 2) {
		closest = 0;

		for (i = 0; i < 15; i++)
			map[i] = i < 8;
	}

	bzero(&fg, sizeof(fg));
	bzero(&bg, sizeof(bg));

	p = (struct rgba *)buf;
	int n1, n2;
	n1 = n2 = 0;
	for (i = 0; i < 15; i++, p++) {
		if (map[i]) {
			fg.r += p->r;
			fg.g += p->g;
			fg.b += p->b;
			medf[n1] = greyscale(p);
			n1++;
		} else {
			bg.r += p->r;
			bg.g += p->g;
			bg.b += p->b;
			medb[n2] = greyscale(p);
			n2++;
		}
	}

	qsort(medf, n1, sizeof(int), cmpfunc);
	qsort(medb, n2, sizeof(int), cmpfunc);


	ret->fg.r = fg.r / n1;
	ret->fg.g = fg.g / n1;
	ret->fg.b = fg.b / n1;

	ret->bg.r = bg.r / n2;
	ret->bg.g = bg.g / n2;
	ret->bg.b = bg.b / n2;

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
	int x, y;

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

		printf("\033[0m");
		putchar('\n');
	}

	return 0;
}
