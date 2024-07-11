// Copyright 2010 The Go Authors.  All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "runtime.h"
#include "arch_GOARCH.h"
#include "defs_GOOS_GOARCH.h"
#include "os_GOOS.h"
#include "malloc.h"

enum
{
	ENOMEM = 12,
};

void*
runtime_SysAlloc(uintptr n)
{
	void *v;

	mstats.sys += n;
	v = runtime_mmap(nil, n, PROT_READ|PROT_WRITE|PROT_EXEC, MAP_ANON|MAP_PRIVATE, -1, 0);
	if(v < (void*)4096)
		return nil;
	return v;
}

void
runtime_SysUnused(void *v, uintptr n)
{
	USED(v);
	USED(n);
	// TODO(rsc): call madvise MADV_DONTNEED
}

void
runtime_SysFree(void *v, uintptr n)
{
	mstats.sys -= n;
	runtime_munmap(v, n);
}

void*
runtime_SysReserve(void *v, uintptr n)
{
	void *p;

	// On 64-bit, people with ulimit -v set complain if we reserve too
	// much address space.  Instead, assume that the reservation is okay
	// and check the assumption in SysMap.
	if(sizeof(void*) == 8)
		return v;

	p = runtime_mmap(v, n, PROT_NONE, MAP_ANON|MAP_PRIVATE, -1, 0);
	if (p == ((void *)-ENOMEM))
		return nil;
	else
		return p;
}

void
runtime_SysMap(void *v, uintptr n)
{
	void *p;
	
	mstats.sys += n;

	// On 64-bit, we don't actually have v reserved, so tread carefully.
	if(sizeof(void*) == 8) {
		p = runtime_mmap(v, n, PROT_READ|PROT_WRITE|PROT_EXEC, MAP_ANON|MAP_PRIVATE, -1, 0);
		if(p == (void*)-ENOMEM)
			runtime_throw("runtime: out of memory");
		if(p != v) {
			runtime_printf("runtime: address space conflict: map(%p) = %p\n", v, p);
			runtime_throw("runtime: address space conflict");
		}
		return;
	}

	p = runtime_mmap(v, n, PROT_READ|PROT_WRITE|PROT_EXEC, MAP_ANON|MAP_FIXED|MAP_PRIVATE, -1, 0);
	if(p == (void*)-ENOMEM)
		runtime_throw("runtime: out of memory");
	if(p != v)
		runtime_throw("runtime: cannot map pages in arena address space");
}
