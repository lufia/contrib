#include "tcode.h"

/*
 * isspacerune(2) includes full-width whitespace,
 * and isspace(2), isspace(L'欠') as true is not true.
 */
#define whitespc(c) runestrchr(L" \t\r\n\v\f", (c))

#define ungetc(s) Bungetrune((s)->f)

static long
getc(Stream *fin)	/* get a Rune without isspace(2) chars */
{
	long c;

	while((c=Bgetrune(fin->f)) != Beof && whitespc(c))
		;
	return c;
}

static void
fullread(Stream *fin, Rune buf[], int n)
{
	int i;
	long c;

	for(i = 0; i < n-1 && (c=getc(fin)) != Beof; i++){
		if(c == ':'){
			buf[i] = '\0';
			sysfatal("bad colon near %S", buf);
		}
		buf[i] = c;
	}
	buf[i] = '\0';
	if(i < n-1)
		sysfatal("fullread %d runes near %S", n, buf);
}

static void
splitblocks(Stream *fin, Rune s[Nrow*Ncol*Ncol])
{
	int i, j;

	for(i = 0; i < nelem(fin->b); i++){
		fin->b[i] = emalloc(sizeof(*fin->b[i]));
		fin->b[i]->type = TYP(fin->mode, fin->row, i);
	}

	for(i = 0; i < Nrow; i++)
		for(j = 0; j < Ncol; j++){
			runestrncpy(fin->b[j]->b[i], s, Ncol);
			s += Ncol;
		}
}

static int
rdblocks(Stream *fin)
{
	Rune mode[3], buf[Nrow*Ncol*Ncol+1];
	long c;

again:
	c = getc(fin);
	if(c < 0)
		return -1;
	if(c == ':'){
		fullread(fin, mode, nelem(mode));
		if(runestrcmp(mode, L"LL") == 0)
			fin->mode = LL;
		else if(runestrcmp(mode, L"LR") == 0)
			fin->mode = LR;
		else if(runestrcmp(mode, L"RL") == 0)
			fin->mode = RL;
		else if(runestrcmp(mode, L"RR") == 0)
			fin->mode = RR;
		else
			sysfatal("unknown mode %S", mode);
		fin->row = 0;
		goto again;
	}

	if(fin->row >= Nrow)
		sysfatal("too many rows");
	ungetc(fin);
	fullread(fin, buf, nelem(buf));
	splitblocks(fin, buf);
	fin->row++;
	return 0;
}

Block *
getblock(Stream *fin)
{
	if(fin->i >= nelem(fin->b)){
		if(rdblocks(fin) < 0)
			return nil;
		fin->i = 0;
	}
	return fin->b[fin->i++];
}

static char *mtab[] = {
	[LL] "LL",
	[LR] "LR",
	[RL] "RL",
	[RR] "RR",
};

int
blockfmt(Fmt *fmt)
{
	Block *b;
	int i;
	char m, r, c;

	b = va_arg(fmt->args, Block*);
	m = MODE(b->type);
	r = ROW(b->type);
	c = COL(b->type);
	fmtprint(fmt, "%s:row=%d,col=%d\n", mtab[m], r, c);
	for(i = 0; i < Nrow; i++)
		fmtprint(fmt, "%.*S\n", Ncol, b->b[i]);
	return 0;
}
