#include "tcode.h"
#include <draw.h>

typedef struct Keymap Keymap;
struct Keymap {
	Image *img;
	Rune c;
	Rectangle left;
	Rectangle center;
	Rectangle right;
};

extern void initcolors(void);
extern void setkeymap(char *s);
extern Point printpix(Image *img, Rectangle r, Rune *s);
extern Point printrunestr(Image *img, Rectangle r, Rune *s);
extern Rectangle divrow(Rectangle *r, int n);
extern Rectangle divcol(Rectangle *r, int n);
