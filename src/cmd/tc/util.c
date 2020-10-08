#include "tcode.h"

void *
emalloc(long n)
{
	void *p;

	p = malloc(n);
	if(p == nil)
		sysfatal("malloc: %r");
	memset(p, 0, n);
	return p;
}

void *
erealloc(void *p, long n)
{
	p = realloc(p, n);
	if(p == nil)
		sysfatal("realloc: %r");
	return p;
}

char *
estrdup(char *s)
{
	char *t;

	t = emalloc(strlen(s)+1);
	strcpy(t, s);
	return t;
}

Rune *
strtorune(char *s)
{
	Rune *rs;
	int i, n;

	n = utflen(s);
	rs = emalloc((n+1)*sizeof(*rs));
	for(i = 0; i < n; i++)
		s += chartorune(&rs[i], s);
	rs[n] = '\0';
	return rs;
}

Stream *
sopen(char *name, int mode)
{
	Biobuf *b;
	Stream *f;

	b = Bopen(name, mode);
	if(b == nil)
		sysfatal("Bopen: %r");

	f = emalloc(sizeof(*f));
	f->i = nelem(f->b);	/* to read in getblock */
	f->f = b;
	return f;
}

void
sclose(Stream *fin)
{
	Bterm(fin->f);
	free(fin);
}

void
dictinit(char *fname)
{
	Stream *fin;
	Block *b;

	fin = sopen(fname, OREAD);
	while(b = getblock(fin)){
		setblock(b);
		free(b);
	}
	sclose(fin);
}
