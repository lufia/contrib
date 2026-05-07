#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>
#include <keyboard.h>
#include <mouse.h>
#include <frame.h>
#include "lib.h"

enum {
	Margin = 4,
	Padding = 4,
	Border = 1,
};

/* Acme colors */
enum {
	C_TextBack = 0xFFFFEAFF,
	C_TextSel  = 0xEEEE9EFF,
	C_Border   = 0x8888CCFF,
};

Rune buf[Maxbuf];
int nbuf;
int icursor;

Rune buf2[Maxbuf];
int nbuf2;
Rune buf3[Maxbuf];
int nbuf3;

Keyboardctl *kc;
Mousectl *mc;

Rectangle r1, r2, r3;
Frame f1, f2, f3;
Image *cols[NCOL];
Image *acme_border;
Image *acme_back;

char **cmdargv;

char *menu[] = {
	"exit",
	nil
};

Menu m = {
	menu
};

void
calculaterects(void)
{
	int h;
	Rectangle r;

	r = screen->r;
	h = (r.max.y - r.min.y) / 3;

	r1 = Rect(r.min.x, r.min.y, r.max.x, r.min.y + h);
	r2 = Rect(r.min.x, r1.max.y, r.max.x, r1.max.y + h);
	r3 = Rect(r.min.x, r2.max.y, r.max.x, r.max.y);

	/* Apply margin */
	r1 = insetrect(r1, Margin);
	r2 = insetrect(r2, Margin);
	r3 = insetrect(r3, Margin);
}

void
initframes(void)
{
	calculaterects();
	frinit(&f1, insetrect(r1, Border + Padding), font, screen, cols);
	frinit(&f2, insetrect(r2, Border + Padding), font, screen, cols);
	frinit(&f3, insetrect(r3, Border + Padding), font, screen, cols);
}

void
redraw(void)
{
	if(screen == nil)
		return;

	draw(screen, screen->r, acme_back, nil, ZP);

	/* Draw borders */
	border(screen, r1, Border, acme_border, ZP);
	border(screen, r2, Border, acme_border, ZP);
	border(screen, r3, Border, acme_border, ZP);

	/* r1: editor */
	frdelete(&f1, 0, f1.nchars);
	frinsert(&f1, buf, buf + nbuf, 0);
	frdrawsel(&f1, frptofchar(&f1, icursor), icursor, icursor, 1);

	/* r2: history 1 */
	frdelete(&f2, 0, f2.nchars);
	if(nbuf2 > 0)
		frinsert(&f2, buf2, buf2 + nbuf2, 0);

	/* r3: history 2 */
	frdelete(&f3, 0, f3.nchars);
	if(nbuf3 > 0)
		frinsert(&f3, buf3, buf3 + nbuf3, 0);

	flushimage(display, 1);
}

void
usage(void)
{
	fprint(2, "usage: %s [-f font] [-l label] [cmd [args...]]\n", argv0);
	threadexitsall("usage");
}

void
output(void)
{
	int p[2];
	int pid, wid;

	if(nbuf == 0)
		return;

	/* shift history */
	memmove(buf3, buf2, nbuf2 * sizeof(Rune));
	nbuf3 = nbuf2;
	memmove(buf2, buf, nbuf * sizeof(Rune));
	nbuf2 = nbuf;

	if(cmdargv == nil || *cmdargv == nil){
		print("%.*S", nbuf, buf);
		return;
	}

	if(pipe(p) < 0)
		sysfatal("pipe: %r");

	pid = fork();
	switch(pid){
	case -1:
		sysfatal("fork: %r");
	case 0:
		close(p[1]);
		dup(p[0], 0);
		close(p[0]);
		exec(cmdargv[0], cmdargv);
		sysfatal("exec: %r");
	default:
		close(p[0]);
		fprint(p[1], "%.*S", nbuf, buf);
		close(p[1]);
		while((wid = waitpid()) != -1 && wid != pid)
			;
	}
}

void
threadmain(int argc, char *argv[])
{
	Rune r;
	char *fontname = nil;
	char *label = "text";
	int i;

	ARGBEGIN{
	case 'f':
		fontname = EARGF(usage());
		break;
	case 'l':
		label = EARGF(usage());
		break;
	default:
		usage();
	}ARGEND

	cmdargv = argv;

	if(initdraw(nil, nil, label) < 0)
		sysfatal("initdraw: %r");

	if(fontname != nil){
		Font *f = openfont(display, fontname);
		if(f == nil)
			sysfatal("openfont: %r");
		font = f;
	}

	acme_back = allocimage(display, Rect(0,0,1,1), screen->chan, 1, C_TextBack);
	acme_border = allocimage(display, Rect(0,0,1,1), screen->chan, 1, C_Border);

	cols[BACK] = acme_back;
	cols[HIGH] = allocimage(display, Rect(0,0,1,1), screen->chan, 1, C_TextSel);
	cols[BORD] = display->black;
	cols[TEXT] = display->black;
	cols[HTEXT] = display->black;

	if((kc = initkeyboard(nil)) == nil)
		sysfatal("initkeyboard: %r");
	if((mc = initmouse(nil, screen)) == nil)
		sysfatal("initmouse: %r");

	initframes();
	redraw();

	enum { AMOUSE, AKBD, ARESIZE, NALT };
	Alt alts[NALT+1];

	alts[AMOUSE].c = mc->c;
	alts[AMOUSE].v = &mc->m;
	alts[AMOUSE].op = CHANRCV;
	alts[AKBD].c = kc->c;
	alts[AKBD].v = &r;
	alts[AKBD].op = CHANRCV;
	alts[ARESIZE].c = mc->resizec;
	alts[ARESIZE].v = nil;
	alts[ARESIZE].op = CHANRCV;
	alts[NALT].op = CHANEND;

	int rev;
	while((rev = alt(alts)) >= 0){
		switch(rev){
		case AMOUSE:
			if(mc->m.buttons & 1){
				if(ptinrect(mc->m.xy, r1)){
					icursor = frcharofpt(&f1, mc->m.xy);
					redraw();
				}
			}else if(mc->m.buttons & 4){
				if(menuhit(3, mc, &m, nil) == 0)
					goto out;
			}
			break;
		case AKBD:
			switch(r){
			case Kdel:
				goto out;
			case Keof:
				output();
				goto out;
			case Kbs:
				if(icursor > 0){
					memmove(buf+icursor-1, buf+icursor, (nbuf-icursor)*sizeof(Rune));
					nbuf--;
					icursor--;
				}
				break;
			case 0x15:	/* Ctrl-U */
				i = icursor;
				while(i > 0 && buf[i-1] != '\n')
					i--;
				memmove(buf+i, buf+icursor, (nbuf-icursor)*sizeof(Rune));
				nbuf -= (icursor - i);
				icursor = i;
				break;
			case 0x0b:	/* Ctrl-K */
				i = icursor;
				while(i < nbuf && buf[i] != '\n')
					i++;
				memmove(buf+icursor, buf+i, (nbuf-i)*sizeof(Rune));
				nbuf -= (i - icursor);
				break;
			case 0x01:	/* Ctrl-A */
				while(icursor > 0 && buf[icursor-1] != '\n')
					icursor--;
				break;
			case 0x05:	/* Ctrl-E */
				while(icursor < nbuf && buf[icursor] != '\n')
					icursor++;
				break;
			case Kleft:
				if(icursor > 0) icursor--;
				break;
			case Kright:
				if(icursor < nbuf) icursor++;
				break;
			case '\n':
				if(nbuf < Maxbuf)
					buf[nbuf++] = '\n';
				output();
				nbuf = 0;
				icursor = 0;
				break;
			case '\r':
				r = '\n';
				/* fall through */
			default:
				if(nbuf < Maxbuf-1){
					memmove(buf+icursor+1, buf+icursor, (nbuf-icursor)*sizeof(Rune));
					buf[icursor] = r;
					nbuf++;
					icursor++;
				}
				break;
			}
			redraw();
			break;
		case ARESIZE:
			if(getwindow(display, Refnone) < 0)
				sysfatal("getwindow: %r");
			initframes();
			redraw();
			break;
		}
	}

out:
	closedisplay(display);
	threadexitsall(nil);
}
