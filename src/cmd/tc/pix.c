#include "pix.h"

static Keymap *keymap[1<<16];

/*
 * 0: default color
 * 1: first key color
 * 2: second key color
 * 3: doubly position color
 */
static int colorcode[] = { DBlack, DRed, DGreen, DBlue };
static Image *color[4];

static Keymap *
newkeymap(void)
{
	Keymap *k;
	Point p;
	Rectangle r;

	k = emalloc(sizeof(*k));
	/* font->width is used cache only */
	p = Pt(font->height, font->height);
	k->left = Rect(0, 0, p.x, p.y);
	k->center = Rect(p.x, 0, p.x*2, p.y);
	k->right = Rect(p.x*2, 0, p.x*3, p.y);
	r = Rect(0, 0, p.x*3, p.y);
	k->img = allocimage(display, r, screen->chan, 1, DWhite);
	if(k->img == nil)
		sysfatal("allocimage: %r");
	return k;
}

static void
splitrect(Rectangle a[Nrow][Ncol], Rectangle r)
{
	Rectangle b;
	int i, j, row, col;

	row = Dy(r) / Nrow;
	col = Dx(r) / Ncol;
	for(i = 0; i < Nrow; i++)
		for(j = 0; j < Ncol; j++){
			b = Rect(j*col, i*row, (j+1)*col, (i+1)*row);
			a[i][j] = rectaddpt(b, r.min);
		}
}

static Rectangle
switchrect(Keymap *k, int lr)
{
	if(lr == Left)
		return k->left;
	else
		return k->right;
}

static void
countkey(int lr, int a[Nrow][Ncol], char code, int n)
{
	int row, col;

	if(POS(code) != lr)
		return;
	row = ROW(code);
	col = COL(code);
	/* 1st = 1, 2nd = 2, double = 1+2 = 3 */
	a[row][col] += n;
}

static Rectangle
crect(Rectangle r)
{
	Point p, q;

	p.x = Dx(r) / 2;
	p.y = Dy(r) / 2;
	p = addpt(r.min, p);
	q = addpt(p, Pt(1,1));
	return Rpt(p, q);
}

static void
drawkbd(Keymap *k, int lr, ushort pair)
{
	Rectangle r, a[Nrow][Ncol];
	int i, j, b[Nrow][Ncol];

	memset(b, 0, sizeof(b));
	countkey(lr, b, FIRST(pair), 1);
	countkey(lr, b, SECOND(pair), 2);
	splitrect(a, switchrect(k, lr));
	for(i = 0; i < Nrow; i++)
		for(j = 0; j < Ncol; j++){
			r = a[i][j];
			if(b[i][j] == 0)
				r = crect(r);
			else
				r.min = addpt(r.min, Pt(1,1));
			draw(k->img, r, color[b[i][j]], nil, ZP);
		}
}

static void
drawkeymap(Keymap *k)
{
	char s[50];
	ushort pair;

	snprint(s, sizeof(s), "%C", k->c);
	string(k->img, k->center.min, display->black, ZP, font, s);
	if(runetopair(&pair, k->c) < 0){
		fprint(2, "%s: warning: '%C' not found\n", argv0, k->c);
		return;
	}
	drawkbd(k, Left, pair);
	drawkbd(k, Right, pair);
}

static int
chartokeymap(char *s)
{
	Rune c;
	int n;

	if(n = chartorune(&c, s))
		if(keymap[c] == nil){
			keymap[c] = newkeymap();
			keymap[c]->c = c;
			drawkeymap(keymap[c]);
		}
	return n;
}

void
initcolors(void)
{
	Rectangle r;
	int i;

	r = Rect(0, 0, 1, 1);
	for(i = 0; i < nelem(colorcode); i++)
		color[i] = allocimage(display, r, RGB24, 1, colorcode[i]);
}

void
setkeymap(char *s)
{
	while(*s)
		s += chartokeymap(s);
}

Point
printpix(Image *img, Rectangle r, Rune *s)
{
	Keymap *k;
	Rectangle b;
	Point min, max;
	int i, len;

	draw(img, r, display->white, nil, ZP);
	min = r.min;
	len = runestrlen(s);
	for(i = 0; i < len; i++){
		k = keymap[s[i]];
		if(k == nil)
			continue;
		max = addpt(min, k->img->r.max);
		if(max.x > r.max.x){
			min.x = r.min.x;
			min.y = max.y + 5;
			max = addpt(min, k->img->r.max);
		}
		if(max.y > r.max.y)
			return min;
		b = Rpt(min, max);
		draw(img, b, k->img, nil, ZP);
		min.x = b.max.x + 5;
	}
	return min;
}

Point
printrunestr(Image *img, Rectangle r, Rune *s)
{
	Point min, max, p;
	int w;

	draw(img, r, display->white, nil, ZP);
	min = r.min;
	for( ; *s; s++){
		if(*s == '\n'){
			min.x = r.min.x;
			min.y += font->height;
			continue;
		}
		w = runestringnwidth(font, s, 1);
		p = Pt(w, font->height);
		max = addpt(min, p);
		if(max.x > r.max.x){
			min.x = r.min.x;
			min.y = max.y;
			max = addpt(min, p);
		}
		if(max.y > r.max.y)
			return min;
		runestringn(img, min, display->black, ZP, font, s, 1);
		min.x = max.x;
	}
	return min;
}

Rectangle
divrow(Rectangle *r, int n)
{
	Rectangle b;
	int y;

	y = Dy(*r) / 100 * n;
	b = *r;
	r->max.y = r->min.y+y;
	b.min.y = r->max.y;
	return b;
}

Rectangle
divcol(Rectangle *r, int n)
{
	Rectangle b;
	int x;

	x = Dx(*r) / 100 * n;
	b = *r;
	r->max.x = r->min.x+x;
	b.min.x = r->max.x;
	return b;
}
