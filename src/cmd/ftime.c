#include <u.h>
#include <libc.h>

enum {
	ATIME = 1<<0,
	MTIME = 1<<1,
};

int flag;

void
usage(void)
{
	fprint(2, "usage: %s [-am] [file ...]\n", argv0);
	exits("usage");
}

int
ftime(char *s, int multi)
{
	Dir *d;
	char *t;

	d = dirstat(s);
	if(d == nil){
		fprint(2, "%s: %s: %r\n", argv0, s);
		return 1;
	}

	t = "";
	if(multi)
		print("%s: ", s);
	if(flag & ATIME){
		print("%satime=%uld", t, d->atime);
		t = " ";
	}
	if(flag & MTIME)
		print("%smtime=%uld", t, d->mtime);
	print("\n");

	free(d);
	return 0;
}

void
main(int argc, char *argv[])
{
	int i, flag1, errs;

	flag = MTIME;
	flag1 = 0;
	ARGBEGIN {
	case 'a':		/* last read */
		flag1 |= ATIME;
		break;
	case 'm':		/* last write */
		flag1 |= MTIME;
		break;
	default:
		usage();
	} ARGEND
	if(flag1)
		flag = flag1;

	errs = 0;
	if(argc == 0)
		errs = ftime(".", 0);
	else
		for(i = 0; i < argc; i++)
			errs |= ftime(argv[i], argc > 1);
	exits(errs? "errors" : nil);
}
