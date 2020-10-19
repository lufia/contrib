#include <u.h>
#include <libc.h>
#include <uuid.h>

int (*gen)(uchar *p);

static void
usage(void)
{
	fprint(2, "usage: %s [-rt]\n", argv0);
	exits("usage");
}

void
main(int argc, char **argv)
{
	uchar uuid[UUIDlen];

	gen = uuidgenrand;
	fmtinstall('U', uuidfmt);
	ARGBEGIN {
	case 'r':
		gen = uuidgenrand;
		break;
	case 't':
		gen = uuidgen;
		break;
	default:
		usage();
	} ARGEND
	if(gen(uuid) < 0)
		sysfatal("cannot generate an uuid: %r");
	print("%U\n", uuid);
	exits(nil);
}
