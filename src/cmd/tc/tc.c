#include "tcode.h"

char dictname[512];

static int
catchnote(void*, char *note)
{
	return strcmp(note, "interrupt") == 0;
}

void
main(int, char**)
{
	int n;
	char buf[1024];

	snprint(dictname, sizeof(dictname),
		"%s/lib/tc/dict", getenv("home"));

	//if(rfork(RFPROC))
	//	exits(nil);		/* parent proc */

	atnotify(catchnote, 1);
	kbdinit();
	dictinit(dictname);

	for(;;)
		if((n=uread(0, buf, sizeof(buf))) > 0)
			write(1, buf, n);
}
