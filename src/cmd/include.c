#include <u.h>
#include <libc.h>
#include <bio.h>

char *prefix = "@";		/* @include */
char *lookdir[] = {
	nil,		/* before */
	".",		/* current */
	nil,		/* after */
};

int verbose;

void
usage(void)
{
	fprint(2, "include [-abc dir] [-C pref] [file ...]\n");
	exits("usage");
}

char *
abspath(char *path)
{
	char dir[1024], *p;

	if(*path == '/')
		return path;
	else{
		getwd(dir, sizeof(dir));
		p = smprint("%s/%s", dir, path);
		if (p == nil)
			sysfatal("smprint: %r");
		return p;
	}
}

int
isinclude(char *s)
{
	int len;

	len = strlen(prefix);
	if(strncmp(s, prefix, len) == 0)
	if(strcmp(s+len, "include") == 0)
		return 1;
	return 0;
}

void include(char *path);

void
execute(Biobuf *fin)
{
	char *s, *t, *p, bak;

	while(s = Brdline(fin, '\n')){
		s[Blinelen(fin)-1] = '\0';
		t = s + strspn(s, " \t");
		p = strpbrk(t, " \t");
		if(p == nil)
			print("%s\n", s);
		else{
			bak = *p;
			*p = '\0';
			if(isinclude(t))
				include(p+1+strspn(p+1, " \t"));
			else{
				*p = bak;
				print("%s\n", s);
			}
		}
	}
}

void
echdir(char *dir)
{
	if(verbose)
		fprint(2, "chdir %q\n", dir);
	if(chdir(dir) < 0)
		sysfatal("chdir: %r");
}

int
wchdir(char *dir)
{
	int e;
	char *s;

	if(verbose)
		fprint(2, "chdir %q: ", dir);
	e = chdir(dir);
	if(e < 0)
		s = "fail";
	else
		s = "ok";
	if(verbose)
		fprint(2, "%s\n", s);
	return e;
}

int
wopen(char *file, int mode)
{
	int fd;
	char *s;

	if(verbose)
		fprint(2, "open %q: ", file);
	fd = open(file, mode);
	if(fd < 0)
		s = "fail";
	else
		s = "ok";
	if(verbose)
		fprint(2, "%s\n", s);
	return fd;
}

void
basename(char **dir, char **file, char *path)
{
	char *p;

	if(p = strrchr(path, '/')){
		*dir = path;
		*p = '\0';
		*file = p+1;
	}else if (p == path){
		*dir = "/";
		*file = p+1;
	}else{
		*dir = nil;
		*file = path;
	}
}

void
include(char *path)
{
	Biobuf fin;
	int fd, i;
	char bak[1024], *dir, *file;

	fd = -1;
	getwd(bak, sizeof(bak));		/* backup */

	basename(&dir, &file, path);
	for(i = 0; i < nelem(lookdir); i++){
		if(lookdir[i] == nil)
			continue;
		echdir(bak);
		echdir(lookdir[i]);
		if(dir && wchdir(dir) < 0)
			continue;
		fd = wopen(file, OREAD);
		if(fd >= 0)
			break;
	}
	if(fd < 0)
		sysfatal("open: %r");
	Binit(&fin, fd, OREAD);
	execute(&fin);
	close(fd);
	echdir(bak);
}

void
main(int argc, char *argv[])
{
	Biobuf fin;
	int i;
	char *s;

	ARGBEGIN {
	case 'a':
		s = EARGF(usage());
		lookdir[2] = abspath(s);
		break;
	case 'b':
		s = EARGF(usage());
		lookdir[0] = abspath(s);
		break;
	case 'c':
		s = EARGF(usage());
		lookdir[1] = abspath(s);
		break;
	case 'C':
		prefix = EARGF(usage());
		break;
	case 'v':
		verbose = 1;
		break;
	default:
		usage();
	} ARGEND

	quotefmtinstall();
	if(argc == 0){
		Binit(&fin, 0, OREAD);
		execute(&fin);
	}else
		for(i = 0; i < argc; i++)
			include(argv[i]);
	exits(nil);
}
