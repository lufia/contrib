#include <u.h>
#include <libc.h>

void usage(void);
void check(char *name, char *uid);
void walk(char *name, char *uid);

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
	fprint(2, "usage: %s [-u uid]\n", argv0);
	exits("usage");
}

void
check(char *name, char *uid)
{
	Dir *d;

	d = nil;
	if(uid == nil){
		d = dirstat(name);
		if(d == nil)
			sysfatal("can't retrieve stat: %r");
		uid = d->uid;
	}
	walk(name, uid);
	if(d)
		free(d);
}

void
walk(char *name, char *uid)
{
	long n;
	int fd;
	Dir *d, *p, *ep;
	char wd[256];

	if(getwd(wd, sizeof wd) == nil)
		sysfatal("getwd: %r");
	if(wd[strlen(wd)-1] == '/')
		wd[strlen(wd)-1] = '\0';

	d = dirstat(name);
	if(d == nil)
		sysfatal("dirstat: %r");
	if((d->mode&DMDIR) == 0){
		if(strcmp(d->uid, uid) != 0)
			print("chgrp -u %q %q/%q\n", uid, wd, name);
		free(d);
		return;
	}
	free(d);

	if(chdir(name) < 0)
		sysfatal("chdir: %r");
	fd = open(".", OREAD);
	if(fd < 0)
		sysfatal("open %s/%s: %r", wd, name);
	n = dirreadall(fd, &d);
	if(n < 0)
		sysfatal("dirreadall: %r");
	ep = d + n;
	for(p = d; p < ep; p++){
		if(strcmp(p->name, ".") == 0 || strcmp(p->name, "..") == 0)
			continue;
		walk(p->name, uid);
	}
	free(d);
	close(fd);
	if(chdir(wd) < 0)
		sysfatal("chdir: %r");
}
