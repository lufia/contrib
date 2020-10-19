#include <u.h>
#include <libc.h>
#include <uuid.h>

int (*gen)(uchar *p);

static void
usage(void)
{
	fprint(2, "usage: %s [-r]\n", argv0);
	exits("usage");
}

void
main(int argc, char **argv)
{
	uchar uuid[UUIDlen];

	gen = uuidgen;
	fmtinstall('U', uuidfmt);
	ARGBEGIN {
	case 'r':
		gen = uuidgenrand;
		break;
	default:
		usage();
	} ARGEND
	if(gen(uuid) < 0)
		sysfatal("cannot generate an uuid: %r");
	print("%U\n", uuid);
	exits(nil);
}
