#include "tcode.h"
#include <draw.h>
#include <event.h>

typedef struct Stroke Stroke;

enum {
	Nstroke = 2,
	Kbdsz = 20,
	Fontht = 20,
	EOF = Beof,
};

struct Stroke {
	char c;
	char code;
};

Image *colors[Nstroke];
Image *grey;
char basedir[512];
int clesson;		/* current lesson */
Rectangle crect[2][Nrow][Ncol];
Rectangle textarea;
Rectangle statarea;
Rectangle lessonarea;

static int
getline(Biobufhdr *f, Rune buf[], int n)
{
	long c;
	int i;

	for(i = 0; i < n-1 && (c=Bgetrune(f)) != Beof; i++){
		buf[i] = c;
		if(c == '\n'){
			i++;
			break;
		}
	}
	buf[i] = '\0';
	return i;
}

static Point
e(Point p)
{
	if(p.x > p.y)
		p.x = p.y;
	else
		p.y = p.x;
	if(p.x > Kbdsz)
		p = (Point){Kbdsz, Kbdsz};
	return p;
}

static void
fill(Rectangle r, int pos)
{
	Rectangle b, t;
	Point p;
	int i, j;

	p.x = Dx(r) / Ncol;
	p.y = Dy(r) / Nrow;
	p = e(p);
	b.max.y = r.min.y;
	for(i = 0; i < Nrow; i++){
		b.min.y = b.max.y;
		b.max.y = r.min.y + p.y*(i+1);
		b.max.x = r.min.x;
		for(j = 0; j < Ncol; j++){
			b.min.x = b.max.x;
			b.max.x = r.min.x + p.x*(j+1);
			t = insetrect(b, 1);
			crect[pos][i][j] = t;
			draw(screen, t, grey, nil, ZP);
		}
	}
}

Rectangle
allocarea(Rectangle *r, int n)
{
	Point p;

	r->max.y -= Fontht*n;
	if(r->max.y < 0)
		sysfatal("allocarea: area limit reached");
	p = Pt(r->min.x, r->max.y);
	return Rpt(p, Pt(r->max.x, p.y+Fontht));
}

void
eresized(int new)
{
	Rectangle r, b;

	if(new && getwindow(display, Refnone) < 0)
		sysfatal("getwindow: %r");

	draw(screen, screen->r, display->white, nil, ZP);
	r = insetrect(screen->r, 5);
	b = allocarea(&r, 3);
	textarea = allocarea(&b, 1);
	statarea = allocarea(&b, 1);
	lessonarea = allocarea(&b, 1);

	/* keyboard left */
	b = r;
	b.max.x -= Dx(r)/2;
	fill(b, Left);

	/* keyboard right */
	b = r;
	b.min.x += Dx(r)/2;
	fill(b, Right);

	flushimage(display, 1);
}

static void
setstroke(Stroke p[Nstroke], Rune c)
{
	ushort pair;
	char buf[2];

	if(runetopair(&pair, c) < 0)
		sysfatal("'%C' not exist in dict", c);
	p[0].code = FIRST(pair);
	sprint(buf, "%K", p[0].code);
	p[0].c = buf[0];

	p[1].code = SECOND(pair);
	sprint(buf, "%K", p[1].code);
	p[1].c = buf[0];
}

void
key(Stroke s, Image *color)
{
	Rectangle b;
	int p, r, c;

	p = POS(s.code);
	r = ROW(s.code);
	c = COL(s.code);
	b = crect[p][r][c];
	draw(screen, b, color, nil, ZP);
}

void
text(Rune *s)
{
	draw(screen, textarea, display->white, nil, ZP);
	runestring(screen, textarea.min, display->black, ZP, font, s);
}

void
status(char *s)
{
	draw(screen, statarea, display->white, nil, ZP);
	string(screen, statarea.min, display->black, ZP, font, s);
}

void
lesson(void)
{
	char s[50];

	draw(screen, lessonarea, display->white, nil, ZP);
	snprint(s, sizeof(s), "Lesson %d", clesson);
	string(screen, lessonarea.min, display->black, ZP, font, s);
}

int
clear(Stroke k[], int n)
{
	int i;

	for(i = 0; i < n; i++)
		key(k[i], grey);
	return 0;
}

char *buttons[] = {
	"save",
	"exit",
	nil,
};

Menu menu = {
	buttons,
};

void
save(void)
{
	int fd;
	char s[512];

	snprint(s, sizeof(s), "%s/lesson", basedir);
	fd = create(s, OWRITE, 0644);
	if(fd < 0)
		sysfatal("create: %r");
	snprint(s, sizeof(s), "%d", clesson);
	if(write(fd, s, strlen(s)) < 0)
		sysfatal("write: %r");
	close(fd);
}

int
recovery(void)
{
	int fd, n, r;
	char s[512];

	snprint(s, sizeof(s), "%s/lesson", basedir);
	fd = open(s, OREAD);
	if(fd < 0)
		return 1;

	n = read(fd, s, sizeof(s)-1);
	if(n < 0){
		close(fd);
		return 1;
	}

	s[n] = '\0';
	r = atoi(s);
	if(r == 0)
		r = 1;
	close(fd);
	return r;
}

void
type(Rune c)
{
	Event e;
	Mouse m;
	Stroke k[Nstroke];
	Rune r;
	int i;

	setstroke(k, c);
	for(i = 0; i < Nstroke; ){
		clear(k, i);
		key(k[i], colors[i]);
		switch(event(&e)){
		case Ekeyboard:
			r = e.kbdc;
			if(r == k[i].c){
				status("");
				i++;
			}else{
				status("miss");
				i = clear(k, i+1);
			}
			break;
		case Emouse:
			m = e.mouse;
			switch(m.buttons){
			case 4:
				switch(emenuhit(3, &m, &menu)){
				case 0:
					save();
					break;
				case 1:
					exits(nil);
				}
			}
			break;
		}
	}
	clear(k, i);
}

int
runeatoi(Rune *s)
{
	int rv;

	rv = 0;
	for( ; *s; s++){
		if(*s < '0' || *s > '9')
			break;
		rv = rv*10 + *s-'0';
	}
	return rv;
}

void
main(int, char**)
{
	Biobuf *fin;
	int n, r;
	char buf[512];
	Rune s[1024], *p;

	snprint(basedir, sizeof(basedir), "%s/lib/tc", getenv("home"));
	snprint(buf, sizeof(buf), "%s/dict", basedir);
	kbdinit();
	dictinit(buf);
	fmtinstall('K', codefmt);
	if(initdraw(nil, nil, "tc/owlll") < 0)
		sysfatal("initdraw: %r");
	colors[0] = allocimagemix(display, DRed, DWhite);
	colors[1] = allocimagemix(display, DBlue, DWhite);
	grey = allocimagemix(display, DBlack, DWhite);
	if(colors[0] == nil || colors[1] == nil || grey == nil)
		sysfatal("allocimagemix: %r");
	einit(Emouse|Ekeyboard);

	snprint(buf, sizeof(buf), "%s/owlll", basedir);
	fin = Bopen(buf, OREAD);
	if(fin == nil)
		sysfatal("Bopen: %r");
	eresized(0);
	r = recovery();
	while(n = getline(fin, s, nelem(s))){
		s[n-1] = '\0';
		if(*s == '\f')
			continue;
		if(runestrncmp(s, L"Lesson", 6) == 0){
			if(s[6] == ' ')
				clesson = runeatoi(s+7);
			if(clesson >= r)
				lesson();
			continue;
		}
		if(clesson < r)
			continue;

		for(p = s; *p; p++){
			text(p);
			type(*p);
		}
	}
	Bterm(fin);
	exits(nil);
}
