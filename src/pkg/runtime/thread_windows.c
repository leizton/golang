// Copyright 2009 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "runtime.h"
#include "type.h"
#include "defs_GOOS_GOARCH.h"
#include "os_GOOS.h"

#pragma dynimport runtime_CloseHandle CloseHandle "kernel32.dll"
#pragma dynimport runtime_CreateEvent CreateEventA "kernel32.dll"
#pragma dynimport runtime_CreateThread CreateThread "kernel32.dll"
#pragma dynimport runtime_CreateWaitableTimer CreateWaitableTimerA "kernel32.dll"
#pragma dynimport runtime_DuplicateHandle DuplicateHandle "kernel32.dll"
#pragma dynimport runtime_ExitProcess ExitProcess "kernel32.dll"
#pragma dynimport runtime_FreeEnvironmentStringsW FreeEnvironmentStringsW "kernel32.dll"
#pragma dynimport runtime_GetEnvironmentStringsW GetEnvironmentStringsW "kernel32.dll"
#pragma dynimport runtime_GetProcAddress GetProcAddress "kernel32.dll"
#pragma dynimport runtime_GetStdHandle GetStdHandle "kernel32.dll"
#pragma dynimport runtime_GetSystemInfo GetSystemInfo "kernel32.dll"
#pragma dynimport runtime_GetSystemTimeAsFileTime GetSystemTimeAsFileTime "kernel32.dll"
#pragma dynimport runtime_GetThreadContext GetThreadContext "kernel32.dll"
#pragma dynimport runtime_LoadLibrary LoadLibraryW "kernel32.dll"
#pragma dynimport runtime_ResumeThread ResumeThread "kernel32.dll"
#pragma dynimport runtime_SetConsoleCtrlHandler SetConsoleCtrlHandler "kernel32.dll"
#pragma dynimport runtime_SetEvent SetEvent "kernel32.dll"
#pragma dynimport runtime_SetThreadPriority SetThreadPriority "kernel32.dll"
#pragma dynimport runtime_SetWaitableTimer SetWaitableTimer "kernel32.dll"
#pragma dynimport runtime_Sleep Sleep "kernel32.dll"
#pragma dynimport runtime_SuspendThread SuspendThread "kernel32.dll"
#pragma dynimport runtime_timeBeginPeriod timeBeginPeriod "winmm.dll"
#pragma dynimport runtime_WaitForSingleObject WaitForSingleObject "kernel32.dll"
#pragma dynimport runtime_WriteFile WriteFile "kernel32.dll"

extern void *runtime_CloseHandle;
extern void *runtime_CreateEvent;
extern void *runtime_CreateThread;
extern void *runtime_CreateWaitableTimer;
extern void *runtime_DuplicateHandle;
extern void *runtime_ExitProcess;
extern void *runtime_FreeEnvironmentStringsW;
extern void *runtime_GetEnvironmentStringsW;
extern void *runtime_GetProcAddress;
extern void *runtime_GetStdHandle;
extern void *runtime_GetSystemInfo;
extern void *runtime_GetSystemTimeAsFileTime;
extern void *runtime_GetThreadContext;
extern void *runtime_LoadLibrary;
extern void *runtime_ResumeThread;
extern void *runtime_SetConsoleCtrlHandler;
extern void *runtime_SetEvent;
extern void *runtime_SetThreadPriority;
extern void *runtime_SetWaitableTimer;
extern void *runtime_Sleep;
extern void *runtime_SuspendThread;
extern void *runtime_timeBeginPeriod;
extern void *runtime_WaitForSingleObject;
extern void *runtime_WriteFile;

static int32
getproccount(void)
{
	SystemInfo info;

	runtime_stdcall(runtime_GetSystemInfo, 1, &info);
	return info.dwNumberOfProcessors;
}

void
runtime_osinit(void)
{
	// -1 = current process, -2 = current thread
	runtime_stdcall(runtime_DuplicateHandle, 7,
		(uintptr)-1, (uintptr)-2, (uintptr)-1, &m->thread,
		(uintptr)0, (uintptr)0, (uintptr)DUPLICATE_SAME_ACCESS);
	runtime_stdcall(runtime_SetConsoleCtrlHandler, 2, runtime_ctrlhandler, (uintptr)1);
	runtime_stdcall(runtime_timeBeginPeriod, 1, (uintptr)1);
	runtime_ncpu = getproccount();
}

void
runtime_goenvs(void)
{
	extern Slice syscall_envs;

	uint16 *env;
	String *s;
	int32 i, n;
	uint16 *p;

	env = runtime_stdcall(runtime_GetEnvironmentStringsW, 0);

	n = 0;
	for(p=env; *p; n++)
		p += runtime_findnullw(p)+1;

	s = runtime_malloc(n*sizeof s[0]);

	p = env;
	for(i=0; i<n; i++) {
		s[i] = runtime_gostringw(p);
		p += runtime_findnullw(p)+1;
	}
	syscall_envs.array = (byte*)s;
	syscall_envs.len = n;
	syscall_envs.cap = n;

	runtime_stdcall(runtime_FreeEnvironmentStringsW, 1, env);
}

void
runtime_exit(int32 code)
{
	runtime_stdcall(runtime_ExitProcess, 1, (uintptr)code);
}

int32
runtime_write(int32 fd, void *buf, int32 n)
{
	void *handle;
	uint32 written;

	written = 0;
	switch(fd) {
	case 1:
		handle = runtime_stdcall(runtime_GetStdHandle, 1, (uintptr)-11);
		break;
	case 2:
		handle = runtime_stdcall(runtime_GetStdHandle, 1, (uintptr)-12);
		break;
	default:
		return -1;
	}
	runtime_stdcall(runtime_WriteFile, 5, handle, buf, (uintptr)n, &written, (uintptr)0);
	return written;
}

void
runtime_osyield(void)
{
	runtime_stdcall(runtime_Sleep, 1, (uintptr)0);
}

void
runtime_usleep(uint32 us)
{
	us /= 1000;
	if(us == 0)
		us = 1;
	runtime_stdcall(runtime_Sleep, 1, (uintptr)us);
}

#define INFINITE ((uintptr)0xFFFFFFFF)

int32
runtime_semasleep(int64 ns)
{
	uintptr ms;

	if(ns < 0)
		ms = INFINITE;
	else if(ns/1000000 > 0x7fffffffLL)
		ms = 0x7fffffff;
	else {
		ms = ns/1000000;
		if(ms == 0)
			ms = 1;
	}
	if(runtime_stdcall(runtime_WaitForSingleObject, 2, m->waitsema, ms) != 0)
		return -1;  // timeout
	return 0;
}

void
runtime_semawakeup(M *mp)
{
	runtime_stdcall(runtime_SetEvent, 1, mp->waitsema);
}

uintptr
runtime_semacreate(void)
{
	return (uintptr)runtime_stdcall(runtime_CreateEvent, 4, (uintptr)0, (uintptr)0, (uintptr)0, (uintptr)0);
}

#define STACK_SIZE_PARAM_IS_A_RESERVATION ((uintptr)0x00010000)

void
runtime_newosproc(M *m, G *g, void *stk, void (*fn)(void))
{
	void *thandle;

	USED(stk);
	USED(g);	// assuming g = m->g0
	USED(fn);	// assuming fn = mstart

	thandle = runtime_stdcall(runtime_CreateThread, 6,
		nil, (uintptr)0x20000, runtime_tstart_stdcall, m,
		STACK_SIZE_PARAM_IS_A_RESERVATION, nil);
	if(thandle == nil) {
		runtime_printf("runtime: failed to create new OS thread (have %d already; errno=%d)\n", runtime_mcount(), runtime_getlasterror());
		runtime_throw("runtime.newosproc");
	}
	runtime_atomicstorep(&m->thread, thandle);
}

// Called to initialize a new m (including the bootstrap m).
void
runtime_minit(void)
{
}

int64
runtime_nanotime(void)
{
	int64 filetime;

	runtime_stdcall(runtime_GetSystemTimeAsFileTime, 1, &filetime);

	// Filetime is 100s of nanoseconds since January 1, 1601.
	// Convert to nanoseconds since January 1, 1970.
	return (filetime - 116444736000000000LL) * 100LL;
}

void
time_now(int64 sec, int32 usec)
{
	int64 ns;

	ns = runtime_nanotime();
	sec = ns / 1000000000LL;
	usec = ns - sec * 1000000000LL;
	FLUSH(&sec);
	FLUSH(&usec);
}

// Calling stdcall on os stack.
#pragma textflag 7
void *
runtime_stdcall(void *fn, int32 count, ...)
{
	WinCall c;

	c.fn = fn;
	c.n = count;
	c.args = (uintptr*)&count + 1;
	runtime_asmcgocall(runtime_asmstdcall, &c);
	return (void*)c.r1;
}

uint32
runtime_issigpanic(uint32 code)
{
	switch(code) {
	case EXCEPTION_ACCESS_VIOLATION:
	case EXCEPTION_INT_DIVIDE_BY_ZERO:
	case EXCEPTION_INT_OVERFLOW:
	case EXCEPTION_FLT_DENORMAL_OPERAND:
	case EXCEPTION_FLT_DIVIDE_BY_ZERO:
	case EXCEPTION_FLT_INEXACT_RESULT:
	case EXCEPTION_FLT_OVERFLOW:
	case EXCEPTION_FLT_UNDERFLOW:
		return 1;
	}
	return 0;
}

void
runtime_sigpanic(void)
{
	switch(g->sig) {
	case EXCEPTION_ACCESS_VIOLATION:
		if(g->sigcode1 < 0x1000) {
			if(g->sigpc == 0)
				runtime_panicstring("call of nil func value");
			runtime_panicstring("invalid memory address or nil pointer dereference");
		}
		runtime_printf("unexpected fault address %p\n", g->sigcode1);
		runtime_throw("fault");
	case EXCEPTION_INT_DIVIDE_BY_ZERO:
		runtime_panicstring("integer divide by zero");
	case EXCEPTION_INT_OVERFLOW:
		runtime_panicstring("integer overflow");
	case EXCEPTION_FLT_DENORMAL_OPERAND:
	case EXCEPTION_FLT_DIVIDE_BY_ZERO:
	case EXCEPTION_FLT_INEXACT_RESULT:
	case EXCEPTION_FLT_OVERFLOW:
	case EXCEPTION_FLT_UNDERFLOW:
		runtime_panicstring("floating point error");
	}
	runtime_throw("fault");
}

extern void *runtime_sigtramp;

void
runtime_initsig(void)
{
	// following line keeps sigtramp alive at link stage
	// if there's a better way please write it here
	void *p = runtime_sigtramp;
	USED(p);
}

uint32
runtime_ctrlhandler1(uint32 type)
{
	int32 s;

	switch(type) {
	case CTRL_C_EVENT:
	case CTRL_BREAK_EVENT:
		s = SIGINT;
		break;
	default:
		return 0;
	}

	if(runtime_sigsend(s))
		return 1;
	runtime_exit(2);	// SIGINT, SIGTERM, etc
	return 0;
}

extern void runtime_dosigprof(Context *r, G *gp);
extern void runtime_profileloop(void);
static void *profiletimer;

static void
profilem(M *mp)
{
	extern M runtime_m0;
	extern uint32 runtime_tls0[];
	byte rbuf[sizeof(Context)+15];
	Context *r;
	void *tls;
	G *gp;

	tls = mp->tls;
	if(mp == &runtime_m0)
		tls = runtime_tls0;
	gp = *(G**)tls;

	if(gp != nil && gp != mp->g0 && gp->status != Gsyscall) {
		// align Context to 16 bytes
		r = (Context*)((uintptr)(&rbuf[15]) & ~15);
		r->ContextFlags = CONTEXT_CONTROL;
		runtime_stdcall(runtime_GetThreadContext, 2, mp->thread, r);
		runtime_dosigprof(r, gp);
	}
}

void
runtime_profileloop1(void)
{
	M *mp, *allm;
	void *thread;

	runtime_stdcall(runtime_SetThreadPriority, 2,
		(uintptr)-2, (uintptr)THREAD_PRIORITY_HIGHEST);

	for(;;) {
		runtime_stdcall(runtime_WaitForSingleObject, 2, profiletimer, (uintptr)-1);
		allm = runtime_atomicloadp(&runtime_allm);
		for(mp = allm; mp != nil; mp = mp->alllink) {
			thread = runtime_atomicloadp(&mp->thread);
			if(thread == nil)
				continue;
			runtime_stdcall(runtime_SuspendThread, 1, thread);
			if(mp->profilehz != 0)
				profilem(mp);
			runtime_stdcall(runtime_ResumeThread, 1, thread);
		}
	}
}

void
runtime_resetcpuprofiler(int32 hz)
{
	static Lock lock;
	void *timer, *thread;
	int32 ms;
	int64 due;

	runtime_lock(&lock);
	if(profiletimer == nil) {
		timer = runtime_stdcall(runtime_CreateWaitableTimer, 3, nil, nil, nil);
		runtime_atomicstorep(&profiletimer, timer);
		thread = runtime_stdcall(runtime_CreateThread, 6,
			nil, nil, runtime_profileloop, nil, nil, nil);
		runtime_stdcall(runtime_CloseHandle, 1, thread);
	}
	runtime_unlock(&lock);

	ms = 0;
	due = 1LL<<63;
	if(hz > 0) {
		ms = 1000 / hz;
		if(ms == 0)
			ms = 1;
		due = ms * -10000;
	}
	runtime_stdcall(runtime_SetWaitableTimer, 6,
		profiletimer, &due, (uintptr)ms, nil, nil, nil);
	runtime_atomicstore((uint32*)&m->profilehz, hz);
}

void
os_sigpipe(void)
{
	runtime_throw("too many writes on closed pipe");
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

int8 runtime_badcallbackmsg[] = "runtime: cgo callback on thread not created by Go.\n";
int32 runtime_badcallbacklen = sizeof runtime_badcallbackmsg - 1;

int8 runtime_badsignalmsg[] = "runtime: signal received on thread not created by Go.\n";
int32 runtime_badsignallen = sizeof runtime_badsignalmsg - 1;
