%{
#include "rpgmap.h"
%}
%union {
	Prog *prog;
	Cmd *cmd;
	int con;
	char *s;
}
%token		TFLOOR TLINE TMARK TTOP TBOT TLEFT TRIGHT TUNDEF
%token	<con>	TCONST
%token	<s>		TSTRING
%type	<prog>	prog
%type	<cmd>	cmd
%type	<con>	place const
%type	<s>		string
%%
prog:
	{
		$$ = nil;
	}
|	prog cmd
	{
		$$ = inst($2);
	}

cmd:
	TFLOOR const const string
	{
		Cmd *p;

		p = new(OFLOOR, $2-1, $3-1);
		p->s = $4;
		$$ = p;
	}
|	TLINE const const place
	{
		Cmd *p;

		p = new(OLINE, $2-1, $3-1);
		p->place = $4;
		switch($4){
		case CBOT:
			p->y++;
			p->place = CTOP;
			break;
		case CRIGHT:
			p->x++;
			p->place = CLEFT;
			break;
		}
		$$ = p;
	}
|	TMARK const const string
	{
		Cmd *p;

		p = new(OMARK, $2-1, $3-1);
		p->s = $4;
		$$ = p;
	}

place:
	TTOP
	{
		$$ = CTOP;
	}
|	TBOT
	{
		$$ = CBOT;
	}
|	TLEFT
	{
		$$ = CLEFT;
	}
|	TRIGHT
	{
		$$ = CRIGHT;
	}

const:
	TCONST

string:
	TSTRING
%%
void *
emalloc(ulong n)
{
	void *p;

	p = malloc(n);
	if(p == nil)
		sysfatal("malloc: %r");
	memset(p, 0, n);
	return p;
}

void
usage(void)
{
	fprint(2, "usage: rpgmap [-o output] [-s subject] [file ...]\n");
	exits("usage");
}

void
main(int argc, char *argv[])
{
	int i, fd;
	char *outfile;

	outfile = nil;
	ARGBEGIN {
	case 'o':
		outfile = EARGF(usage());
		break;
	case 's':
		subject = EARGF(usage());
		break;
	default:
		usage();
	} ARGEND

	for(i = 0; i < argc; i++){
		fd = open(argv[i], OREAD);
		if(fd < 0)
			sysfatal("open: %r");
		lexinit(fd, argv[i]);
		yyparse();
		close(fd);
	}
	if(nerrors > 0)
		exits("error");

	edit();

	if(outfile){
		fd = create(outfile, OWRITE, 0644);
		if(fd < 0)
			sysfatal("create %r");
		dump(fd);
		close(fd);
	}else
		dump(1);
	exits(nil);
}
