#include "rpgmap.h"
#include "y.tab.h"
#include <bio.h>
#include <String.h>
#include <ctype.h>

static struct {
	char *name;
	int type;
} syminit[] = {
	"floor",	TFLOOR,
	"line",	TLINE,
	"mark",	TMARK,
	"top",	TTOP,
	"bot",	TBOT,
	"bottom",	TBOT,
	"left",	TLEFT,
	"right",	TRIGHT,
};

static Biobuf fin;
static char *filename;
static int lineno;
static String *sbuf;
int nerrors;

void
lexinit(int fd, char *file)
{
	Binit(&fin, fd, OREAD);
	filename = file;
	lineno = 1;
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

int
yylex(void)
{
	int c, i;

	while((c=GETC()) != Beof && isspace(c))
		;
	if(c == Beof)
		return 0;
	if(c == '#'){
		while((c=GETC()) != Beof && c != '\n')
			;
		return yylex();
	}
	s_reset(sbuf);
	if(isdigit(c)){
		s_putc(sbuf, c);
		while((c=GETC()) != Beof && isdigit(c))
			s_putc(sbuf, c);
		s_terminate(sbuf);
		yylval.con = strtol(s_to_c(sbuf), nil, 10);
		return TCONST;
	}

	if(c == '"'){
		while((c=GETC()) != Beof && c != '"')
			s_putc(sbuf, c);
		s_terminate(sbuf);
		yylval.s = strdup(s_to_c(sbuf));
		if(yylval.s == nil)
			sysfatal("strdup: %r");
		return TSTRING;
	}

	if(isalpha(c) || c == '_'){
		s_putc(sbuf, c);
		while((c=GETC()) != Beof && (isalnum(c) || c == '_'))
			s_putc(sbuf, c);
		s_terminate(sbuf);
		for(i = 0; i < nelem(syminit); i++)
			if(strcmp(syminit[i].name, s_to_c(sbuf)) == 0)
				return syminit[i].type;
		return TUNDEF;
	}

	return c;
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
}
