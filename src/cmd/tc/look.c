#include "pix.h"
#include <event.h>

Rune *looking;

void
usage(void)
{
	fprint(2, "usage: %s str\n", argv0);
	exits("usage");
}

void
eresized(int new)
{
	if(new && getwindow(display, Refnone) < 0)
		sysfatal("getwindow: %r");
	printpix(screen, screen->r, looking);
	flushimage(display, 1);
}

char *buttons[] = {
	"exit",
	nil,
};

Menu menu = {
	buttons
};

void
main(int argc, char *argv[])
{
	Mouse m;
	char *home;
	char dict[512];

	ARGBEGIN {
	default:
		usage();
	} ARGEND
	if(argc != 1)
		usage();

	home = getenv("home");
	if(home == nil)
		sysfatal("getenv: %r");
	snprint(dict, sizeof(dict), "%s/lib/tc/dict", home);
	free(home);

	kbdinit();
	dictinit(dict);
	fmtinstall('K', codefmt);
	if(initdraw(nil, nil, "tc/owlll") < 0)
		sysfatal("initdraw: %r");
	initcolors();
	setkeymap(argv[0]);
	looking = strtorune(argv[0]);
	einit(Emouse);
	eresized(0);
	for(;;){
		m = emouse();
		switch(m.buttons){
		case 4:
			switch(emenuhit(3, &m, &menu)){
			case 0:
				exits(nil);
			}
			break;
		}
	}
}
