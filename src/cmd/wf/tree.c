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
	memset(n, 0, sizeof *n);
	n->op = op;
	n->left = left;
	n->right = right;
	return n;
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
	print("<script src=\"%T\"></script>\n", h->s);
}

static void
genstyle(Head *h, int cont)
{
	print("<link rel=\"stylesheet\" type=\"text/css\" href=\"%T\">\n", h->s);
}

static void
genfeed(Head *h, int cont)
{
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
[OFOOTER]	{ "footer", 1 },
[OHEADER]	{ "header", 1 },
[OLI]		{ "li", 0 },
[ONAV]	{ "nav", 1 },
[OOL]	{ "ol", 0 },
[OP]		{ "p", 0 },
[OQUOTE]		{ "blockquote", 0 },
[OSECTION]	{ "section", 1 },
[OSTRONG]	{ "strong", 0 },
[OTABLE]	{ "table", 0 },
[OTD]	{ "td", 0 },
[OTH]	{ "th", 0 },
[OTR]	{ "tr", 0 },
[OUL]	{ "ul", 0 },
};

static void
cgen(Node *n, int i, int h, int cflag)
{
	if(n == nil)
		return;

	cgen(n->head, i, h, cflag);
	cgen(n->nav, i, h, cflag);

	switch(n->op){
	case OARTICLE:
	case OASIDE:
	case ODIV:
	case ODL:
	case ONAV:
	case OOL:
	case OTABLE:
	case OUL:
		print("%I<%s%A>\n", i, tag[n->op].s, n->attr);
		cgen(n->left, i+tag[n->op].i, h, cflag);
		cgen(n->right, i+tag[n->op].i, h, cflag);
		print("%I</%s>\n", i, tag[n->op].s);
		break;
	case OSECTION:
		if(cflag&CSECTION)
			print("%I<%s%A>\n", i, tag[n->op].s, n->attr);
		cgen(n->left, i+tag[n->op].i, h, cflag);
		cgen(n->right, i+tag[n->op].i, h, cflag);
		if(cflag&CSECTION)
			print("%I</%s>\n", i, tag[n->op].s);
		break;
	case OSECTIND:
		cgen(n->left, i+tag[n->op].i, h+1, cflag);
		cgen(n->right, i+tag[n->op].i, h+1, cflag);
		break;
	case OFOOTER:
		if((cflag&CFOOT) == 0)
			fprint(2, "warning: footer element in prohibited block\n");
		print("%I<%s%A>\n", i, tag[n->op].s, n->attr);
		cgen(n->left, i+tag[n->op].i, h, cflag&~(CSECTION|CH|CHEAD|CFOOT));
		cgen(n->right, i+tag[n->op].i, h, cflag&~(CSECTION|CH|CHEAD|CFOOT));
		print("%I</%s>\n", i, tag[n->op].s);
		break;
	case OHEADER:
		if((cflag&CHEAD) == 0)
			fprint(2, "warning: header element in prohibited block\n");
		print("%I<%s%A>\n", i, tag[n->op].s, n->attr);
		cgen(n->left, i+tag[n->op].i, h, cflag&~(CSECTION|CHEAD|CFOOT));
		cgen(n->right, i+tag[n->op].i, h, cflag&~(CSECTION|CHEAD|CFOOT));
		print("%I</%s>\n", i, tag[n->op].s);
		break;
	case OCODE:
		print("%I<%s><code%A>", i, tag[n->op].s, n->attr);
		cgen(n->left, i+tag[n->op].i, h, cflag);
		cgen(n->right, i+tag[n->op].i, h, cflag);
		print("</code></%s>\n", tag[n->op].s);
		break;
	case ODD:
	case ODT:
	case OLI:
	case OP:
	case OQUOTE:
	case OTR:
		print("%I<%s%A>", i, tag[n->op].s, n->attr);
		cgen(n->left, i+tag[n->op].i, h, cflag);
		cgen(n->right, i+tag[n->op].i, h, cflag);
		print("</%s>\n", tag[n->op].s);
		break;
	case OH:
		if((cflag&CH) == 0)
			fprint(2, "warning: heading element in prohibited block\n");
		print("%I<h%d%A>", i, h, n->attr);
		cgen(n->left, i, h, cflag);
		print("</h%d>\n", h);
		break;
	case OLIST:
		if(n->attr)
			print("%I<div%A>\n", i, n->attr);
		cgen(n->left, i, h, cflag);
		cgen(n->right, i, h, cflag);
		if(n->attr)
			print("%I</div>\n", i);
		break;
	case OSTRONG:
	case OTD:
	case OTH:
		print("<%s>", tag[n->op].s);
		cgen(n->left, i+tag[n->op].i, h, cflag);
		print("</%s>", tag[n->op].s);
		break;
	case OLINK:
		print("<a href=\"%T\">", n->s);
		cgen(n->left, i, h, cflag);
		print("</a>");
		break;
	case OIMG:
		print("<img src=\"%T\"", n->s);
		if(n->left){
			print(" alt=\"");
			cgen(n->left, i, h, cflag);
			print("\"");
		}
		print(">");
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
	cgen(n->aside, i, h, cflag);
	cgen(n->foot, i, h, cflag);
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
	cgen(n, 0, 1, CSECTION|CH|CHEAD|CFOOT);
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

static Node *
elemental(Node *n)
{
	Attr *a;

	a = pickattr(n->attr, ACLASS, "nav");
	if(a){
		n->attr = removeattr(n->attr, a);
		return cast(ONAV, n);
	}

	a = pickattr(n->attr, ACLASS, "footer");
	if(a){
		n->attr = removeattr(n->attr, a);
		return cast(OFOOTER, n);
	}

	a = pickattr(n->attr, ACLASS, "header");
	if(a){
		n->attr = removeattr(n->attr, a);
		return cast(OHEADER, n);
	}

	a = pickattr(n->attr, ACLASS, "aside");
	if(a){
		n->attr = removeattr(n->attr, a);
		return cast(OASIDE, n);
	}

	return n;
}

Node *
complex(Node *n, Node *nn)
{
	Node *p;

	if(n == nil)
		return nil;

	if(n->op == OTD)
	if(n->left && n->left->op == OSTRONG){
		n->op = OTH;
		n->left = n->left->left;
	}

	if(n->op == OSECTIND || n->op == OARTICLE)
		nn = n;
	n->left = complex(n->left, nn);
	n->right = complex(n->right, nn);
	p = elemental(n);
	switch(p->op){
	case ONAV:
		nn->nav = new(OLIST, nn->nav, p);
		return nil;
	case OHEADER:
		nn->head = new(OLIST, nn->head, p);
		return nil;
	case OFOOTER:
		nn->foot = new(OLIST, nn->foot, p);
		return nil;
	case OASIDE:
		nn->aside = new(OLIST, nn->aside, p);
		return nil;
	default:
		return p;
	}
}
