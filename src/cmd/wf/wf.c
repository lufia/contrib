#include "wf.h"

Node *root;
int partial;
int debug;
char *lang;
Ext xdb[100];
int nx;

void
usage(void)
{
	fprint(2, "usage: wf [-dp] [-l lang] [-x before after] [file]\n");
	exits("usage");
}

void
main(int argc, char *argv[])
{
	int fd;

	ARGBEGIN {
	case 'd':
		debug++;
		break;
	case 'l':
		lang = EARGF(usage());
		break;
	case 'p':
		partial++;
		break;
	case 'x':
		if(nx >= nelem(xdb))
			sysfatal("too many x options");
		xdb[nx].before = EARGF(usage());
		xdb[nx].after = EARGF(usage());
		nx++;
		break;
	default:
		usage();
	} ARGEND
	quotefmtinstall();
	fmtinstall('T', htmlfmt);
	fmtinstall('A', attrfmt);
	fmtinstall('I', indentfmt);

	if(argc > 1)
		usage();

	if(argc == 0)
		lexinit(0, "<stdin>");
	else{
		fd = open(argv[0], OREAD);
		if(fd < 0)
			sysfatal("open: %r");
		lexinit(fd, argv[0]);
	}
	yyparse();
	if(nerrors > 0)
		exits("parse");
	if(debug)
		dumptree(root, 0);
	root = complex(root);
	simplify(root);
	root = reorder(root, root);
	gen(root);
	exits(nil);
}
