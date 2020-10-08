#include <u.h>
#include <libc.h>
#include <bio.h>

typedef struct Stream Stream;
typedef struct Block Block;

#pragma varargck type "B" Block*
#pragma varargck type "K" int
#pragma varargck type "P" uint

enum {
	Nrow = 4,
	Ncol = 5,

	/* key codes */
	Kascii = 0x05,	/* to Ascii mode */
	Ktcode = 0x0e,	/* to Tcode mode */

	/* trans-modes */
	Mascii = 0,
	Mtcode,

	/* table-modes */
	LL = 0,
	LR,
	RL,
	RR,

	/* positions */
	Left = 0,
	Right,
};

struct Stream {
	Biobufhdr *f;
	Block *b[Ncol];
	int i;		/* b[i] */
	char mode;
	char row;
};
struct Block {
	char type;

	Rune b[Nrow][Ncol];
};

#define ISCKEY(c) ((c) == Kascii || (c) == Ktcode)
#define HASH(c1, c2) ((c1)<<8 | (c2))

/*
 * -- type --
 * bit[0..1]: row
 * bit[2..4]: col
 * bit[5..6]: mode (LL, LR, RL or RR)
 *
 * macro name TYP? TYPE was defined by draw.h
 */
#define TYP(m, r, c) ((m)<<5 | (c)<<2 | (r))
#define MODE(t) (((t)>>5) & 0x03)
#define COL(t) (((t)>>2) & 0x07)
#define ROW(t) ((t) & 0x03)

/*
 * -- code --
 * bit[0..1]: row
 * bit[2..4]: col
 * bit[5]: pos (Left or Right)
 */
#define CODE(p, r, c) ((p)<<5 | (c)<<2 | (r))
#define POS(t) (((t)>>5) & 0x01)

/*
 * -- code pair --
 * bit[0..7]: first code
 * bit[8..15]: second code
 */
#define PAIR(c1, c2) ((c1)<<8 | (c2))
#define FIRST(m) (((m)>>8)&0xff)
#define SECOND(m) ((m) & 0xff)

extern void *emalloc(long n);
extern void *erealloc(void *p, long n);
extern char *estrdup(char *s);
extern Rune *strtorune(char *s);
extern int blockfmt(Fmt *fmt);
extern Stream *sopen(char *name, int mode);
extern void sclose(Stream *f);
extern void dictinit(char *fname);
extern Block *getblock(Stream *f);
extern void kbdinit(void);
extern int code(char pos, char row, char col);
extern void setblock(Block *b);
extern Rune getrune(char c1, char c2);
extern int codefmt(Fmt *fmt);
extern int runetopair(ushort *p, Rune c);
extern int pairfmt(Fmt *fmt);
extern int uread(int fd, char s[], int n);
