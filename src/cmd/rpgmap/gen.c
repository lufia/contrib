#include "rpgmap.h"

char *subject;
Prog *top;
static Prog *pc;

Prog *
inst(Cmd *c)
{
	Prog *p;

	p = emalloc(sizeof *p);
	p->cmd = c;
	if(pc == nil){
		top = emalloc(sizeof *top);
		top->cmd = new(ONOP, -1, -1);
		pc = top;
	}
	pc->next = p;
	p->prev = pc;
	pc = p;
	return p;
}

void
uninst(Prog *p)
{
	Prog *prev;

	prev = p->prev;
	assert(prev != nil);
	prev->next = p->next;
	if(p->next)
		p->next->prev = prev;
	else
		pc = prev;

	free(p->cmd->s);
	free(p->cmd);
	free(p);
}

Cmd *
new(int op, int x, int y)
{
	Cmd *c;

	c = emalloc(sizeof *c);
	c->op = op;
	c->x = x;
	c->y = y;
	return c;
}

static void
genfloor(int fd, Prog *p)
{
	Cmd *c;
	int x, y;

	x = y = 0;
	for(; p; p = p->next){
		c = p->cmd;
		if(c->x >= 0 && c->x > x)
			x = c->x;
		if(c->y >= 0 && c->y > y)
			y = c->y;
		if(c->op == OFLOOR && c->s && subject == nil)
			subject = c->s;
	}
	if(subject == nil)
		subject = "autogen";
	fprint(fd, "floor %d %d \"%s\"\n", x+1, y+1, subject);
}

static char *ctab[] = {
[CLEFT]	"left",
[CTOP]	"top",
};

void
dump(int fd)
{
	Prog *p;
	Cmd *c;

	genfloor(fd, top);
	for(p = top; p; p = p->next){
		c = p->cmd;
		switch(c->op){
		case ONOP:
		case OFLOOR:
			break;
		case OLINE:
			fprint(fd, "line %d %d %s\n", c->x+1, c->y+1, ctab[c->place]);
			break;
		case OMARK:
			fprint(fd, "mark %d %d \"%s\"\n", c->x+1, c->y+1, c->s);
			break;
		default:
			assert(0);
		}
	}
}
