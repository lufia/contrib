%{
#include "wf.h"
%}
%union {
	Node *node;
	Attr *attr;
	Sym *sym;
	char *s;
}
%token		Tbegin Tend Terror Tbreak
%token	<sym>	Ttitle Ttag Tscript Tstyle Tfeed Tmeta Turl
%token	<s>		Tstring
%type	<node>	body article
%type	<node>	sections section blocks block ublock
%type	<node>	h p ol ul dl dict item quote qitem code table cells
%type	<node>	inline nestable nestok token text link
%type	<attr>	attr uattr
%type	<sym>	symbol
%type	<s>		string
%%
doc:
	head body
	{
		root = $2;
	}

head:
|	head uhead

uhead:
	error ';'
|	symbol string ';'
	{
		define($1, $2);
	}

symbol:
	Ttitle
|	Ttag
|	Tscript
|	Tstyle
|	Tfeed
|	Turl
|	Tmeta

body:
	article
	{
		$$ = nil;
		if($1)
			$$ = new(OMAIN, $1, nil);
	}

article:
	blocks
|	blocks sections
	{
		$$ = new(OLIST, $1, $2);
	}

sections:
	section
|	sections section
	{
		$$ = new(OLIST, $1, $2);
	}

section:
	h blocks
	{
		$$ = new(OSECTION, $1, $2);
	}

blocks:
	{
		$$ = nil;
	}
|	blocks block
	{
		$$ = new(OLIST, $1, $2);
	}

block:
	ublock
|	attr ublock
	{
		$$ = $2;
		if($$)
			$$->attr = $1;
	}

ublock:
	error ';'
	{
		$$ = nil;
	}
|	p
	{
		$$ = new(OP, $1, nil);
	}
|	ol
	{
		$$ = new(OOL, $1, nil);
	}
|	ul
	{
		$$ = new(OUL, $1, nil);
	}
|	dl
	{
		$$ = new(ODL, $1, nil);
	}
|	quote
	{
		$$ = new(OQUOTE, $1, nil);
	}
|	code
	{
		$$ = new(OCODE, $1, nil);
	}
|	table
	{
		$$ = new(OTABLE, $1, nil);
	}
|	break
	{
		$$ = nil;
	}
|	'{' ';' Tbegin article Tend '}' ';'
	{
		$$ = $4;
		if($$)
			$$ = new(ODIV, $4, nil);
	}
|	Tbegin article Tend
	{
		$$ = new(OSECTIND, $2, nil);
	}

h:
	'=' token ';'
	{
		$$ = new(OH, $2, nil);
	}

p:
	'\\' inline ';'
	{
		$$ = $2;
	}
|	p '\\' inline ';'
	{
		Node *n;

		n = new(OLIST, new(OBREAK, nil, nil), new(OIND, nil, nil));
		$$ = new(OLIST, $1, new(OLIST, n, $3));
	}

ol:
	'+' item ';'
	{
		$$ = new(OLI, $2, nil);
	}
|	ol '+' item ';'
	{
		$$ = new(OLIST, $1, new(OLI, $3, nil));
	}

ul:
	'*' item ';'
	{
		$$ = new(OLI, $2, nil);
	}
|	ul '*' item ';'
	{
		$$ = new(OLIST, $1, new(OLI, $3, nil));
	}

dl:
	dict
|	dl dict
	{
		$$ = new(OLIST, $1, $2);
	}

dict:
	':' inline ';'
	{
		$$ = new(ODT, $2, nil);
	}
|	'-' item ';'
	{
		$$ = new(ODD, $2, nil);
	}

item:
	inline

quote:
	'>' qitem ';'
	{
		$$ = $2;
	}
|	quote '>' qitem ';'
	{
		Node *n;

		n = new(OLIST, new(OBREAK, nil, nil), new(OIND, nil, nil));
		$$ = new(OLIST, $1, new(OLIST, n, $3));
	}

qitem:
	inline

code:
	'!' text ';'
	{
		$$ = $2;
	}
|	code '!' text ';'
	{
		Node *n;

		n = new(OLIST, new(OBREAK, nil, nil), $3);
		$$ = new(OLIST, $1, n);
	}

table:
	'|' cells ';'
	{
		$$ = new(OTR, $2, nil);
	}
|	table '|' cells ';'
	{
		$$ = new(OLIST, $1, new(OTR, $3, nil));
	}

cells:
	inline
	{
		$$ = new(OTD, $1, nil);
	}
|	',' inline
	{
		$$ = new(OTD, $2, nil);
	}
|	cells ',' inline
	{
		$$ = new(OLIST, $1, new(OTD, $3, nil));
	}

break:
	Tbreak
|	break Tbreak

attr:
	uattr
|	attr uattr
	{
		$2->next = $1;
		$$ = $2;
	}

uattr:
	'#' string ';'
	{
		$$ = attr(AID);
		$$->s = $2;
	}
|	'.' string ';'
	{
		$$ = attr(ACLASS);
		$$->s = $2;
	}

inline:
	token
|	inline token
	{
		$$ = new(OLIST, $1, $2);
	}

token:
	'*' nestable '*'
	{
		$$ = new(OSTRONG, $2, nil);
	}
|	link
|	text

nestable:
	nestok
|	nestable nestok
	{
		$$ = new(OLIST, $1, $2);
	}

nestok:		/* nestable token */
	text
|	link

link:
	'[' inline '|' string ']'
	{
		int i;

		$$ = new(OLINK, $2, nil);
		for(i = 0; i < nx; i++)
			$4 =realname($4, xdb[i].before, xdb[i].after);
		$$->s = $4;
	}
|	'[' text ']'
	{
		int i;

		$$ = new(OLINK, $2, nil);
		for(i = 0; i < nx; i++)
			$2->s = realname($2->s, xdb[i].before, xdb[i].after);
		$$->s = expandurl($2->s);
	}
|	'<' inline '|' string '>'
	{
		int i;

		$$ = new(OIMG, $2, nil);
		for(i = 0; i < nx; i++)
			$4 = realname($4, xdb[i].before, xdb[i].after);
		$$->s = $4;
	}
|	'<' text '>'
	{
		int i;

		$$ = new(OIMG, nil, nil);
		for(i = 0; i < nx; i++)
			$2->s = realname($2->s, xdb[i].before, xdb[i].after);
		$$->s = expandurl($2->s);
	}

text:
	string
	{
		$$ = new(OTEXT, nil, nil);
		$$->s = $1;
	}

string:
	Tstring
%%
