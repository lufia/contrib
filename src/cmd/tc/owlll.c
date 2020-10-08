#include "pix.h"
#include <event.h>
#include <ctype.h>

typedef struct Key Key;

enum {
	Term = '\f',
};

struct Key {
	Rune c;
	char key[2+1];
};

int starting = 1;
int lesson;
Rune *lessonchars;
Key *lessonkeys;
Rune **msg;
int nallocmsg;
int nmsg;
Rectangle lessonarea;
Rectangle pixarea;
Rectangle msgarea;

void
usage(void)
{
	fprint(2, "usage: %s [-l lesson]\n", argv0);
	exits("usage");
}

void
printmsg(Image *img, Rectangle r, Rune *a[], int n)
{
	Point p, min;
	int i;

	min = r.min;
	for(i = 0; i < n; i++){
		p = printrunestr(img, r, a[i]);
		min.x = r.min.x;
		min.y = p.y+font->height;
		if(min.y > r.max.y)
			return;
		r = Rpt(min, r.max);
	}
}

void
shiftmsg(int n)
{
	int i;

	for(i = 0; i < n; i++)
		free(msg[i]);
	nmsg -= n;
	memmove(msg, msg+n, nmsg*sizeof(msg[0]));
}

void
addmsg(char *s)
{
	if(nmsg >= nallocmsg)
		shiftmsg(1);
	msg[nmsg++] = strtorune(s);
	printmsg(screen, msgarea, msg, nmsg);
}

void
resizemsg(void)
{
	int n;

	/* -1: prompt */
	n = Dy(msgarea)/font->height - 1;
	if(nmsg > n)
		shiftmsg(nmsg - n);
	msg = erealloc(msg, n*sizeof(msg[0]));
	nallocmsg = n;
}

void
printlesson(void)
{
	Rectangle r;
	char *p, s[UTFmax*512];
	Rune rune[512];
	int n, i, w;

	snprint(s, sizeof(s), "Lesson %d", lesson);
	for(i=0, p=s; *p; i++, p+=n)
		n = chartorune(&rune[i], p);
	rune[i] = '\0';

	r = lessonarea;
	w = runestringwidth(font, rune);
	r.min.x += (Dx(r)-w)/2;
	printrunestr(screen, r, rune);
}

Rectangle
drawrect(Image *img, Rectangle r)
{
	draw(img, r, display->black, nil, ZP);
	r = insetrect(r, 1);
	draw(img, r, display->white, nil, ZP);
	return r;
}

void
eresized(int new)
{
	Rectangle r, b;

	if(new && getwindow(display, Refnone) < 0)
		sysfatal("getwindow: %r");
	r = screen->r;
	b = divcol(&r, 70);	/* 7:3 */
	b = drawrect(screen, b);
	lessonarea = divrow(&b, 33);
	divrow(&lessonarea, 33);
	printlesson();

	b = divrow(&r, 60);	/* 6:4 */
	msgarea = drawrect(screen, b);
	resizemsg();
	printmsg(screen, msgarea, msg, nmsg);

	pixarea = drawrect(screen, r);
	if(lessonchars)
		printpix(screen, pixarea, lessonchars);
	flushimage(display, 1);
}

void
setlessonkeys(char *s)
{
	Key *k;
	ushort pair;
	char c1, c2;
	int i, n;

	free(lessonkeys);
	n = utflen(s);
	lessonkeys = emalloc((n+1)*sizeof(*lessonkeys));
	for(i = 0; i < n; i++){
		k = &lessonkeys[i];
		s += chartorune(&k->c, s);
		if(runetopair(&pair, k->c) < 0)
			sysfatal("'%C' not found", k->c);
		c1 = FIRST(pair);
		c2 = SECOND(pair);
		sprint(k->key, "%K%K", c1, c2);
	}
	lessonkeys[n].c = '\0';
	strcpy(lessonkeys[n].key, "");
}

void
answer(char buf[], int n, char *s)
{
	Key *k;
	int i, m;

	k = lessonkeys;
	for(i = 0; i+UTFmax < n && *s; i += m, s += 2){
		if(strncmp(s, k->key, 2) == 0)
			m = runetochar(buf+i, &k->c);
		else{
			buf[i] = *s;
			buf[i+1] = *(s+1);
			m = 2;
		}
		if(k->c)		/* a case of k->c as '\0', it's term */
			k++;
	}
	buf[i] = '\0';
}

char *
Bgetline(Biobufhdr *fin)
{
	char *s;

	if(s = Brdline(fin, '\n'))
		s[Blinelen(fin)-1] = '\0';
	return s;
}

void
skiplesson(Biobufhdr *fin)
{
	char *s;
	vlong off;

	/*
	 * Bgetline puts 0 at end of string,
	 * but next Brdline with Bseek will be disturbed by 0.
	 */
	while(lesson < starting && (s = Brdline(fin, '\n')))
		if(*s == Term)
			off = Boffset(fin);
		else if(strncmp(s, "Lesson ", 7) == 0)
			lesson = atoi(s+7);
	if(Bseek(fin, off, 0) < 0)
		sysfatal("Bseek: %r");
}

int
setheader(Biobufhdr *fin)
{
	char *s;

	s = Bgetline(fin);
	if(s == nil)
		return 0;
	if(strncmp(s, "Lesson ", 7) != 0)
		sysfatal("syntax");
	lesson = atoi(s+7);
	printlesson();

	s = Bgetline(fin);
	if(s == nil)
		sysfatal("syntax");
	if(strncmp(s, "Lesson-chars:", 13) != 0)
		sysfatal("syntax");
	s += 13;
	while(isspace(*s))
		s++;
	setkeymap(s);
	free(lessonchars);
	lessonchars = strtorune(s);
	printpix(screen, pixarea, lessonchars);
	return 1;
}

char *buttons[] = {
	"exit",
	nil,
};

Menu menu = {
	buttons
};

Rune
getc(void)		/* Don't return EOF */
{
	Event e;
	Mouse m;

	for(;;)
		switch(event(&e)){
		case Ekeyboard:
			return e.kbdc;
		case Emouse:
			m = e.mouse;
			switch(m.buttons){
			case 4:
				switch(emenuhit(3, &m, &menu)){
				case 0:
					exits(nil);
				}
			}
		}
	return '\0';
}

char *
getline(char buf[], int n)
{
	Rune c;
	int i, len;

	for(i = 0; (c=getc()) != '\n'; i += len){
		len = runelen(c);
		if(i+len > n-1)
			break;
		runetochar(buf+i, &c);
	}
	buf[i] = '\0';
	return buf;
}

int
readtext(Biobufhdr *fin, char *a[], int n)
{
	int i;
	char *s;

	s = nil;
	for(i = 0; i < n && (s = Bgetline(fin)); i++){
		if(*s == Term)
			break;
		a[i] = estrdup(s);
	}
	if(i == n)		/* buffer full */
		while((s = Bgetline(fin)) && *s != Term)
			;
	assert(s == nil || *s == Term);
	return i;
}

void
freetext(char *a[], int n)
{
	int i;

	for(i = 0; i < n; i++)
		free(a[i]);
}

void
practice(Biobufhdr *fin)
{
	char *text[50], buf[512], ans[1024];
	int i, n;

	while(setheader(fin)){
		addmsg("press return");
		getline(buf, sizeof(buf));
		n = readtext(fin, text, nelem(text));
		do{
			for(i = 0; i < n; i++){
				addmsg(text[i]);
				setlessonkeys(text[i]);
				getline(buf, sizeof(buf));
				answer(ans, sizeof(ans), buf);
				addmsg(ans);
			}
			addmsg("try next? [default=n]");
		}while(getc() != 'y');
		freetext(text, n);
	}
}

void
main(int argc, char *argv[])
{
	Biobuf *fin;
	char *home, dict[512], text[512];

	ARGBEGIN {
	case 'l':
		starting = atoi(EARGF(usage()));
		break;
	default:
		usage();
	} ARGEND

	home = getenv("home");
	if(home == nil)
		sysfatal("getenv: %r");
	snprint(dict, sizeof(dict), "%s/lib/tc/dict", home);
	snprint(text, sizeof(text), "%s/lib/tc/owlll", home);
	free(home);

	kbdinit();
	dictinit(dict);
	fmtinstall('K', codefmt);
	if(initdraw(nil, nil, "tc/owlll") < 0)
		sysfatal("initdraw: %r");
	initcolors();
	einit(Emouse|Ekeyboard);

	fin = Bopen(text, OREAD);
	if(fin == nil)
		sysfatal("Bopen: %r");
	skiplesson(fin);
	eresized(0);
	practice(fin);
	Bterm(fin);
	exits(nil);
}
