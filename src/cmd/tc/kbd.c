#include "tcode.h"
#include <ctype.h>

static char kbdleft[Nrow][Ncol] = {
	{ '1', '2', '3', '4', '5' },
	{ 'q', 'w', 'e', 'r', 't' },
	{ 'a', 's', 'd', 'f', 'g' },
	{ 'z', 'x', 'c', 'v', 'b' },
};
static char kbdright[Nrow][Ncol] = {
	{ '6', '7', '8', '9', '0' },
	{ 'y', 'u', 'i', 'o', 'p' },
	{ 'h', 'j', 'k', 'l', ';' },
	{ 'n', 'm', ',', '.', '/' },
};

#define ENOUGH(n) (ep-bp >= (n))

static char chartab[1<<8];
static char codetab[1<<8];
static Rune transtab[1<<16];
static ushort pairtab[1<<16];
static int mode;
static int changed;		/* was mode changed? */
static char buf[1024*2];
static char *bp;
static char *ep;

static Rune Unknown = L'■';

void
kbdinit(void)
{
	int i, j;

	for(i = 0; i < Nrow; i++)
		for(j = 0; j < Ncol; j++){
			chartab[CODE(Left, i, j)] = kbdleft[i][j];
			chartab[CODE(Right, i, j)] = kbdright[i][j];
			codetab[kbdleft[i][j]] = CODE(Left, i, j);
			codetab[kbdright[i][j]] = CODE(Right, i, j);
		}
	bp = ep = buf;
}

/*
 * similar to CODE(), exception to check error
 */
int
code(char pos, char row, char col)
{
	if(pos != Left && pos != Right)
		return -1;
	if(row < 0 || row >= Nrow)
		return -1;
	if(col < 0 || col >= Ncol)
		return -1;
	return CODE(pos, row, col);
}

static char
codetochar(int n)
{
	if(n < 0 || n >= sizeof(chartab))
		return '\0';
	else
		return chartab[n];
}

static int
chartocode(char *t, char c)
{
	if(c < 0 || c >= sizeof(codetab))
		return -1;
	else{
		*t = codetab[c];
		return 0;
	}
}

static void
setblock1(char np, Block *b, char c2)
{
	ushort h;
	int i, j;
	char c, t;

	for(i = 0; i < Nrow; i++)
		for(j = 0; j < Ncol; j++){
			c = code(np, i, j);
			h = HASH(codetochar(c), c2);
			transtab[h] = b->b[i][j];
			if(chartocode(&t, c2) >= 0)
				pairtab[b->b[i][j]] = PAIR(c, t);
		}
}

void
setblock(Block *b)
{
	char c, row, col;

	row = ROW(b->type);
	col = COL(b->type);
	switch(MODE(b->type)){
	case LL:
		c = code(Left, row, col);
		c = codetochar(c);
		assert(c >= 0);
		setblock1(Left, b, c);
		break;
	case LR:
		c = code(Right, row, col);
		c = codetochar(c);
		assert(c >= 0);
		setblock1(Left, b, c);
		break;
	case RL:
		c = code(Left, row, col);
		c = codetochar(c);
		assert(c >= 0);
		setblock1(Right, b, c);
		break;
	case RR:
		c = code(Right, row, col);
		c = codetochar(c);
		assert(c >= 0);
		setblock1(Right, b, c);
		break;
	default:
		assert(0);
	}
}

Rune
getrune(char c1, char c2)
{
	Rune c;

	if(c = transtab[HASH(c1, c2)])
		return c;
	else
		return Unknown;
}

static int
readbuf(int fd)
{
	int n, i;

	n = ep - bp;
	memmove(buf, bp, n);
	bp = buf;
	ep = bp + n;
	n = read(fd, ep, buf+sizeof(buf)-ep);
	if(n < 0)
		sysfatal("read: %r");
	ep += n;
	for(i = 0; i < n; i++)
		if(iscntrl(bp[i]))
			return -1;		/* flush */
	return ep - bp;
}

int
codefmt(Fmt *fmt)
{
	int n;
	char c;

	n = va_arg(fmt->args, int);
	c = codetochar(n);
	return fmtprint(fmt, "%c", c);
}

int
runetopair(ushort *p, Rune c)
{
	if(c < 0 || c >= nelem(pairtab))
		return -1;
	else if(c == Unknown || !pairtab[c])
		return -1;
	else{
		*p = pairtab[c];
		return 0;
	}
}

static int
putcode(Fmt *fmt, char code)
{
	char r, c;

	r = ROW(code);
	c = COL(code);
	switch(POS(code)){
	case Left:
		return fmtprint(fmt, "Left:row=%d,col=%d", r, c);
	case Right:
		return fmtprint(fmt, "Right:row=%d,col=%d", r, c);
	default:
		werrstr("unknown position %d", POS(code));
		return -1;
	}
}

int
pairfmt(Fmt *fmt)
{
	Rune r;
	ushort pair;
	char c;

	r = va_arg(fmt->args, uint);
	if(runetopair(&pair, r) < 0)
		return fmtprint(fmt, "BAD");
	else{
		c = FIRST(pair);
		if(putcode(fmt, c) < 0)
			return -1;
		if(fmtprint(fmt, ";") < 0)
			return -1;
		c = SECOND(pair);
		return putcode(fmt, c);
	}
}

static void
changemode(int c)
{
	switch(c){
	case Ktcode:
		mode = Mtcode;
		break;
	case Kascii:
		mode = Mascii;
		break;
	default:
		assert(0);
	}
	changed = 1;
}

static int
aconv(char *s)	/* Ascii mode */
{
	char *t;

	t = s;
	if(ISCKEY(*bp))
		changemode(*bp++);
	else
		*t++ = *bp++;
	return t - s;
}

static int
tconv(char *s)		/* Tcode mode */
{
	Rune c;
	char c1, c2;

	c1 = *bp++;
	if(ISCKEY(c1)){
		changemode(c1);
		return 0;
	}else if(iscntrl(c1)){
		*s = c1;
		return 1;
	}

	c2 = *bp++;
	if(ISCKEY(c2)){
		changemode(c2);
		*s = c1;
		return 1;
	}else if(iscntrl(c2)){
		*s++ = c1;
		*s = c2;
		return 2;
	}

	c = getrune(c1, c2);
	return runetochar(s, &c);
}

struct {
	long n;	/* n stroke */
	int (*conv)(char *s);
} spec[] = {
	{ 1, aconv },
	{ 2, tconv },
};

static int
reads(char s[], int n)
{
	char *p, *e;

	changed = 0;
	p = s;
	e = s + n;
	while(e-p >= UTFmax && ENOUGH(spec[mode].n))
		p += spec[mode].conv(p);
	return p - s;
}

static int
flushbuf(char s[], int n)
{
	int m, rv;

	m = mode;
	mode = Mascii;
	rv = reads(s, n);
	if(!changed)
		mode = m;
	return rv;
}

int
uread(int fd, char s[], int n)
{
	while(!ENOUGH(spec[mode].n))
		if(readbuf(fd) <= 0)
			return flushbuf(s, n);

	return reads(s, n);
}
