#include <u.h>
#include <libc.h>

void usage(void);
void check(char *name, char *uid);
void walk(char *name, char *p, char *e, char *uid);

void
main(int argc, char **argv)
{
	char *uid;
	int i;

	uid = nil;
	ARGBEGIN {
	case 'u':
		uid = EARGF(usage());
		break;
	default:
		usage();
	} ARGEND
	quotefmtinstall();

	if(argc == 0)
		check(".", uid);
	else
		for(i = 0; i < argc; i++)
			check(argv[i], uid);
	exits(nil);
}

void
usage(void)
{
	fprint(2, "usage: %s [-u uid] [file ...]\n", argv0);
	exits("usage");
}

void
check(char *name, char *uid)
{
	Dir *d;
	char path[1024], *p, *e;

	d = nil;
	if(uid == nil){
		d = dirstat(name);
		if(d == nil)
			sysfatal("can't retrieve stat: %r");
		uid = d->uid;
	}
	e = path + sizeof(path);
	p = strecpy(path, e, name);
	if(*(p-1) == '/')
		*--p = '\0';
	walk(path, p, e, uid);
	if(d)
		free(d);
}

void
walk(char *name, char *p, char *e, char *uid)
{
	long n;
	int fd;
	char *pp;
	Dir *d, *dp, *ep;

	d = dirstat(name);
	if(d == nil)
		sysfatal("dirstat: %r");
	if((d->mode&DMDIR) == 0){
		if(strcmp(d->uid, uid) != 0)
			print("chgrp -u %q %q\n", uid, name);
		free(d);
		return;
	}
	free(d);

	fd = open(name, OREAD);
	if(fd < 0)
		sysfatal("open %q: %r", name);
	n = dirreadall(fd, &d);
	if(n < 0)
		sysfatal("dirreadall: %r");
	close(fd);

	ep = d + n;
	for(dp = d; dp < ep; dp++){
		if(strcmp(dp->name, ".") == 0 || strcmp(dp->name, "..") == 0)
			continue;
		pp = p;
		if(pp >= e)
			sysfatal("%s: name too long", name);
		*pp++ = '/';
		*pp = '\0';
		pp = strecpy(pp, e, dp->name);
		walk(name, pp, e, uid);
		*p = '\0';
	}
	free(d);
}
