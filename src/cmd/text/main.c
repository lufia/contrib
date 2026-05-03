#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>
#include <keyboard.h>
#include <mouse.h>
#include "lib.h"

Rune buf[Maxbuf];
int nbuf;
int icursor;
Keyboardctl *kc;
Mousectl *mc;

char **cmdargv;

char *menu[] = {
	"exit",
	nil
};

Menu m = {
	menu
};

void
usage(void)
{
	fprint(2, "usage: %s [-f font] [-l label] [cmd [args...]]\n", argv0);
	threadexitsall("usage");
}

Point
getpt(int index)
{
	Point p, ep;
	int i;
	Rune rs[2];

	p = screen->r.min;
	rs[1] = 0;

	for(i = 0; i < index; i++){
		if(buf[i] == '\n'){
			p.x = screen->r.min.x;
			p.y += font->height;
			continue;
		}
		rs[0] = buf[i];
		ep = runestringsize(font, rs);
		if(p.x + ep.x > screen->r.max.x){
			p.x = screen->r.min.x;
			p.y += font->height;
		}
		p.x += ep.x;
	}
	return p;
}

int
pt2index(Point pt)
{
	Point p, ep;
	int i, besti;
	Rune rs[2];

	p = screen->r.min;
	rs[1] = 0;
	besti = 0;

	for(i = 0; i <= nbuf; i++){
		if(pt.y >= p.y && pt.y < p.y + font->height){
			if(pt.x < p.x + 2)
				return i;
			besti = i;
		} else if(pt.y >= p.y + font->height){
			besti = i;
		}

		if(i == nbuf) break;

		if(buf[i] == '\n'){
			if(pt.y >= p.y && pt.y < p.y + font->height)
				return i;
			p.x = screen->r.min.x;
			p.y += font->height;
			continue;
		}
		rs[0] = buf[i];
		ep = runestringsize(font, rs);
		if(p.x + ep.x > screen->r.max.x){
			p.x = screen->r.min.x;
			p.y += font->height;
		}
		p.x += ep.x;
	}
	return besti;
}

void
redraw(void)
{
	Point p, ep, curp;
	int i;
	Rune rs[2];

	if(screen == nil)
		return;

	draw(screen, screen->r, display->white, nil, ZP);
	p = screen->r.min;
	curp = p;
	rs[1] = 0;

	for(i = 0; i < nbuf; i++){
		if(i == icursor)
			curp = p;

		if(buf[i] == '\n'){
			p.x = screen->r.min.x;
			p.y += font->height;
			continue;
		}
		rs[0] = buf[i];
		ep = runestringsize(font, rs);
		if(p.x + ep.x > screen->r.max.x){
			p.x = screen->r.min.x;
			p.y += font->height;
		}
		runestring(screen, p, display->black, ZP, font, rs);
		p.x += ep.x;
	}
	if(i == icursor)
		curp = p;

	draw(screen, Rect(curp.x, curp.y, curp.x+2, curp.y+font->height), display->black, nil, ZP);
	flushimage(display, 1);
}

void
output(void)
{
	int p[2];
	int pid, wid;

	if(nbuf == 0)
		return;

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
				icursor = pt2index(mc->m.xy);
				redraw();
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
			redraw();
			break;
		}
	}

out:
	closedisplay(display);
	threadexitsall(nil);
}
