#include <u.h>
#include <libc.h>
#include <uuid.h>

/* Epoch = (Unix date - Lilian date) / 100ns */
#define	Epoch	(141427LL*86400LL*10000000LL)

static int seq = -1;
static Lock seqLock;

static int
random(uchar *p, int n)
{
	int fd, rv;

	fd = open("/dev/random", OREAD);
	if(fd < 0)
		return -1;
	rv = readn(fd, p, n);
	close(fd);
	return rv;
}

static uchar *
putb(uchar *p, vlong v, int n)
{
	while(n-- > 0)
		*p++ = (v>>(n*8)) & 0xff;
	return p;
}

int
uuidgen(uchar p[UUIDlen])
{
	static vlong last;
	vlong t, a;
	int fd;
	uchar buf[2];
	char addr[6*2+1];

	t = nsec()/100 + Epoch;
	p = putb(p, t, 4);
	p = putb(p, t>>32, 2);
	p = putb(p, (t>>48)&0x0fff|0x1000, 2); /* Version 1 */

	lock(&seqLock);
	if(seq < 0){
		if(random(buf, sizeof buf) < 0){
			unlock(&seqLock);
			return -1;
		}
		seq = (buf[0]<<8 | buf[1]) & 0x3fff;
	}
	if(t <= last)
		seq = (seq+1) & 0x3fff;
	p = putb(p, seq, 2);
	unlock(&seqLock);

	fd = open("/net/ether0/addr", OREAD);
	if(readn(fd, addr, sizeof(addr)-1) < 0)
		return -1;
	addr[sizeof(addr)-1] = '\0';
	close(fd);
	a = strtoll(addr, nil, 16);
	putb(p, a, 48);
	return 0;
}

int
uuidgenrand(uchar p[UUIDlen])
{
	int n;

	n = random(p, UUIDlen);
	if(n < 0)
		return -1;
	p[6] = (p[6]&0xf) | 0x40;  /* version: 4 */
	p[8] = (p[8]&0x3f) | 0x80; /* variant: rfc4122 */
	return 0;
}

static int
encode(Fmt *fmt, uchar *p, int n, int delim)
{
	int c, r;
	uchar *ep;

	c = 0;
	for(ep = p+n; p < ep; p++){
		r = fmtprint(fmt, "%hhux", *p);
		if(r < 0)
			return -1;
		c += r;
	}
	if(delim){
		r = fmtprint(fmt, "-");
		if(r < 0)
			return -1;
		c += r;
	}
	return c;
}

static int blocksizes[] = { 4, 2, 2, 2, 6, 0 };

int
uuidfmt(Fmt *fmt)
{
	int c, r, *z, next;
	uchar *p;

	p = va_arg(fmt->args, uchar*);
	c = 0;
	for(z = blocksizes; *z; z++){ 
		next = *(z+1) != 0;
		r = encode(fmt, p, *z, next);
		if(r < 0)
			return -1;
		c += r;
		p += *z;
	}
	return c;
}
