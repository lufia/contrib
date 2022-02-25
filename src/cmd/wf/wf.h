#include <u.h>
#include <libc.h>
#include <regexp.h>

typedef struct Node Node;
typedef struct Attr Attr;
typedef struct Head Head;
typedef struct Sym Sym;
typedef struct Ext Ext;

#pragma	varargck	type	"T"	char*
#pragma	varargck	type	"A"	Attr*
#pragma	varargck	type	"I"	int

enum {		/* lexical analyzer state */
	Head0,
	Head1,
	Head2,

	Body0,
	Body1,
	Body2,
	Body3,

	Code,
	ID,
	Class,
	Table,
	Text,
};

enum {
	AID,
	ACLASS,
};

enum {
	CSECTION = 1<<0,
	CH = 1<<1,
	CHEAD = 1<<2,
	CFOOT = 1<<3,
};

enum {
	OARTICLE,
	OASIDE,
	OBREAK,
	OCODE,
	ODD,
	ODIV,
	ODL,
	ODT,
	OEM,
	OFIGCAPTION,
	OFIGIMG,
	OFIGURE,
	OFOOTER,
	OH,
	OHEADER,
	OIMG,
	OIND,
	OLI,
	OLINK,
	OLIST,
	OMAIN,
	ONAV,
	OOL,
	OP,
	OQUOTE,
	OSECTIND,
	OSECTION,
	OSTRONG,
	OTABLE,
	OTD,
	OTEXT,
	OTH,
	OTR,
	OUL,
};

struct Attr {
	int type;
	char *s;
	Attr *next;
};

struct Node {
	int op;
	Node *left;
	Node *right;
	Attr *attr;
	char *s;

	Node *aside;
	Node *foot;
	Node *head;
	Node *nav;
	int h;
};

struct Head {
	char *s;
	Head *next;

	Reprog *prog;
	char *t;
	int ign;
};

struct Sym {
	int type;
	char *name;
	Sym *next;
	Head *bp;
	Head *ep;
};

struct Ext {
	char *before;
	char *after;
};

/*
 * parse.y
 */
extern int yyparse(void);

/*
 * lex.c
 */
extern char *estrdup(char *s);
extern Sym *lookup(char *name);
extern void lexinit(int fd, char *file);
extern int yylex(void);
extern void yyerror(char *fmt, ...);

extern int lineno;
extern Sym *symlist;
extern int nerrors;

/*
 * tree.c
 */
extern void define(Sym *sym, char *s);
extern Node *new(int op, Node *left, Node *right);
extern Attr *attr(int type);
extern int htmlfmt(Fmt *fmt);
extern int attrfmt(Fmt *fmt);
extern int indentfmt(Fmt *fmt);
extern void dumptree(Node *n, int i);
extern void gen(Node *n);
extern char *realname(char *url, char *ix, char *ex);
extern char *expandurl(char *s);
extern Node *complex(Node *n, int h);
extern void simplify(Node *n);
extern Node *reorder(Node *n, Node *nn);

/*
 * wf.c
 */
extern Node *root;
extern int partial;
extern int debug;
extern char *lang;
extern Ext xdb[];
extern int nx;
