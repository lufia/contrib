#include <u.h>
#include <libc.h>
#include <draw.h>

typedef struct Cmd Cmd;
typedef struct Prog Prog;
typedef struct Wall Wall;
typedef struct Box Box;

enum {
	Boxpt = 20,
	Wallpt = 3,
};

enum {
	ONOP,
	OFLOOR,
	OLINE,
	OMARK,
};

enum {
	CTOP,
	CBOT,
	CLEFT,
	CRIGHT,
};

struct Cmd {
	int op;
	int x;
	int y;

	int place;
	char *s;
	int color;
};

struct Prog {
	Cmd *cmd;
	Prog *prev;
	Prog *next;
};

struct Wall {
	Rectangle r;
	Prog *prog;
};

struct Box {
	Wall top;
	Wall left;
	Rectangle r;	/* without top and left */
};

extern void *emalloc(ulong n);
extern void lexinit(int fd, char *file);
extern int yylex(void);
extern int yyparse(void);
extern void yyerror(char *fmt, ...);

extern Prog *inst(Cmd *c);
extern void uninst(Prog *p);
extern Cmd *new(int op, int x, int y);
extern void dump(int fd);

extern void edit(void);

extern char *subject;
extern Prog *top;
extern int nerrors;
