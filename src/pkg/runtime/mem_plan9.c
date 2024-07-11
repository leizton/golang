// Copyright 2010 The Go Authors.  All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "runtime.h"
#include "arch_GOARCH.h"
#include "malloc.h"
#include "os_GOOS.h"

extern byte end[];
static byte *bloc = { end };
static Lock memlock;

enum
{
	Round = 4095
};

void*
runtime_SysAlloc(uintptr nbytes)
{
	uintptr bl;
	
	runtime_lock(&memlock);
	mstats.sys += nbytes;
	// Plan 9 sbrk from /sys/src/libc/9sys/sbrk.c
	bl = ((uintptr)bloc + Round) & ~Round;
	if(runtime_brk_((void*)(bl + nbytes)) < 0) {
		runtime_unlock(&memlock);
		return nil;
	}
	bloc = (byte*)bl + nbytes;
	runtime_unlock(&memlock);
	return (void*)bl;
}

void
runtime_SysFree(void *v, uintptr nbytes)
{
	runtime_lock(&memlock);
	mstats.sys -= nbytes;
	// from tiny/mem.c
	// Push pointer back if this is a free
	// of the most recent SysAlloc.
	nbytes += (nbytes + Round) & ~Round;
	if(bloc == (byte*)v+nbytes)
		bloc -= nbytes;	
	runtime_unlock(&memlock);
}

void
runtime_SysUnused(void *v, uintptr nbytes)
{
	USED(v, nbytes);
}

void
runtime_SysMap(void *v, uintptr nbytes)
{
	USED(v, nbytes);
}

void*
runtime_SysReserve(void *v, uintptr nbytes)
{
	USED(v);
	return runtime_SysAlloc(nbytes);
}
