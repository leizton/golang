// Copyright 2009 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

/*
 * basic types
 */
typedef	signed char		int8;
typedef	unsigned char		uint8;
typedef	signed short		int16;
typedef	unsigned short		uint16;
typedef	signed int		int32;
typedef	unsigned int		uint32;
typedef	signed long long int	int64;
typedef	unsigned long long int	uint64;
typedef	float			float32;
typedef	double			float64;

#ifdef _64BIT
typedef	uint64		uintptr;
typedef	int64		intptr;
#else
typedef	uint32		uintptr;
typedef int32		intptr;
#endif

/*
 * get rid of C types
 * the / / / forces a syntax error immediately,
 * which will show "last name: XXunsigned".
 */
#define	unsigned		XXunsigned / / /
#define	signed			XXsigned / / /
#define	char			XXchar / / /
#define	short			XXshort / / /
#define	int			XXint / / /
#define	long			XXlong / / /
#define	float			XXfloat / / /
#define	double			XXdouble / / /

/*
 * defined types
 */
typedef	uint8			bool;
typedef	uint8			byte;
typedef	struct	Func		Func;
typedef	struct	G		G;
typedef	struct	Gobuf		Gobuf;
typedef	union	Lock		Lock;
typedef	struct	M		M;
typedef	struct	Mem		Mem;
typedef	union	Note		Note;
typedef	struct	Slice		Slice;
typedef	struct	Stktop		Stktop;
typedef	struct	String		String;
typedef	struct	SigTab		SigTab;
typedef	struct	MCache		MCache;
typedef	struct	FixAlloc	FixAlloc;
typedef	struct	Iface		Iface;
typedef	struct	Itab		Itab;
typedef	struct	Eface		Eface;
typedef	struct	Type		Type;
typedef	struct	ChanType		ChanType;
typedef	struct	MapType		MapType;
typedef	struct	Defer		Defer;
typedef	struct	Panic		Panic;
typedef	struct	Hmap		Hmap;
typedef	struct	Hchan		Hchan;
typedef	struct	Complex64	Complex64;
typedef	struct	Complex128	Complex128;
typedef	struct	WinCall		WinCall;
typedef	struct	Timers		Timers;
typedef	struct	Timer		Timer;

/*
 * per-cpu declaration.
 * "extern register" is a special storage class implemented by 6c, 8c, etc.
 * on machines with lots of registers, it allocates a register that will not be
 * used in generated code.  on the x86, it allocates a slot indexed by a
 * segment register.
 *
 * amd64: allocated downwards from R15
 * x86: allocated upwards from 0(GS)
 * arm: allocated downwards from R10
 *
 * every C file linked into a Go program must include runtime.h
 * so that the C compiler knows to avoid other uses of these registers.
 * the Go compilers know to avoid them.
 */
extern	register	G*	g;
extern	register	M*	m;

/*
 * defined constants
 */
enum
{
	// G status
	//
	// If you add to this list, add to the list
	// of "okay during garbage collection" status
	// in mgc0.c too.
	Gidle,
	Grunnable,
	Grunning,
	Gsyscall,
	Gwaiting,
	Gmoribund,
	Gdead,
};
enum
{
	true	= 1,
	false	= 0,
};

/*
 * structures
 */
union	Lock
{
	uint32	key;	// futex-based impl
	M*	waitm;	// linked list of waiting M's (sema-based impl)
};
union	Note
{
	uint32	key;	// futex-based impl
	M*	waitm;	// waiting M (sema-based impl)
};
struct String
{
	byte*	str;
	int32	len;
};
struct Iface
{
	Itab*	tab;
	void*	data;
};
struct Eface
{
	Type*	type;
	void*	data;
};
struct Complex64
{
	float32	real;
	float32	imag;
};
struct Complex128
{
	float64	real;
	float64	imag;
};

struct	Slice
{				// must not move anything
	byte*	array;		// actual data
	uint32	len;		// number of elements
	uint32	cap;		// allocated number of elements
};
struct	Gobuf
{
	// The offsets of these fields are known to (hard-coded in) libmach.
	byte*	sp;
	byte*	pc;
	G*	g;
};
struct	G
{
	byte*	stackguard;	// cannot move - also known to linker, libmach, runtime/cgo
	byte*	stackbase;	// cannot move - also known to libmach, runtime/cgo
	Defer*	defer;
	Panic*	panic;
	Gobuf	sched;
	byte*	gcstack;		// if status==Gsyscall, gcstack = stackbase to use during gc
	byte*	gcsp;		// if status==Gsyscall, gcsp = sched.sp to use during gc
	byte*	gcguard;		// if status==Gsyscall, gcguard = stackguard to use during gc
	byte*	stack0;
	byte*	entry;		// initial function
	G*	alllink;	// on allg
	void*	param;		// passed parameter on wakeup
	int16	status;
	int32	goid;
	uint32	selgen;		// valid sudog pointer
	int8*	waitreason;	// if status==Gwaiting
	G*	schedlink;
	bool	readyonstop;
	bool	ispanic;
	M*	m;		// for debuggers, but offset not hard-coded
	M*	lockedm;
	M*	idlem;
	int32	sig;
	int32	writenbuf;
	byte*	writebuf;
	uintptr	sigcode0;
	uintptr	sigcode1;
	uintptr	sigpc;
	uintptr	gopc;	// pc of go statement that created this goroutine
	uintptr	end[];
};
struct	M
{
	// The offsets of these fields are known to (hard-coded in) libmach.
	G*	g0;		// goroutine with scheduling stack
	void	(*morepc)(void);
	void*	moreargp;	// argument pointer for more stack
	Gobuf	morebuf;	// gobuf arg to morestack

	// Fields not known to debuggers.
	uint32	moreframesize;	// size arguments to morestack
	uint32	moreargsize;
	uintptr	cret;		// return value from C
	uint64	procid;		// for debuggers, but offset not hard-coded
	G*	gsignal;	// signal-handling G
	uint32	tls[8];		// thread-local storage (for 386 extern register)
	G*	curg;		// current running goroutine
	int32	id;
	int32	mallocing;
	int32	gcing;
	int32	locks;
	int32	nomemprof;
	int32	waitnextg;
	int32	dying;
	int32	profilehz;
	int32	helpgc;
	uint32	fastrand;
	uint64	ncgocall;
	Note	havenextg;
	G*	nextg;
	M*	alllink;	// on allm
	M*	schedlink;
	uint32	machport;	// Return address for Mach IPC (OS X)
	MCache	*mcache;
	FixAlloc	*stackalloc;
	G*	lockedg;
	G*	idleg;
	uintptr	createstack[32];	// Stack that created this thread.
	uint32	freglo[16];	// D[i] lsb and F[i]
	uint32	freghi[16];	// D[i] msb and F[i+16]
	uint32	fflag;		// floating point compare flags
	M*	nextwaitm;	// next M waiting for lock
	uintptr	waitsema;	// semaphore for parking on locks
	uint32	waitsemacount;
	uint32	waitsemalock;

#ifdef GOOS_windows
	void*	thread;		// thread handle
#endif
	uintptr	end[];
};

struct	Stktop
{
	// The offsets of these fields are known to (hard-coded in) libmach.
	uint8*	stackguard;
	uint8*	stackbase;
	Gobuf	gobuf;
	uint32	argsize;

	uint8*	argp;	// pointer to arguments in old frame
	uintptr	free;	// if free>0, call stackfree using free as size
	bool	panic;	// is this frame the top of a panic?
};
struct	SigTab
{
	int32	flags;
	int8	*name;
};
enum
{
	SigNotify = 1<<0,	// let signal.Notify have signal, even if from kernel
	SigKill = 1<<1,		// if signal.Notify doesn't take it, exit quietly
	SigThrow = 1<<2,	// if signal.Notify doesn't take it, exit loudly
	SigPanic = 1<<3,	// if the signal is from the kernel, panic
	SigDefault = 1<<4,	// if the signal isn't explicitly requested, don't monitor it
};

// NOTE(rsc): keep in sync with extern.go:/type.Func.
// Eventually, the loaded symbol table should be closer to this form.
struct	Func
{
	String	name;
	String	type;	// go type string
	String	src;	// src file name
	Slice	pcln;	// pc/ln tab for this func
	uintptr	entry;	// entry pc
	uintptr	pc0;	// starting pc, ln for table
	int32	ln0;
	int32	frame;	// stack frame size
	int32	args;	// number of 32-bit in/out args
	int32	locals;	// number of 32-bit locals
};

struct	WinCall
{
	void	(*fn)(void*);
	uintptr	n;	// number of parameters
	void*	args;	// parameters
	uintptr	r1;	// return values
	uintptr	r2;
	uintptr	err;	// error number
};

#ifdef GOOS_windows
enum {
   Windows = 1
};
#else
enum {
   Windows = 0
};
#endif

struct	Timers
{
	Lock;
	G	*timerproc;
	bool		sleeping;
	bool		rescheduling;
	Note	waitnote;
	Timer	**t;
	int32	len;
	int32	cap;
};

// Package time knows the layout of this structure.
// If this struct changes, adjust ../time/sleep.go:/runtimeTimer.
struct	Timer
{
	int32	i;		// heap index

	// Timer wakes up at when, and then at when+period, ... (period > 0 only)
	// each time calling f(now, arg) in the timer goroutine, so f must be
	// a well-behaved function and not block.
	int64	when;
	int64	period;
	void	(*f)(int64, Eface);
	Eface	arg;
};

/*
 * defined macros
 *    you need super-gopher-guru privilege
 *    to add this list.
 */
#define	nelem(x)	(sizeof(x)/sizeof((x)[0]))
#define	nil		((void*)0)
#define	offsetof(s,m)	(uint32)(&(((s*)0)->m))
#define	ROUND(x, n)	(((x)+(n)-1)&~((n)-1)) /* all-caps to mark as macro: it evaluates n twice */

/*
 * known to compiler
 */
enum {
	Structrnd = sizeof(uintptr)
};

/*
 * type algorithms - known to compiler
 */
enum
{
	AMEM,
	AMEM0,
	AMEM8,
	AMEM16,
	AMEM32,
	AMEM64,
	AMEM128,
	ANOEQ,
	ANOEQ0,
	ANOEQ8,
	ANOEQ16,
	ANOEQ32,
	ANOEQ64,
	ANOEQ128,
	ASTRING,
	AINTER,
	ANILINTER,
	ASLICE,
	AFLOAT32,
	AFLOAT64,
	ACPLX64,
	ACPLX128,
	Amax
};
typedef	struct	Alg		Alg;
struct	Alg
{
	void	(*hash)(uintptr*, uintptr, void*);
	void	(*equal)(bool*, uintptr, void*, void*);
	void	(*print)(uintptr, void*);
	void	(*copy)(uintptr, void*, void*);
};

extern	Alg	runtime_algarray[Amax];

void	runtime_memhash(uintptr*, uintptr, void*);
void	runtime_nohash(uintptr*, uintptr, void*);
void	runtime_strhash(uintptr*, uintptr, void*);
void	runtime_interhash(uintptr*, uintptr, void*);
void	runtime_nilinterhash(uintptr*, uintptr, void*);

void	runtime_memequal(bool*, uintptr, void*, void*);
void	runtime_noequal(bool*, uintptr, void*, void*);
void	runtime_strequal(bool*, uintptr, void*, void*);
void	runtime_interequal(bool*, uintptr, void*, void*);
void	runtime_nilinterequal(bool*, uintptr, void*, void*);

void	runtime_memprint(uintptr, void*);
void	runtime_strprint(uintptr, void*);
void	runtime_interprint(uintptr, void*);
void	runtime_nilinterprint(uintptr, void*);

void	runtime_memcopy(uintptr, void*, void*);
void	runtime_memcopy8(uintptr, void*, void*);
void	runtime_memcopy16(uintptr, void*, void*);
void	runtime_memcopy32(uintptr, void*, void*);
void	runtime_memcopy64(uintptr, void*, void*);
void	runtime_memcopy128(uintptr, void*, void*);
void	runtime_memcopy(uintptr, void*, void*);
void	runtime_strcopy(uintptr, void*, void*);
void	runtime_algslicecopy(uintptr, void*, void*);
void	runtime_intercopy(uintptr, void*, void*);
void	runtime_nilintercopy(uintptr, void*, void*);

/*
 * deferred subroutine calls
 */
struct Defer
{
	int32	siz;
	bool	nofree;
	byte*	argp;  // where args were copied from
	byte*	pc;
	byte*	fn;
	Defer*	link;
	byte	args[8];	// padded to actual size
};

/*
 * panics
 */
struct Panic
{
	Eface	arg;		// argument to panic
	byte*	stackbase;	// g->stackbase in panic
	Panic*	link;		// link to earlier panic
	bool	recovered;	// whether this panic is over
};

/*
 * external data
 */
extern	String	runtime_emptystring;
G*	runtime_allg;
G*	runtime_lastg;
M*	runtime_allm;
extern	int32	runtime_gomaxprocs;
extern	bool	runtime_singleproc;
extern	uint32	runtime_panicking;
extern	int32	runtime_gcwaiting;		// gc is waiting to run
int8*	runtime_goos;
int32	runtime_ncpu;
extern	bool	runtime_iscgo;
extern  void (*runtime_sysargs)(int32, uint8**);

/*
 * common functions and data
 */
int32	runtime_strcmp(byte*, byte*);
byte*	runtime_strstr(byte*, byte*);
int32	runtime_findnull(byte*);
int32	runtime_findnullw(uint16*);
void	runtime_dump(byte*, int32);
int32	runtime_runetochar(byte*, int32);
int32	runtime_charntorune(int32*, uint8*, int32);

/*
 * very low level c-called
 */
#define FLUSH(x)	USED(x)

void	runtime_gogo(Gobuf*, uintptr);
void	runtime_gogocall(Gobuf*, void(*)(void));
void	runtime_gosave(Gobuf*);
void	runtime_lessstack(void);
void	runtime_goargs(void);
void	runtime_goenvs(void);
void	runtime_goenvs_unix(void);
void*	runtime_getu(void);
void	runtime_throw(int8*);
void	runtime_panicstring(int8*);
void	runtime_prints(int8*);
void	runtime_printf(int8*, ...);
byte*	runtime_mchr(byte*, byte, byte*);
int32	runtime_mcmp(byte*, byte*, uint32);
void	runtime_memmove(void*, void*, uint32);
void*	runtime_mal(uintptr);
String	runtime_catstring(String, String);
String	runtime_gostring(byte*);
String  runtime_gostringn(byte*, int32);
Slice	runtime_gobytes(byte*, int32);
String	runtime_gostringnocopy(byte*);
String	runtime_gostringw(uint16*);
void	runtime_initsig(void);
void	runtime_sigenable(uint32 sig);
int32	runtime_gotraceback(void);
void	runtime_goroutineheader(G*);
void	runtime_traceback(uint8 *pc, uint8 *sp, uint8 *lr, G* gp);
void	runtime_tracebackothers(G*);
int32	runtime_write(int32, void*, int32);
int32	runtime_mincore(void*, uintptr, byte*);
bool	runtime_cas(uint32*, uint32, uint32);
bool	runtime_casp(void**, void*, void*);
// Don't confuse with XADD x86 instruction,
// this one is actually 'addx', that is, add-and-fetch.
uint32	runtime_xadd(uint32 volatile*, int32);
uint32	runtime_xchg(uint32 volatile*, uint32);
uint32	runtime_atomicload(uint32 volatile*);
void	runtime_atomicstore(uint32 volatile*, uint32);
void*	runtime_atomicloadp(void* volatile*);
void	runtime_atomicstorep(void* volatile*, void*);
void	runtime_jmpdefer(byte*, void*);
void	runtime_exit1(int32);
void	runtime_ready(G*);
byte*	runtime_getenv(int8*);
int32	runtime_atoi(byte*);
void	runtime_newosproc(M *m, G *g, void *stk, void (*fn)(void));
void	runtime_signalstack(byte*, int32);
G*	runtime_malg(int32);
void	runtime_asminit(void);
void	runtime_minit(void);
Func*	runtime_findfunc(uintptr);
int32	runtime_funcline(Func*, uintptr);
void*	runtime_stackalloc(uint32);
void	runtime_stackfree(void*, uintptr);
MCache*	runtime_allocmcache(void);
void	runtime_mallocinit(void);
bool	runtime_ifaceeq_c(Iface, Iface);
bool	runtime_efaceeq_c(Eface, Eface);
uintptr	runtime_ifacehash(Iface);
uintptr	runtime_efacehash(Eface);
void*	runtime_malloc(uintptr size);
void	runtime_free(void *v);
bool	runtime_addfinalizer(void*, void(*fn)(void*), int32);
void	runtime_runpanic(Panic*);
void*	runtime_getcallersp(void*);
int32	runtime_mcount(void);
int32	runtime_gcount(void);
void	runtime_mcall(void(*)(G*));
uint32	runtime_fastrand1(void);

void	runtime_exit(int32);
void	runtime_breakpoint(void);
void	runtime_gosched(void);
void	runtime_tsleep(int64);
M*	runtime_newm(void);
void	runtime_goexit(void);
void	runtime_asmcgocall(void (*fn)(void*), void*);
void	runtime_entersyscall(void);
void	runtime_exitsyscall(void);
G*	runtime_newproc1(byte*, byte*, int32, int32, void*);
bool	runtime_sigsend(int32 sig);
int32	runtime_callers(int32, uintptr*, int32);
int32	runtime_gentraceback(byte*, byte*, byte*, G*, int32, uintptr*, int32);
int64	runtime_nanotime(void);
void	runtime_dopanic(int32);
void	runtime_startpanic(void);
void	runtime_sigprof(uint8 *pc, uint8 *sp, uint8 *lr, G *gp);
void	runtime_resetcpuprofiler(int32);
void	runtime_setcpuprofilerate(void(*)(uintptr*, int32), int32);
void	runtime_usleep(uint32);
int64	runtime_cputicks(void);

#pragma	varargck	argpos	runtime_printf	1
#pragma	varargck	type	"d"	int32
#pragma	varargck	type	"d"	uint32
#pragma	varargck	type	"D"	int64
#pragma	varargck	type	"D"	uint64
#pragma	varargck	type	"x"	int32
#pragma	varargck	type	"x"	uint32
#pragma	varargck	type	"X"	int64
#pragma	varargck	type	"X"	uint64
#pragma	varargck	type	"p"	void*
#pragma	varargck	type	"p"	uintptr
#pragma	varargck	type	"s"	int8*
#pragma	varargck	type	"s"	uint8*
#pragma	varargck	type	"S"	String

void	runtime_stoptheworld(void);
void	runtime_starttheworld(bool);
extern uint32 runtime_worldsema;

/*
 * mutual exclusion locks.  in the uncontended case,
 * as fast as spin locks (just a few user-level instructions),
 * but on the contention path they sleep in the kernel.
 * a zeroed Lock is unlocked (no need to initialize each lock).
 */
void	runtime_lock(Lock*);
void	runtime_unlock(Lock*);

/*
 * sleep and wakeup on one-time events.
 * before any calls to notesleep or notewakeup,
 * must call noteclear to initialize the Note.
 * then, exactly one thread can call notesleep
 * and exactly one thread can call notewakeup (once).
 * once notewakeup has been called, the notesleep
 * will return.  future notesleep will return immediately.
 * subsequent noteclear must be called only after
 * previous notesleep has returned, e.g. it's disallowed
 * to call noteclear straight after notewakeup.
 *
 * notetsleep is like notesleep but wakes up after
 * a given number of nanoseconds even if the event
 * has not yet happened.  if a goroutine uses notetsleep to
 * wake up early, it must wait to call noteclear until it
 * can be sure that no other goroutine is calling
 * notewakeup.
 */
void	runtime_noteclear(Note*);
void	runtime_notesleep(Note*);
void	runtime_notewakeup(Note*);
void	runtime_notetsleep(Note*, int64);

/*
 * low-level synchronization for implementing the above
 */
uintptr	runtime_semacreate(void);
int32	runtime_semasleep(int64);
void	runtime_semawakeup(M*);
// or
void	runtime_futexsleep(uint32*, uint32, int64);
void	runtime_futexwakeup(uint32*, uint32);

/*
 * This is consistent across Linux and BSD.
 * If a new OS is added that is different, move this to
 * $GOOS/$GOARCH/defs.h.
 */
#define EACCES		13

/*
 * low level C-called
 */
uint8*	runtime_mmap(byte*, uintptr, int32, int32, int32, uint32);
void	runtime_munmap(byte*, uintptr);
void	runtime_madvise(byte*, uintptr, int32);
void	runtime_memclr(byte*, uintptr);
void	runtime_setcallerpc(void*, void*);
void*	runtime_getcallerpc(void*);

/*
 * runtime go-called
 */
void	runtime_printbool(bool);
void	runtime_printfloat(float64);
void	runtime_printint(int64);
void	runtime_printiface(Iface);
void	runtime_printeface(Eface);
void	runtime_printstring(String);
void	runtime_printpc(void*);
void	runtime_printpointer(void*);
void	runtime_printuint(uint64);
void	runtime_printhex(uint64);
void	runtime_printslice(Slice);
void	runtime_printcomplex(Complex128);
void	reflect_call(byte*, byte*, uint32);
void	runtime_panic(Eface);
void	runtime_panicindex(void);
void	runtime_panicslice(void);

/*
 * runtime c-called (but written in Go)
 */
void	runtime_printany(Eface);
void	runtime_newTypeAssertionError(String*, String*, String*, String*, Eface*);
void	runtime_newErrorString(String, Eface*);
void	runtime_fadd64c(uint64, uint64, uint64*);
void	runtime_fsub64c(uint64, uint64, uint64*);
void	runtime_fmul64c(uint64, uint64, uint64*);
void	runtime_fdiv64c(uint64, uint64, uint64*);
void	runtime_fneg64c(uint64, uint64*);
void	runtime_f32to64c(uint32, uint64*);
void	runtime_f64to32c(uint64, uint32*);
void	runtime_fcmp64c(uint64, uint64, int32*, bool*);
void	runtime_fintto64c(int64, uint64*);
void	runtime_f64tointc(uint64, int64*, bool*);

/*
 * wrapped for go users
 */
float64	runtime_Inf(int32 sign);
float64	runtime_NaN(void);
float32	runtime_float32frombits(uint32 i);
uint32	runtime_float32tobits(float32 f);
float64	runtime_float64frombits(uint64 i);
uint64	runtime_float64tobits(float64 f);
float64	runtime_frexp(float64 d, int32 *ep);
bool	runtime_isInf(float64 f, int32 sign);
bool	runtime_isNaN(float64 f);
float64	runtime_ldexp(float64 d, int32 e);
float64	runtime_modf(float64 d, float64 *ip);
void	runtime_semacquire(uint32*);
void	runtime_semrelease(uint32*);
int32	runtime_gomaxprocsfunc(int32 n);
void	runtime_procyield(uint32);
void	runtime_osyield(void);
void	runtime_LockOSThread(void);
void	runtime_UnlockOSThread(void);

void	runtime_mapassign(MapType*, Hmap*, byte*, byte*);
void	runtime_mapaccess(MapType*, Hmap*, byte*, byte*, bool*);
void	runtime_mapiternext(struct hash_iter*);
bool	runtime_mapiterkey(struct hash_iter*, void*);
void	runtime_mapiterkeyvalue(struct hash_iter*, void*, void*);
Hmap*	runtime_makemap_c(MapType*, int64);

Hchan*	runtime_makechan_c(ChanType*, int64);
void	runtime_chansend(ChanType*, Hchan*, byte*, bool*);
void	runtime_chanrecv(ChanType*, Hchan*, byte*, bool*, bool*);
int32	runtime_chanlen(Hchan*);
int32	runtime_chancap(Hchan*);
bool	runtime_showframe(Func*);

void	runtime_ifaceE2I(struct InterfaceType*, Eface, Iface*);

uintptr	runtime_memlimit(void);

// If appropriate, ask the operating system to control whether this
// thread should receive profiling signals.  This is only necessary on OS X.
// An operating system should not deliver a profiling signal to a
// thread that is not actually executing (what good is that?), but that's
// what OS X prefers to do.  When profiling is turned on, we mask
// away the profiling signal when threads go to sleep, so that OS X
// is forced to deliver the signal to a thread that's actually running.
// This is a no-op on other systems.
void	runtime_setprof(bool);
