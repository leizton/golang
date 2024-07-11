// Copyright 2010 The Go Authors.  All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "runtime.h"
#include "os_GOOS.h"
#include "arch_GOARCH.h"

int8 *goos = "plan9";

void
runtime_minit(void)
{
}

static int32
getproccount(void)
{
	int32 fd, i, n, ncpu;
	byte buf[2048];

	fd = runtime_open((byte*)"/dev/sysstat", OREAD);
	if(fd < 0)
		return 1;
	ncpu = 0;
	for(;;) {
		n = runtime_read(fd, buf, sizeof buf);
		if(n <= 0)
			break;
		for(i = 0; i < n; i++) {
			if(buf[i] == '\n')
				ncpu++;
		}
	}
	runtime_close(fd);
	return ncpu > 0 ? ncpu : 1;
}

void
runtime_osinit(void)
{
	runtime_ncpu = getproccount();
}

void
runtime_goenvs(void)
{
}

void
runtime_initsig(void)
{
}

void
runtime_osyield(void)
{
	runtime_sleep(0);
}

void
runtime_usleep(uint32 µs)
{
	uint32 ms;

	ms = µs/1000;
	if(ms == 0)
		ms = 1;
	runtime_sleep(ms);
}

int64
runtime_nanotime(void)
{
	static int32 fd = -1;
	byte b[8];
	uint32 hi, lo;

	// As long as all goroutines share the same file
	// descriptor table we can get away with using
	// just a static fd.  Without a lock the file can
	// be opened twice but that's okay.
	//
	// Using /dev/bintime gives us a latency on the
	// order of ten microseconds between two calls.
	//
	// The naïve implementation (without the cached
	// file descriptor) is roughly four times slower
	// in 9vx on a 2.16 GHz Intel Core 2 Duo.

	if(fd < 0 && (fd = runtime_open((byte*)"/dev/bintime", OREAD|OCEXEC)) < 0)
		return 0;
	if(runtime_pread(fd, b, sizeof b, 0) != sizeof b)
		return 0;
	hi = b[0]<<24 | b[1]<<16 | b[2]<<8 | b[3];
	lo = b[4]<<24 | b[5]<<16 | b[6]<<8 | b[7];
	return (int64)hi<<32 | (int64)lo;
}

void
time_now(int64 sec, int32 nsec)
{
	int64 ns;

	ns = runtime_nanotime();
	sec = ns / 1000000000LL;
	nsec = ns - sec * 1000000000LL;
	FLUSH(&sec);
	FLUSH(&nsec);
}

extern Tos *_tos;
void
runtime_exit(int32)
{
	int32 fd;
	uint8 buf[128];
	uint8 tmp[16];
	uint8 *p, *q;
	int32 pid;

	runtime_memclr(buf, sizeof buf);
	runtime_memclr(tmp, sizeof tmp);
	pid = _tos->pid;

	/* build path string /proc/pid/notepg */
	for(q=tmp; pid > 0;) {
		*q++ = '0' + (pid%10);
		pid = pid/10;
	}
	p = buf;
	runtime_memmove((void*)p, (void*)"/proc/", 6);
	p += 6;
	for(q--; q >= tmp;)
		*p++ = *q--;
	runtime_memmove((void*)p, (void*)"/notepg", 7);

	/* post interrupt note */
	fd = runtime_open(buf, OWRITE);
	runtime_write(fd, "interrupt", 9);
	runtime_exits(nil);
}

void
runtime_newosproc(M *m, G *g, void *stk, void (*fn)(void))
{
	m->tls[0] = m->id;	// so 386 asm can find it
	if(0){
		runtime_printf("newosproc stk=%p m=%p g=%p fn=%p rfork=%p id=%d/%d ostk=%p\n",
			stk, m, g, fn, runtime_rfork, m->id, m->tls[0], &m);
	}

	if(runtime_rfork(RFPROC|RFMEM|RFNOWAIT, stk, m, g, fn) < 0)
		runtime_throw("newosproc: rfork failed");
}

uintptr
runtime_semacreate(void)
{
	return 1;
}

int32
runtime_semasleep(int64 ns)
{
	int32 ret;
	int32 ms;

	if(ns >= 0) {
		// TODO: Plan 9 needs a new system call, tsemacquire.
		// The kernel implementation is the same as semacquire
		// except with a tsleep and check for timeout.
		// It would be great if the implementation returned the
		// value that was added to the semaphore, so that on
		// timeout the return value would be 0, on success 1.
		// Then the error string does not have to be parsed
		// to detect timeout.
		//
		// If a negative time indicates no timeout, then
		// semacquire can be implemented (in the kernel)
		// as tsemacquire(p, v, -1).
		runtime_throw("semasleep: timed sleep not implemented on Plan 9");

		/*
		if(ns < 0)
			ms = -1;
		else if(ns/1000 > 0x7fffffffll)
			ms = 0x7fffffff;
		else
			ms = ns/1000;
		ret = runtime_plan9_tsemacquire(&m->waitsemacount, 1, ms);
		if(ret == 1)
			return 0;  // success
		return -1;  // timeout or interrupted
		*/
	}

	while(runtime_plan9_semacquire(&m->waitsemacount, 1) < 0) {
		/* interrupted; try again */
	}
	return 0;  // success
}

void
runtime_semawakeup(M *mp)
{
	runtime_plan9_semrelease(&mp->waitsemacount, 1);
}

void
os_sigpipe(void)
{
	runtime_throw("too many writes on closed pipe");
}

/*
 * placeholder - once notes are implemented,
 * a signal generating a panic must appear as
 * a call to this function for correct handling by
 * traceback.
 */
void
runtime_sigpanic(void)
{
}

int32
runtime_read(int32 fd, void *buf, int32 nbytes)
{
	return runtime_pread(fd, buf, nbytes, -1LL);
}

int32
runtime_write(int32 fd, void *buf, int32 nbytes)
{
	return runtime_pwrite(fd, buf, nbytes, -1LL);
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
	runtime_pwrite(2, badcallback, sizeof badcallback - 1, -1LL);
}

static int8 badsignal[] = "runtime: signal received on thread not created by Go.\n";

// This runs on a foreign stack, without an m or a g.  No stack split.
#pragma textflag 7
void
runtime_badsignal(void)
{
	runtime_pwrite(2, badsignal, sizeof badsignal - 1, -1LL);
}
