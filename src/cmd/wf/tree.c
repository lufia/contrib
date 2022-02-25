#include "wf.h"
#include "y.tab.h"

static char *
cstrchr(char *s, char c)
{
	for(; *s; s++){
		if(*s == '\\' && s[1]){
			s++;
			continue;
		}
		if(*s == c)
			return s;
	}
	return nil;
}

static int
compile(Head *h, char *exp)
{
	char c, *p, *r, *t, buf[1024];

	if(strlen(exp) >= sizeof buf){
		werrstr("regexp too long");
		return 0;
	}

	strcpy(buf, exp);
	p = buf + strspn(buf, " \t");
	switch(c = *p++){
	case 's':
		break;
	default:
		werrstr("unrecognized command '%c'", c);
		return 0;
	}
	c = *p++;
	r = p;
	p = cstrchr(p, c);
	if(p == nil){
	garb:
		werrstr("command garbled %q", exp);
		return 0;
	}
	*p++ = '\0';
	t = p;
	p = cstrchr(p, c);
	if(p == nil)
		goto garb;
	*p++ = '\0';
	p += strspn(p, " \t");

	h->prog = regcomp(r);
	if(h->prog == nil)
		sysfatal("regcomp: %r");
	h->t = estrdup(t);
	h->ign = *p == 'i';
	return 1;
}

void
define(Sym *sym, char *t)
{
	Head *h;

	h = malloc(sizeof(*h));
	if(h == nil)
		sysfatal("malloc: %r");
	memset(h, 0, sizeof *h);

	if(sym->type == Turl)
		if(!compile(h, t)){
			yyerror("compile: %r");
			free(t);
			free(h);
			return;
		}
	h->s = t;
	if(sym->ep == nil)
		sym->bp = sym->ep = h;
	else{
		sym->ep->next = h;
		sym->ep = h;
	}
}

Node *
new(int op, Node *left, Node *right)
{
	Node *n;

	n = malloc(sizeof *n);
	if(n == nil)
		sysfatal("malloc: %r");
	if(left == nil){
		left = right;
		right = nil;
	}
	memset(n, 0, sizeof *n);
	n->op = op;
	n->left = left;
	n->right = right;
	return n;
}

static Node *
list(Node *left, Node *right)
{
	if(left == nil)
		return right;
	return new(OLIST, left, right);
}

Attr *
attr(int type)
{
	Attr *a;

	a = malloc(sizeof *a);
	if(a == nil)
		sysfatal("malloc: %r");
	memset(a, 0, sizeof *a);
	a->type = type;
	return a;
}

static char *tab[] = {
	['<']		"&lt;",
	['>']		"&gt;",
	['&']		"&amp;",
	['"']		"&quot;",
};

static void
specialchars(Fmt *fmt, char *s)
{
	int c;
	char *p;

	for(; p = strpbrk(s, "<>&\""); s = p+1){
		c = *p;
		*p = '\0';
		fmtprint(fmt, "%s%s", s, tab[c]);
	}
	fmtprint(fmt, "%s", s);
}

int
htmlfmt(Fmt *fmt)
{
	char *s;

	s = va_arg(fmt->args, char*);
	specialchars(fmt, s);
	return 0;
}

int
attrfmt(Fmt *fmt)
{
	Attr *a, *p;
	int found;

	a = va_arg(fmt->args, Attr*);
	found = 0;
	for(p = a; p; p = p->next){
		if(p->type != AID)
			continue;
		found++;
		if(found > 1)
			fprint(2, "warning: double identifier\n");
		else
			fmtprint(fmt, " id=\"%T\"", a->s);
	}

	found = 0;
	for(p = a; p; p = p->next){
		if(p->type != ACLASS)
			continue;
		fmtprint(fmt, found++ ? " " : " class=\"");
		fmtprint(fmt, "%T", p->s);
	}
	if(found)
		fmtprint(fmt, "\"");
	return 0;
}

int
indentfmt(Fmt *fmt)
{
	int i, n;

	n = va_arg(fmt->args, int);
	for(i = 0; i < n; i++)
		fmtprint(fmt, "\t");
	return 0;
}

static void
each(Sym *s, void (*f)(Head*, int))
{
	Head *h;

	for(h = s->bp; h; h = h->next)
		f(h, h->next != nil);
}

static void
gentitle(Head *h, int cont)
{
	print("%T", h->s);
	if(cont)
		print(": ");
}

static void
genmeta(Head *h, int cont)
{
	print("%T", h->s);
	if(cont)
		print(",");
}

static void
genscript(Head *h, int cont)
{
	USED(cont);
	print("<script src=\"%T\"></script>\n", h->s);
}

static void
genstyle(Head *h, int cont)
{
	USED(cont);
	print("<link rel=\"stylesheet\" type=\"text/css\" href=\"%T\">\n", h->s);
}

static void
genfeed(Head *h, int cont)
{
	USED(cont);
	print("<link rel=\"alternate\" type=\"application/atom+xml\""
		" title=\"Atom\" href=\"%T\">\n", h->s);
}

static void
dgen(void)
{
	Sym *s;

	if(partial)
		return;
	print("<head>\n");
	print("<meta charset=\"utf-8\">\n");

	s = lookup("title");
	if(s->bp){
		print("<title>");
		each(s, gentitle);
		print("</title>\n");
	}

	s = lookup("tag");
	if(s->bp){
		print("<meta name=\"keywords\" content=\"");
		each(s, genmeta);
		print("\">\n");
	}

	s = lookup("script");
	each(s, genscript);
	s = lookup("style");
	each(s, genstyle);
	s = lookup("feed");
	each(s, genfeed);

	for(s = symlist; s; s = s->next){
		if(s->type != Tmeta)
			continue;
		print("<meta name=\"%s\" content=\"", s->name);
		each(s, genmeta);
		print("\">\n");
	}

	print("</head>\n");
}

static struct {
	char *s;
	int i;
} tag[] = {
[OARTICLE]	{ "article", 1 },
[OASIDE]	{ "aside", 1 },
[OCODE]	{ "pre", 0 },
[ODD]	{ "dd", 0 },
[ODIV]	{ "div", 1 },
[ODL]	{ "dl", 0 },
[ODT]	{ "dt", 0 },
[OEM]	{ "em", 0 },
[OFIGCAPTION]	{ "figcaption", 0 },
[OFIGIMG]	{ "img", 0 },
[OFIGURE]	{ "figure", 1 },
[OFOOTER]	{ "footer", 1 },
[OHEADER]	{ "header", 1 },
[OLI]		{ "li", 0 },
[OLINK]	{ "a", 0 },
[OMAIN]	{ "main", 1 },
[ONAV]	{ "nav", 1 },
[OOL]	{ "ol", 0 },
[OP]		{ "p", 0 },
[OQUOTE]	{ "blockquote", 0 },
[OSECTION]	{ "section", 1 },
[OSTRONG]	{ "strong", 0 },
[OTABLE]	{ "table", 0 },
[OTD]	{ "td", 0 },
[OTH]	{ "th", 0 },
[OTR]	{ "tr", 0 },
[OUL]	{ "ul", 0 },

/* dump */
[OLIST]	{ ":list", 0 },
[OH]	{ "h", 0 },
[OSECTIND]	{ ":ind", 0 },
[OTEXT]	{ ":text", 0 },
};

static void
dumpattr(Node *n, char *name, int i)
{
	if(n == nil)
		return;
	print("%I%s:\n", i, name);
	dumptree(n, i+1);
}

void
dumptree(Node *n, int i)
{
	char *s, buf[10];

	if(n == nil){
		print("%I<nil>\n", i);
		return;
	}
	s = tag[n->op].s;
	if(s == nil){
		snprint(buf, sizeof buf, "[tag=%d]", n->op);
		s = buf;
	}
	print("%I<%s%A>\n", i, s, n->attr);
	print("%ILeft:\n", i);
	dumptree(n->left, i+1);
	print("%IRight:\n", i);
	dumptree(n->right, i+1);
	dumpattr(n->aside, "Aside", i);
	dumpattr(n->foot, "Foot", i);
	dumpattr(n->head, "Head", i);
	dumpattr(n->nav, "Nav", i);
}

static void
cgen(Node *n, int i, int cflag)
{
	if(n == nil)
		return;

	cgen(n->head, i, cflag);
	cgen(n->nav, i, cflag);

	switch(n->op){

	/*
	 * sectioning content
	 */
	case OARTICLE:
	case OASIDE:
	case OMAIN:
	case ONAV:
		print("%I<%s%A>\n", i, tag[n->op].s, n->attr);
		cgen(n->left, i+tag[n->op].i, cflag);
		cgen(n->right, i+tag[n->op].i, cflag);
		print("%I</%s>\n", i, tag[n->op].s);
		break;
	case OSECTION:
		if(cflag&CSECTION)
			print("%I<%s%A>\n", i, tag[n->op].s, n->attr);
		cgen(n->left, i+tag[n->op].i, cflag);
		cgen(n->right, i+tag[n->op].i, cflag);
		if(cflag&CSECTION)
			print("%I</%s>\n", i, tag[n->op].s);
		break;
	case OSECTIND:
		cgen(n->left, i+tag[n->op].i, cflag);
		cgen(n->right, i+tag[n->op].i, cflag);
		break;

	/*
	 * flow content (block)
	 */
	case OFOOTER:
		if((cflag&CFOOT) == 0)
			fprint(2, "warning: footer element in prohibited block\n");
		print("%I<%s%A>\n", i, tag[n->op].s, n->attr);
		cgen(n->left, i+tag[n->op].i, cflag&~(CSECTION|CH|CHEAD|CFOOT));
		cgen(n->right, i+tag[n->op].i, cflag&~(CSECTION|CH|CHEAD|CFOOT));
		print("%I</%s>\n", i, tag[n->op].s);
		break;
	case OHEADER:
		if((cflag&CHEAD) == 0)
			fprint(2, "warning: header element in prohibited block\n");
		print("%I<%s%A>\n", i, tag[n->op].s, n->attr);
		cgen(n->left, i+tag[n->op].i, cflag&~(CSECTION|CHEAD|CFOOT));
		cgen(n->right, i+tag[n->op].i, cflag&~(CSECTION|CHEAD|CFOOT));
		print("%I</%s>\n", i, tag[n->op].s);
		break;
	case ODIV:
	case ODL:
	case OFIGURE:
	case OOL:
	case OTABLE:
	case OUL:
		print("%I<%s%A>\n", i, tag[n->op].s, n->attr);
		cgen(n->left, i+tag[n->op].i, cflag);
		cgen(n->right, i+tag[n->op].i, cflag);
		print("%I</%s>\n", i, tag[n->op].s);
		break;

	case ODD:
	case ODT:
	case OFIGCAPTION:
	case OLI:
	case OP:
	case OQUOTE:
	case OTR:
		print("%I<%s%A>", i, tag[n->op].s, n->attr);
		cgen(n->left, i+tag[n->op].i, cflag);
		cgen(n->right, i+tag[n->op].i, cflag);
		print("</%s>\n", tag[n->op].s);
		break;
	case OTD:
	case OTH:
		print("<%s>", tag[n->op].s);
		cgen(n->left, i+tag[n->op].i, cflag);
		print("</%s>", tag[n->op].s);
		break;
	case OCODE:
		print("%I<%s><code%A>", i, tag[n->op].s, n->attr);
		cgen(n->left, i+tag[n->op].i, cflag);
		cgen(n->right, i+tag[n->op].i, cflag);
		print("</code></%s>\n", tag[n->op].s);
		break;
	case OH:
		if((cflag&CH) == 0)
			fprint(2, "warning: heading element in prohibited block\n");
		print("%I<h%d%A>", i, n->h, n->attr);
		cgen(n->left, i, cflag);
		print("</h%d>\n", n->h);
		break;

	/*
	 * flow content (inline)
	 */
	case OSTRONG:
		print("<%s>", tag[n->op].s);
		cgen(n->left, i+tag[n->op].i, cflag);
		print("</%s>", tag[n->op].s);
		break;
	case OEM:
		print("<%s>", tag[n->op].s);
		cgen(n->left, i+tag[n->op].i, cflag);
		print("</%s>", tag[n->op].s);
		break;
	case OLINK:
		print("<%s href=\"%T\">", tag[n->op].s, n->s);
		cgen(n->left, i, cflag);
		print("</%s>", tag[n->op].s);
		break;
	case OIMG:
		print("<img src=\"%T\"", n->s);
		if(n->left){
			print(" alt=\"");
			cgen(n->left, i, cflag);
			print("\"");
		}
		print(">");
		break;
	case OFIGIMG:
		print("%I<img src=\"%T\"", i, n->s);
		if(n->left){
			print(" alt=\"");
			cgen(n->left, i, cflag);
			print("\"");
		}
		print(">\n");
		break;

	case OLIST:
		if(n->attr)
			print("%I<div%A>\n", i, n->attr);
		cgen(n->left, i, cflag);
		cgen(n->right, i, cflag);
		if(n->attr)
			print("%I</div>\n", i);
		break;
	case OTEXT:
		print("%T", n->s);
		break;
	case OBREAK:
		print("\n");
		break;
	case OIND:
		print("%I", i);
		break;
	default:
		assert(0);
		break;
	}
	cgen(n->aside, i, cflag);
	cgen(n->foot, i, cflag);
}

void
gen(Node *n)
{
	if(!partial){
		print("<!doctype html>\n");
		print("<html");
		if(lang)
			print(" lang=\"%s\"", lang);
		print(">\n");
		dgen();
		print("<body>\n");
	}
	cgen(n, 0, CSECTION|CH|CHEAD|CFOOT);
	if(!partial){
		print("</body>\n");
		print("</html>\n");
	}
}

char *
realname(char *url, char *ix, char *ex)
{
	char *p, *t;
	int n;

	p = strrchr(url, '.');
	if(p == nil || strcmp(++p, ix) != 0)
		return url;

	n = p - url;
	t = malloc(n+strlen(ex)+1);
	if(t == nil)
		sysfatal("malloc: %r");
	memmove(t, url, n);
	strcpy(t+n, ex);
	return t;
}

char *
expandurl(char *t)
{
	Sym *s;
	Head *h;
	char buf[1024];
	Resub m[10];

	s = lookup("url");
	for(h = s->bp; h; h = h->next){
		memset(m, 0, sizeof m);
		if(regexec(h->prog, t, m, nelem(m))){
			regsub(h->t, buf, sizeof buf, m, nelem(m));
			if(debug)
				fprint(2, "url %q to %q\n", t, buf);
			return estrdup(buf);
		}
	}
	return t;
}

static Attr *
pickattr(Attr *a, int type, char *name)
{
	for(; a; a = a->next)
		if(a->type == type && strcmp(a->s, name) == 0)
			return a;
	return nil;
}

static Attr *
removeattr(Attr *p, Attr *a)
{
	Attr *prev, tmp;

	tmp.next = p;
	for(prev = &tmp; p; p = p->next)
		if(p == a)
			prev->next = p->next;
	return tmp.next;
}

static Node *
cast(int op, Node *n)
{
	if(n->op == ODIV)
		n->op = op;
	else
		n = new(op, n, nil);
	return n;
}

static struct {
	char *name;
	int type;
} ctab[] = {
	{ "article", OARTICLE },
	{ "aside", OASIDE },
	{ "footer", OFOOTER },
	{ "header", OHEADER },
	{ "nav", ONAV },
};

static Node *
elemental(Node *n)
{
	int i;
	Attr *a;

	for(i = 0; i < nelem(ctab); i++){
		a = pickattr(n->attr, ACLASS, ctab[i].name);
		if(a){
			n->attr = removeattr(n->attr, a);
			return cast(ctab[i].type, n);
		}
	}
	return n;
}

static int
issect(int op)
{
	int i;

	for(i = 0; i < nelem(ctab); i++)
		if(ctab[i].type == op)
			return 1;
	return 0;
}

static int
transdict(Node *n, Node **pp)
{
	/*
	 * Initial n's layout example:
	 *
	 * DL
	 * - LIST
	 *   - LIST
	 *     - LIST
	 *       - DT
	 *       - DD
	 *     - DD
	 *   - DT
	 * - <nil>
	 */
	int d;
	Node *p;

	if(n == nil)
		return -1;
	if(n->op == ODD)
		return 0;

	d = transdict(n->left, pp);
	if(d >= 0){
		if(n->right && n->right->op == ODT){
			p = new(ODIV, n->left, nil);
			*pp = new(OLIST, *pp, p);
			n->left = nil;
			return -1;
		}
		return d + 1;
	}
	d = transdict(n->right, pp);
	if(d < 0)
		return -1;
	return d + 1;
}

Node *
complex(Node *n, int h)
{
	Node *p;

	if(n == nil)
		return nil;

	if(n->op == OTD)
	if(n->left && n->left->op == OSTRONG){
		n->op = OTH;
		n->left = n->left->left;
	}

	/* Groups continual DTs and DDs with a DIV. */
	if(n->op == ODL){
		p = nil;
		transdict(n->left, &p);
		if(n->left->left || n->left->right)
			p = new(OLIST, p, new(ODIV, n->left, nil));
		n->left = p;
	}

	if(n->op == OP)
	if(n->left && n->left->op == OIMG)
	if(n->right == nil){
		n->op = OFIGURE;
		n->left->op = OFIGIMG;
		if(n->left->left)
			n->right = new(OFIGCAPTION, n->left->left, nil);
	}

	if(n->op == OSECTIND)
		h++;
	n->h = h;

	n->left = complex(n->left, h);
	n->right = complex(n->right, h);
	return elemental(n);
}

static int
iselem(Node *n, int op)
{
	if(n == nil)
		return 0;
	return n->op == op;
}

static int
issingle(Node *n, int op)
{
	if(iselem(n, OLIST))
	if(iselem(n->left, op))
	if(n->right == nil)
		return 1;
	return 0;
}

void
simplify(Node *n)
{
	Node *p;

	if(n == nil)
		return;
	if(n->op == OMAIN || issect(n->op))
	if(issingle(n->left, OSECTION))
	if(n->right == nil){
		p = n->left->left;
		n->left->left = p->left;
		n->left->right = p->right;
		free(p);
	}
	simplify(n->left);
	simplify(n->right);
	simplify(n->aside);
	simplify(n->foot);
	simplify(n->head);
	simplify(n->nav);
}

Node *
reorder(Node *n, Node *nn)
{
	if(n == nil)
		return nil;
	if(n->op == OSECTIND || n->op == OMAIN)
		nn = n;
	n->left = reorder(n->left, nn);
	n->right = reorder(n->right, nn);
	if(n->op == OLIST && n->left == nil && n->right == nil){
		free(n);
		return nil;
	}
	if(n->left == nil){
		n->left = n->right;
		n->right = nil;
	}
	switch(n->op){
	case OASIDE:
		nn->aside = list(nn->aside, n);
		return nil;
	case OFOOTER:
		nn->foot = list(nn->foot, n);
		return nil;
	case OHEADER:
		nn->head = list(nn->head, n);
		return nil;
	case ONAV:
		nn->nav = list(nn->nav, n);
		return nil;
	default:
		return n;
	}
}
