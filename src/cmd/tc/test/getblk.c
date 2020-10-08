#include "tcode.h"

char fname[] = "dict.sample";

void
main(int, char**)
{
	Stream *fin;
	Block *b;

	fmtinstall('B', blockfmt);
	fin = sopen(fname, OREAD);
	while(b = getblock(fin)){
		print("%B\n", b);
		free(b);
	}
	sclose(fin);
	exits(nil);
}
