// Copyright 2009 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "runtime.h"
#include "stack.h"

enum {
	maxround = sizeof(uintptr),
};

uint32	runtime_panicking;
void	(*runtime_destroylock)(Lock*);

/*
 * We assume that all architectures turn faults and the like
 * into apparent calls to runtime.sigpanic.  If we see a "call"
 * to runtime.sigpanic, we do not back up the PC to find the
 * line number of the CALL instruction, because there is no CALL.
 */
void	runtime_sigpanic(void);

int32
runtime_gotraceback(void)
{
	byte *p;

	p = runtime_getenv("GOTRACEBACK");
	if(p == nil || p[0] == '\0')
		return 1;	// default is on
	return runtime_atoi(p);
}

static Lock paniclk;

void
runtime_startpanic(void)
{
	if(m->dying) {
		runtime_printf("panic during panic\n");
		runtime_exit(3);
	}
	m->dying = 1;
	runtime_xadd(&runtime_panicking, 1);
	runtime_lock(&paniclk);
}

void
runtime_dopanic(int32 unused)
{
	static bool didothers;

	if(g->sig != 0)
		runtime_printf("[signal %x code=%p addr=%p pc=%p]\n",
			g->sig, g->sigcode0, g->sigcode1, g->sigpc);

	if(runtime_gotraceback()){
		if(g != m->g0) {
			runtime_printf("\n");
			runtime_goroutineheader(g);
			runtime_traceback(runtime_getcallerpc(&unused), runtime_getcallersp(&unused), 0, g);
		}
		if(!didothers) {
			didothers = true;
			runtime_tracebackothers(g);
		}
	}
	runtime_unlock(&paniclk);
	if(runtime_xadd(&runtime_panicking, -1) != 0) {
		// Some other m is panicking too.
		// Let it print what it needs to print.
		// Wait forever without chewing up cpu.
		// It will exit when it's done.
		static Lock deadlock;
		runtime_lock(&deadlock);
		runtime_lock(&deadlock);
	}

	runtime_exit(2);
}

void
runtime_panicindex(void)
{
	runtime_panicstring("index out of range");
}

void
runtime_panicslice(void)
{
	runtime_panicstring("slice bounds out of range");
}

void
runtime_throwreturn(void)
{
	// can only happen if compiler is broken
	runtime_throw("no return at end of a typed function - compiler is broken");
}

void
runtime_throwinit(void)
{
	// can only happen with linker skew
	runtime_throw("recursive call during initialization - linker skew");
}

void
runtime_throw(int8 *s)
{
	runtime_startpanic();
	runtime_printf("throw: %s\n", s);
	runtime_dopanic(0);
	*(int32*)0 = 0;	// not reached
	runtime_exit(1);	// even more not reached
}

void
runtime_panicstring(int8 *s)
{
	Eface err;

	if(m->gcing) {
		runtime_printf("panic: %s\n", s);
		runtime_throw("panic during gc");
	}
	runtime_newErrorString(runtime_gostringnocopy((byte*)s), &err);
	runtime_panic(err);
}

int32
runtime_mcmp(byte *s1, byte *s2, uint32 n)
{
	uint32 i;
	byte c1, c2;

	for(i=0; i<n; i++) {
		c1 = s1[i];
		c2 = s2[i];
		if(c1 < c2)
			return -1;
		if(c1 > c2)
			return +1;
	}
	return 0;
}


byte*
runtime_mchr(byte *p, byte c, byte *ep)
{
	for(; p < ep; p++)
		if(*p == c)
			return p;
	return nil;
}

static int32	argc;
static uint8**	argv;

Slice os_Args;
Slice syscall_envs;

void (*runtime_sysargs)(int32, uint8**);

void
runtime_args(int32 c, uint8 **v)
{
	argc = c;
	argv = v;
	if(runtime_sysargs != nil)
		runtime_sysargs(c, v);
}

int32 runtime_isplan9;
int32 runtime_iswindows;

void
runtime_goargs(void)
{
	String *s;
	int32 i;

	// for windows implementation see "os" package
	if(Windows)
		return;

	s = runtime_malloc(argc*sizeof s[0]);
	for(i=0; i<argc; i++)
		s[i] = runtime_gostringnocopy(argv[i]);
	os_Args.array = (byte*)s;
	os_Args.len = argc;
	os_Args.cap = argc;
}

void
runtime_goenvs_unix(void)
{
	String *s;
	int32 i, n;

	for(n=0; argv[argc+1+n] != 0; n++)
		;

	s = runtime_malloc(n*sizeof s[0]);
	for(i=0; i<n; i++)
		s[i] = runtime_gostringnocopy(argv[argc+1+i]);
	syscall_envs.array = (byte*)s;
	syscall_envs.len = n;
	syscall_envs.cap = n;
}

byte*
runtime_getenv(int8 *s)
{
	int32 i, j, len;
	byte *v, *bs;
	String* envv;
	int32 envc;

	bs = (byte*)s;
	len = runtime_findnull(bs);
	envv = (String*)syscall_envs.array;
	envc = syscall_envs.len;
	for(i=0; i<envc; i++){
		if(envv[i].len <= len)
			continue;
		v = envv[i].str;
		for(j=0; j<len; j++)
			if(bs[j] != v[j])
				goto nomatch;
		if(v[len] != '=')
			goto nomatch;
		return v+len+1;
	nomatch:;
	}
	return nil;
}

void
runtime_getgoroot(String out)
{
	byte *p;

	p = runtime_getenv("GOROOT");
	out = runtime_gostringnocopy(p);
	FLUSH(&out);
}

int32
runtime_atoi(byte *p)
{
	int32 n;

	n = 0;
	while('0' <= *p && *p <= '9')
		n = n*10 + *p++ - '0';
	return n;
}

void
runtime_check(void)
{
	int8 a;
	uint8 b;
	int16 c;
	uint16 d;
	int32 e;
	uint32 f;
	int64 g;
	uint64 h;
	float32 i, i1;
	float64 j, j1;
	void* k;
	uint16* l;
	struct x1 {
		byte x;
	};
	struct y1 {
		struct x1 x1;
		byte y;
	};

	if(sizeof(a) != 1) runtime_throw("bad a");
	if(sizeof(b) != 1) runtime_throw("bad b");
	if(sizeof(c) != 2) runtime_throw("bad c");
	if(sizeof(d) != 2) runtime_throw("bad d");
	if(sizeof(e) != 4) runtime_throw("bad e");
	if(sizeof(f) != 4) runtime_throw("bad f");
	if(sizeof(g) != 8) runtime_throw("bad g");
	if(sizeof(h) != 8) runtime_throw("bad h");
	if(sizeof(i) != 4) runtime_throw("bad i");
	if(sizeof(j) != 8) runtime_throw("bad j");
	if(sizeof(k) != sizeof(uintptr)) runtime_throw("bad k");
	if(sizeof(l) != sizeof(uintptr)) runtime_throw("bad l");
	if(sizeof(struct x1) != 1) runtime_throw("bad sizeof x1");
	if(offsetof(struct y1, y) != 1) runtime_throw("bad offsetof y1.y");
	if(sizeof(struct y1) != 2) runtime_throw("bad sizeof y1");

	uint32 z;
	z = 1;
	if(!runtime_cas(&z, 1, 2))
		runtime_throw("cas1");
	if(z != 2)
		runtime_throw("cas2");

	z = 4;
	if(runtime_cas(&z, 5, 6))
		runtime_throw("cas3");
	if(z != 4)
		runtime_throw("cas4");

	*(uint64*)&j = ~0ULL;
	if(j == j)
		runtime_throw("float64nan");
	if(!(j != j))
		runtime_throw("float64nan1");

	*(uint64*)&j1 = ~1ULL;
	if(j == j1)
		runtime_throw("float64nan2");
	if(!(j != j1))
		runtime_throw("float64nan3");

	*(uint32*)&i = ~0UL;
	if(i == i)
		runtime_throw("float32nan");
	if(!(i != i))
		runtime_throw("float32nan1");

	*(uint32*)&i1 = ~1UL;
	if(i == i1)
		runtime_throw("float32nan2");
	if(!(i != i1))
		runtime_throw("float32nan3");
}

void
runtime_Caller(int32 skip, uintptr retpc, String retfile, int32 retline, bool retbool)
{
	Func *f, *g;
	uintptr pc;
	uintptr rpc[2];

	/*
	 * Ask for two PCs: the one we were asked for
	 * and what it called, so that we can see if it
	 * "called" sigpanic.
	 */
	retpc = 0;
	if(runtime_callers(1+skip-1, rpc, 2) < 2) {
		retfile = runtime_emptystring;
		retline = 0;
		retbool = false;
	} else if((f = runtime_findfunc(rpc[1])) == nil) {
		retfile = runtime_emptystring;
		retline = 0;
		retbool = true;  // have retpc at least
	} else {
		retpc = rpc[1];
		retfile = f->src;
		pc = retpc;
		g = runtime_findfunc(rpc[0]);
		if(pc > f->entry && (g == nil || g->entry != (uintptr)runtime_sigpanic))
			pc--;
		retline = runtime_funcline(f, pc);
		retbool = true;
	}
	FLUSH(&retpc);
	FLUSH(&retfile);
	FLUSH(&retline);
	FLUSH(&retbool);
}

void
runtime_Callers(int32 skip, Slice pc, int32 retn)
{
	// runtime.callers uses pc.array==nil as a signal
	// to print a stack trace.  Pick off 0-length pc here
	// so that we don't let a nil pc slice get to it.
	if(pc.len == 0)
		retn = 0;
	else
		retn = runtime_callers(skip, (uintptr*)pc.array, pc.len);
	FLUSH(&retn);
}

void
runtime_FuncForPC(uintptr pc, void *retf)
{
	retf = runtime_findfunc(pc);
	FLUSH(&retf);
}

uint32
runtime_fastrand1(void)
{
	uint32 x;

	x = m->fastrand;
	x += x;
	if(x & 0x80000000L)
		x ^= 0x88888eefUL;
	m->fastrand = x;
	return x;
}
