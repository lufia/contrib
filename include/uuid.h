#pragma src "/sys/src/libuuid"
#pragma lib "libuuid.a"
#pragma varargck type "U" uchar*

enum {
	UUIDlen = 16,
};

extern int	uuidgen(uchar b[UUIDlen]);
extern int	uuidgenrand(uchar b[UUIDlen]);
extern int	uuidfmt(Fmt *);
