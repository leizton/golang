// Use of this source file is governed by a BSD-style
// license that can be found in the LICENSE file.`

#include "runtime.h"
#include "defs_GOOS_GOARCH.h"
#include "os_GOOS.h"
#include "stack.h"

enum
{
	ESRCH = 3,
	ENOTSUP = 91,

	// From OpenBSD's sys/time.h
	CLOCK_REALTIME = 0,
	CLOCK_VIRTUAL = 1,
	CLOCK_PROF = 2,
	CLOCK_MONOTONIC = 3
};

extern SigTab runtime_sigtab[];

extern int64 runtime_rfork_thread(int32 flags, void *stack, M *m, G *g, void (*fn)(void));
extern int32 runtime_thrsleep(void *ident, int32 clock_id, void *tsp, void *lock);
extern int32 runtime_thrwakeup(void *ident, int32 n);

// From OpenBSD's <sys/sysctl.h>
#define	CTL_HW	6
#define	HW_NCPU	3

static int32
getncpu(void)
{
	uint32 mib[2];
	uint32 out;
	int32 ret;
	uintptr nout;

	// Fetch hw.ncpu via sysctl.
	mib[0] = CTL_HW;
	mib[1] = HW_NCPU;
	nout = sizeof out;
	out = 0;
	ret = runtime_sysctl(mib, 2, (byte*)&out, &nout, nil, 0);
	if(ret >= 0)
		return out;
	else
		return 1;
}

uintptr
runtime_semacreate(void)
{
	return 1;
}

int32
runtime_semasleep(int64 ns)
{
	Timespec ts;

	// spin-mutex lock
	while(runtime_xchg(&m->waitsemalock, 1))
		runtime_osyield();

	for(;;) {
		// lock held
		if(m->waitsemacount == 0) {
			// sleep until semaphore != 0 or timeout.
			// thrsleep unlocks m->waitsemalock.
			if(ns < 0)
				runtime_thrsleep(&m->waitsemacount, 0, nil, &m->waitsemalock);
			else {
				ns += runtime_nanotime();
				ts.tv_sec = ns/1000000000LL;
				ts.tv_nsec = ns%1000000000LL;
				runtime_thrsleep(&m->waitsemacount, CLOCK_REALTIME, &ts, &m->waitsemalock);
			}
			// reacquire lock
			while(runtime_xchg(&m->waitsemalock, 1))
				runtime_osyield();
		}

		// lock held (again)
		if(m->waitsemacount != 0) {
			// semaphore is available.
			m->waitsemacount--;
			// spin-mutex unlock
			runtime_atomicstore(&m->waitsemalock, 0);
			return 0;  // semaphore acquired
		}

		// semaphore not available.
		// if there is a timeout, stop now.
		// otherwise keep trying.
		if(ns >= 0)
			break;
	}

	// lock held but giving up
	// spin-mutex unlock
	runtime_atomicstore(&m->waitsemalock, 0);
	return -1;
}

void
runtime_semawakeup(M *mp)
{
	uint32 ret;

	// spin-mutex lock
	while(runtime_xchg(&mp->waitsemalock, 1))
		runtime_osyield();
	mp->waitsemacount++;
	ret = runtime_thrwakeup(&mp->waitsemacount, 1);
	if(ret != 0 && ret != ESRCH)
		runtime_printf("thrwakeup addr=%p sem=%d ret=%d\n", &mp->waitsemacount, mp->waitsemacount, ret);
	// spin-mutex unlock
	runtime_atomicstore(&mp->waitsemalock, 0);
}

// From OpenBSD's sys/param.h
#define	RFPROC		(1<<4)	/* change child (else changes curproc) */
#define	RFMEM		(1<<5)	/* share `address space' */
#define	RFNOWAIT	(1<<6)	/* parent need not wait() on child */
#define	RFTHREAD	(1<<13)	/* create a thread, not a process */

void
runtime_newosproc(M *m, G *g, void *stk, void (*fn)(void))
{
	int32 flags;
	int32 ret;

	flags = RFPROC | RFTHREAD | RFMEM | RFNOWAIT;

	if (0) {
		runtime_printf(
			"newosproc stk=%p m=%p g=%p fn=%p id=%d/%d ostk=%p\n",
			stk, m, g, fn, m->id, m->tls[0], &m);
	}

	m->tls[0] = m->id;	// so 386 asm can find it

	if((ret = runtime_rfork_thread(flags, stk, m, g, fn)) < 0) {
		runtime_printf("runtime: failed to create new OS thread (have %d already; errno=%d)\n", runtime_mcount() - 1, -ret);
		if (ret == -ENOTSUP)
			runtime_printf("runtime: is kern.rthreads disabled?\n");
		runtime_throw("runtime.newosproc");
	}
}

void
runtime_osinit(void)
{
	runtime_ncpu = getncpu();
}

void
runtime_goenvs(void)
{
	runtime_goenvs_unix();
}

// Called to initialize a new m (including the bootstrap m).
void
runtime_minit(void)
{
	// Initialize signal handling
	m->gsignal = runtime_malg(32*1024);
	runtime_signalstack(m->gsignal->stackguard - StackGuard, 32*1024);
}

void
runtime_sigpanic(void)
{
	switch(g->sig) {
	case SIGBUS:
		if(g->sigcode0 == BUS_ADRERR && g->sigcode1 < 0x1000) {
			if(g->sigpc == 0)
				runtime_panicstring("call of nil func value");
			runtime_panicstring("invalid memory address or nil pointer dereference");
		}
		runtime_printf("unexpected fault address %p\n", g->sigcode1);
		runtime_throw("fault");
	case SIGSEGV:
		if((g->sigcode0 == 0 || g->sigcode0 == SEGV_MAPERR || g->sigcode0 == SEGV_ACCERR) && g->sigcode1 < 0x1000) {
			if(g->sigpc == 0)
				runtime_panicstring("call of nil func value");
			runtime_panicstring("invalid memory address or nil pointer dereference");
		}
		runtime_printf("unexpected fault address %p\n", g->sigcode1);
		runtime_throw("fault");
	case SIGFPE:
		switch(g->sigcode0) {
		case FPE_INTDIV:
			runtime_panicstring("integer divide by zero");
		case FPE_INTOVF:
			runtime_panicstring("integer overflow");
		}
		runtime_panicstring("floating point error");
	}
	runtime_panicstring(runtime_sigtab[g->sig].name);
}

uintptr
runtime_memlimit(void)
{
	return 0;
}

void
runtime_setprof(bool on)
{
	USED(on);
}

static int8 badcallback[] = "runtime: cgo callback on thread not created by Go.\n";

// This runs on a foreign stack, without an m or a g.  No stack split.
#pragma textflag 7
void
runtime_badcallback(void)
{
	runtime_write(2, badcallback, sizeof badcallback - 1);
}

static int8 badsignal[] = "runtime: signal received on thread not created by Go.\n";

// This runs on a foreign stack, without an m or a g.  No stack split.
#pragma textflag 7
void
runtime_badsignal(void)
{
	runtime_write(2, badsignal, sizeof badsignal - 1);
}
