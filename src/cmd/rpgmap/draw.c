#include "rpgmap.h"
#include <event.h>

static Box map[100][100];
static Image *grey;
static Image *canvas;

static Box
box(Rectangle r)
{
	Box b;
	Rectangle t;
	Point p;

	t = Rect(r.min.x+Wallpt, r.min.y, r.max.x, r.min.y+Wallpt);
	b.top = (Wall){ t, nil };
	t = Rect(r.min.x, r.min.y+Wallpt, r.min.x+Wallpt, r.max.y);
	b.left = (Wall){ t, nil };
	p = Pt(Wallpt, Wallpt);
	b.r = Rpt(addpt(r.min, p), r.max);
	return b;
}

static void
initmap(void)
{
	Rectangle r;
	int i, j;

	freeimage(grey);
	grey = allocimage(display, Rect(0,0,1,1), RGB24, 1, 0xccccccff);
	freeimage(canvas);
	r = Rect(0, 0, nelem(map)*Boxpt, nelem(map[0])*Boxpt);
	canvas = allocimage(display, r, RGB24, 0, DWhite);

	for(i = 0; i < nelem(map); i++)
		for(j = 0; j < nelem(map[i]); j++){
			r = Rect(i*Boxpt, j*Boxpt, (i+1)*Boxpt, (j+1)*Boxpt);
			map[i][j] = box(r);
		}
}

static Box *
getbox(int x, int y)
{
	if(x < 0 || x >= nelem(map))
		return nil;
	if(y < 0 || y >= nelem(map[x]))
		return nil;
	return &map[x][y];
}

static void
execute(Prog *prog)
{
	Prog *p;
	Cmd *c;
	Box *b;

	for(p = prog; p; p = p->next){
		c = p->cmd;
		switch(c->op){
		case ONOP:
		case OFLOOR:
		case OMARK:
			break;
		case OLINE:
			b = getbox(c->x, c->y);
			if(b == nil){
				fprint(2, "warning: line %d %d: over\n", c->x, c->y);
				break;
			}
			switch(c->place){
			case CTOP:
				if(b->top.prog)
					uninst(b->top.prog);
				b->top.prog = p;
				break;
			case CLEFT:
				if(b->left.prog)
					uninst(b->left.prog);
				b->left.prog = p;
				break;
			default:
				assert(0);
			}
		}
	}
}

static void
drawwall(Image *img, Wall *w)
{
	Image *color;

	if(w->prog)
		color = display->black;
	else
		color = grey;
	draw(img, w->r, color, nil, ZP);
}

static void
drawbox(Image *img, Box *b)
{
	drawwall(img, &b->top);
	drawwall(img, &b->left);
	draw(img, b->r, display->white, nil, ZP);
}

static void
drawmap(Image *img)
{
	int i, j;

	for(i = 0; i < nelem(map); i++)
		for(j = 0; j < nelem(map[i]); j++)
			drawbox(img, &map[i][j]);
}

static void
cline(Wall *w, int x, int y, int place)
{
	Cmd *c;

	if(w->prog){
		uninst(w->prog);
		w->prog = nil;
	}else{
		c = new(OLINE, x, y);
		c->place = place;
		w->prog = inst(c);
	}
}

static Rectangle
sugar(Rectangle r, int w, int h)
{
	r.min.x -= w;
	r.max.x += w;
	r.min.y -= h;
	r.max.y += h;
	return r;
}

static void
click(Point p)
{
	Point t;
	Box *b;

	t = divpt(p, Boxpt);
	b = getbox(t.x, t.y);
	if(b == nil)
		return;
	if(ptinrect(p, sugar(b->top.r, 0, 5))){
		cline(&b->top, t.x, t.y, CTOP);
		drawwall(canvas, &b->top);
	}else if(ptinrect(p, sugar(b->left.r, 5, 0))){
		cline(&b->left, t.x, t.y, CLEFT);
		drawwall(canvas, &b->left);
	}
}

Point base;

void
eresized(int new)
{
	if(new && getwindow(display, Refnone) < 0)
		sysfatal("getwindow: %r");
	draw(canvas, canvas->r, display->white, nil, ZP);
	drawmap(canvas);
	base = ZP;
	draw(screen, screen->r, canvas, nil, base);
	flushimage(display, 1);
}

char *buttons[] = {
	"exit",
	"move above",
	"move below",
	"move left",
	"move right",
	nil,
};
Menu menu = {
	buttons
};

static void
apply(Prog *p, int x, int y)
{
	Cmd *c;

	for(; p; p = p->next){
		c = p->cmd;
		if(c->x >= 0)
			c->x += x;
		if(c->y >= 0)
			c->y += y;
	}
}

static void
redraw(void)
{
	initmap();
	execute(top);
	eresized(0);
}

void
edit(void)
{
	Event e;
	Mouse m;
	Point o, d;

	if(initdraw(nil, nil, "mapedit") < 0)
		sysfatal("initdraw: %r");
	einit(Ekeyboard|Emouse);
	redraw();
	for(;;)
		switch(event(&e)){
		case Ekeyboard:
			if(e.kbdc == 'q')
				return;
			break;
		case Emouse:
			m = e.mouse;
			switch(m.buttons){
			case 1:
				o = m.xy;
				do{
					d = subpt(m.xy, o);
					o = m.xy;
					base = subpt(base, d);
					if(base.x < 0)
						base.x = 0;
					if(base.y < 0)
						base.y = 0;
					d = subpt(screen->r.max, screen->r.min);
					d = addpt(base, d);
					if(d.x > canvas->r.max.x)
						base.x = canvas->r.max.x-screen->r.max.x;
					if(d.y > canvas->r.max.y)
						base.y = canvas->r.max.y-screen->r.max.y;
					draw(screen, screen->r, canvas, nil, base);
					m = emouse();
				}while(m.buttons == 1);
				break;
			case 2:
				do{
					m = emouse();
				}while(m.buttons == 2);
				d = subpt(m.xy, screen->r.min);
				click(addpt(d, base));
				draw(screen, screen->r, canvas, nil, base);
				break;
			case 4:
				switch(emenuhit(3, &m, &menu)){
				case 0:		/* exit */
					return;
				case 1:		/* above */
					apply(top, 0, -1);
					redraw();
					break;
				case 2:		/* below */
					apply(top, 0, 1);
					redraw();
					break;
				case 3:		/* left */
					apply(top, -1, 0);
					redraw();
					break;
				case 4:		/* right */
					apply(top, 1, 0);
					redraw();
					break;
				}
			}
		}
}
