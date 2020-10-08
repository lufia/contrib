#include "wf.h"
#include "y.tab.h"
#include <bio.h>
#include <String.h>
#include <ctype.h>

static struct {
	char *name;
	int type;
} syminit[] = {
	"title",	Ttitle,
	"tag",	Ttag,
	"script",	Tscript,
	"style",	Tstyle,
	"feed",	Tfeed,
	"url",		Turl,
};

int lineno;
Sym *symlist;
int nerrors;

static int state;
static Biobuf fin;
static char *filename;
static String *sbuf;
static int block;
static int nblock;

char *
estrdup(char *s)
{
	s = strdup(s);
	if(s == nil)
		sysfatal("strdup: %r");
	return s;
}

static Sym *
install(int type, char *name)
{
	Sym *s;

	s = malloc(sizeof(*s));
	if(s == nil)
		sysfatal("malloc: %r");
	s->type = type;
	s->name = estrdup(name);
	s->next = symlist;
	symlist = s;
	return s;
}

Sym *
lookup(char *name)
{
	Sym *s;

	for(s = symlist; s; s = s->next)
		if(strcmp(s->name, name) == 0)
			return s;
	return install(Tmeta, name);
}

void
lexinit(int fd, char *file)
{
	int i;

	state = Head0;
	Binit(&fin, fd, OREAD);
	filename = file;
	lineno = 1;
	symlist = nil;
	for(i = 0; i < nelem(syminit); i++)
		install(syminit[i].type, syminit[i].name);
	sbuf = s_new();
	nerrors = 0;
}

static int
GETC(void)
{
	int c;

	c = Bgetc(&fin);
	if(c == '\n')
		lineno++;
	return c;
}

static void
UNGETC(int c)
{
	if(c < 0)
		return;
	Bungetc(&fin);
	if(c == '\n')
		lineno--;
}

static int
skip(char *s)
{
	int n, c;

	n = 0;
	while((c=GETC()) >= 0 && strchr(s, c))
		n++;
	if(c != Beof)
		UNGETC(c);
	return n;
}

static char *stab[] = {
[Head0]	"Head0",
[Head1]	"Head1",
[Head2]	"Head2",

[Body0]	"Body0",
[Body1]	"Body1",
[Body2]	"Body2",
[Body3]	"Body3",

[Code]	"Code",
[ID]		"ID",
[Class]	"Class",
[Table]	"Table",
[Text]	"Text",
};

static void
setstate(int st)
{
	if(debug)
		fprint(2, "%s -> %s\n", stab[state], stab[st]);
	state = st;
}

static int
fail(int st)
{
	int c;

	while((c=GETC()) >= 0 && c != '\n')
		;
	if(c == '\n')
		UNGETC(c);
	setstate(st);
	return Terror;
}

static int
rreadstr(char *delim, char *s)	/* if s != 0, skip this chars before reading */
{
	int c;

	s_reset(sbuf);
	if(s)
		skip(s);
	while((c=GETC()) >= 0){
		if(strchr(delim, c)){
			UNGETC(c);
			s_terminate(sbuf);
			return Tstring;
		}
		s_putc(sbuf, c);
	}
	if(c == Beof)
		yyerror("eof in string");
	return Terror;
}

static int
readstr(char *delim, char *s, int allow0)
{
	int r;

	r = rreadstr(delim, s);
	if(r == Terror)
		return r;
	if(!allow0 && s_len(sbuf) == 0){
		yyerror("empty string");
		return Terror;
	}
	return r;
}

static int
readname(char *s)
{
	int c;

	s_reset(sbuf);
	if(s)
		skip(s);
	while((c=GETC()) >= 0 && (isalnum(c) || c == '-'))
		s_putc(sbuf, c);
	if(c == Beof){
		yyerror("eof in id/class");
		return Terror;
	}
	UNGETC(c);
	if(s_len(sbuf) == 0){
		yyerror("empty name");
		return Terror;
	}
	s_terminate(sbuf);
	return Tstring;
}

static int
getcc(char *c, char *s)	/* returning inline cmd or 0, c as a text */
{
	int c1, c2;

	c1 = GETC();
	if(c1 == Beof || c1 == '\n')
		return c1;

	if(strchr(s, c1) == nil){
		*c = c1;
		return 0;
	}

	c2 = GETC();
	if(c2 == c1){
		*c = c1;
		return 0;
	}

	UNGETC(c2);
	return c1;
}

char *
ty(int type)
{
	switch(type){
	case Tbegin:	return "begin";
	case Tend:	return "end";
	case Terror:	return "error";
	case Tbreak:	return "break";
	case Ttitle:	return "title";
	case Ttag:		return "tag";
	case Tscript:	return "script";
	case Tstyle:	return "style";
	case Tfeed:	return "feed";
	case Tmeta:	return "meta";
	case Tstring:	return "string";
	case '\n':		return "\\n";
	case '\t':		return "\\t";
	default:		return nil;
	}
}

int
yylex(void)
{
	int c, r;
	char c1;
	Sym *s;

genblock:
	if(block != nblock){
		if(block > nblock){
			if(debug)
				fprint(2, "%s %d -> %d\n", ty(Tend), block, nblock);
			block--;
			return Tend;
		}else{
			if(debug)
				fprint(2, "%s %d -> %d\n", ty(Tbegin), block, nblock);
			block++;
			return Tbegin;
		}
	}

	c = GETC();
	if(c == Beof){
		nblock = 0;
		if(block != nblock)
			goto genblock;
		return 0;
	}
	UNGETC(c);

	switch(state){
	case Head0:			/* [%type] string */
		c = GETC();
		if(c != '%'){
			UNGETC(c);
			setstate(Body0);
			goto genblock;
		}
		r = readname(" \t");
		if(r == Terror)
			return fail(Head2);

		s = lookup(s_to_c(sbuf));
		yylval.sym = s;
		if(debug)
			fprint(2, "Sym[%s] %s\n", ty(s->type), s->name);
		setstate(Head1);
		return s->type;

	case Head1:			/* %type [string] */
		r = readstr("\n", " \t", 0);
		if(r == Terror)
			return fail(Head2);
		yylval.s = estrdup(s_to_c(sbuf));
		if(debug)
			fprint(2, "%s %q\n", ty(Tstring), yylval.s);
		setstate(Head2);
		return Tstring;

	case Head2:
		skip(" \t");
		c = GETC();
		assert(c == '\n' || c == Beof);
		setstate(Head0);
		return ';';

	case Body0:			/* [indent] cmd inline */
		c = GETC();
		if(c == '%'){
			UNGETC(c);
			setstate(Head0);
			goto genblock;
		}
		UNGETC(c);

		r = skip(" \t");
		c = GETC();
		if(c == '\n'){
			if(debug)
				fprint(2, "%s\n", ty(Tbreak));
			return Tbreak;
		}
		UNGETC(c);

		nblock = r;
		setstate(Body1);
		goto genblock;

	case Body1:			/* indent [cmd] inline */
		switch(c = GETC()){
		case Beof:
			yyerror("eof in body");
			if(debug)
				fprint(2, "%s\n", ty(Terror));
			return fail(Body3);
		case '=':
		case '\\':
		case '+':
		case '*':
		case ':':
		case '-':
		case '>':
			if(debug)
				fprint(2, "%c\n", c);
			setstate(Body2);
			return c;
		case '!':
			if(debug)
				fprint(2, "%c\n", c);
			setstate(Code);
			return c;
		case '{':
		case '}':
			if(debug)
				fprint(2, "%c\n", c);
			setstate(Body3);
			return c;
		case '#':
			if(debug)
				fprint(2, "%c\n", c);
			setstate(ID);
			return c;
		case '.':
			if(debug)
				fprint(2, "%c\n", c);
			setstate(Class);
			return c;
		case '|':
			if(debug)
				fprint(2, "%c\n", c);
			setstate(Table);
			return c;
		default:
			UNGETC(c);
			if(debug)
				fprint(2, "\\\n");
			setstate(Body2);
			return '\\';
		}

	case Body2:			/* indent cmd [inline] */
	case Table:
		r = skip(" \t");
		if(state == Table && r > 0)
			return ',';

		switch(c = getcc(&c1, "*[]|<>")){
		case Beof:
			yyerror("eof in body");
			if(debug)
				fprint(2, "%s\n", ty(Terror));
			return fail(Body3);
		case '\n':
			UNGETC(c);
			setstate(Body3);
			break;
		case '*':
		case '[':
		case ']':
		case '|':
		case '<':
		case '>':
			if(c == '[' || c == '|' || c == '<')
				skip(" \t\n");
			if(debug)
				fprint(2, "%c\n", c);
			return c;
		default:
			s_reset(sbuf);
			s_putc(sbuf, c1);
			while((c=getcc(&c1, "*[]|<>")) == 0){
				if(state == Table && c1 == '\t'){
					UNGETC(c);
					break;
				}
				s_putc(sbuf, c1);
			}
			s_terminate(sbuf);
			if(c > 0)
				UNGETC(c);
			yylval.s = estrdup(s_to_c(sbuf));
			if(debug)
				fprint(2, "%s %q\n", ty(Tstring), yylval.s);
			return Tstring;
		}

	case Body3:
		skip(" \t");
		c = GETC();
		if(c != '\n')
			return fail(Body3);
		setstate(Body0);
		return ';';

	case Code:
		r = readstr("\n", "", 1);
		if(r == Terror){
			if(debug)
				fprint(2, "%s\n", ty(Terror));
			return fail(Body3);
		}
		setstate(Body3);
		yylval.s = estrdup(s_to_c(sbuf));
		if(debug)
			fprint(2, "%s %q\n", ty(Tstring), yylval.s);
		return Tstring;

	case ID:
	case Class:
		r = readname(" \t");
		if(r == Terror){
			if(debug)
				fprint(2, "%s\n", ty(Terror));
			return fail(Body3);
		}
		setstate(Body3);
		yylval.s = estrdup(s_to_c(sbuf));
		if(debug)
			fprint(2, "%s %q\n", ty(Tstring), yylval.s);
		return Tstring;

	default:
		assert(0);
		return Terror;
	}
}

void
yyerror(char *fmt, ...)
{
	char buf[1024];
	va_list arg;

	va_start(arg, fmt);
	vseprint(buf, buf+sizeof(buf), fmt, arg);
	va_end(arg);
	fprint(2, "%s:%d %s\n", filename, lineno, buf);
	nerrors++;
	if(nerrors > 10)
		sysfatal("too many errors\n");
}
