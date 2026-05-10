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
	MaxHistory = 10,
};

/* Acme colors */
enum {
	C_TextBack = 0xFFFFEAFF,
	C_TextSel  = 0xEEEE9EFF,
	C_Border   = 0x8888CCFF,
};

typedef struct History History;
struct History {
	Rune buf[Maxbuf];
	int n;
};

Rune buf[Maxbuf];
int nbuf;
int icursor;

History histories[MaxHistory];
int nhistory;

Keyboardctl *kc;
Mousectl *mc;

Rectangle rects[MaxHistory + 1];
Frame frames[MaxHistory + 1];
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

int
countlines(Rune *b, int n, int w)
{
	int nl = 1;
	int x = 0;
	int i;
	Rune rs[2];

	if(w <= 0) return 1;
	rs[1] = 0;
	for(i = 0; i < n; i++){
		if(b[i] == '\n'){
			nl++;
			x = 0;
			continue;
		}
		rs[0] = b[i];
		int lw = runestringsize(font, rs).x;
		if(x + lw > w){
			nl++;
			x = lw;
		} else {
			x += lw;
		}
	}
	return nl;
}

void
calculaterects(void)
{
	int i, n;
	int y;
	int frame_w;

	n = nhistory + 1;
	frame_w = (screen->r.max.x - screen->r.min.x) - (Margin + Border + Padding) * 2;

	y = screen->r.min.y;

	for(i = 0; i < n; i++){
		int nl;
		if(i == 0)
			nl = countlines(buf, nbuf, frame_w);
		else
			nl = countlines(histories[i-1].buf, histories[i-1].n, frame_w);

		int h = nl * font->height + (Border + Padding) * 2;

		rects[i] = Rect(screen->r.min.x + Margin, y + Margin, screen->r.max.x - Margin, y + Margin + h);
		y += h + Margin * 2;
	}
}

void
initframes(void)
{
	int i, n;
	calculaterects();
	n = nhistory + 1;
	for(i = 0; i < n; i++){
		/* Clear old frame content to avoid memory leak */
		if(frames[i].box != nil)
			frclear(&frames[i], 0);
		frinit(&frames[i], insetrect(rects[i], Border + Padding), font, screen, cols);
	}
}

void
redraw(void)
{
	int i, n;
	if(screen == nil)
		return;

	/* In dynamic mode, we re-init frames to adapt to new rectangles */
	initframes();

	draw(screen, screen->r, acme_back, nil, ZP);

	n = nhistory + 1;
	for(i = 0; i < n; i++){
		border(screen, rects[i], Border, acme_border, ZP);

		if(i == 0){
			/* Editor */
			frinsert(&frames[0], buf, buf + nbuf, 0);
			frdrawsel(&frames[0], frptofchar(&frames[0], icursor), icursor, icursor, 1);
		} else {
			/* Histories */
			frinsert(&frames[i], histories[i-1].buf, histories[i-1].buf + histories[i-1].n, 0);
		}
	}

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
	int i;

	if(nbuf == 0)
		return;

	/* shift history */
	if(nhistory < MaxHistory)
		nhistory++;

	for(i = nhistory - 1; i > 0; i--){
		memmove(histories[i].buf, histories[i-1].buf, histories[i-1].n * sizeof(Rune));
		histories[i].n = histories[i-1].n;
	}
	memmove(histories[0].buf, buf, nbuf * sizeof(Rune));
	histories[0].n = nbuf;

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
				if(ptinrect(mc->m.xy, rects[0])){
					icursor = frcharofpt(&frames[0], mc->m.xy);
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
				if(nbuf == 0)
					break;
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
			redraw();
			break;
		}
	}

out:
	closedisplay(display);
	threadexitsall(nil);
}
