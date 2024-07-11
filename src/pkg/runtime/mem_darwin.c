// Copyright 2010 The Go Authors.  All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "runtime.h"
#include "arch_GOARCH.h"
#include "defs_GOOS_GOARCH.h"
#include "os_GOOS.h"
#include "malloc.h"

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
	// Linux's MADV_DONTNEED is like BSD's MADV_FREE.
	runtime_madvise(v, n, MADV_FREE);
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
	return runtime_mmap(v, n, PROT_NONE, MAP_ANON|MAP_PRIVATE, -1, 0);
}

enum
{
	ENOMEM = 12,
};

void
runtime_SysMap(void *v, uintptr n)
{
	void *p;
	
	mstats.sys += n;
	p = runtime_mmap(v, n, PROT_READ|PROT_WRITE|PROT_EXEC, MAP_ANON|MAP_FIXED|MAP_PRIVATE, -1, 0);
	if(p == (void*)-ENOMEM)
		runtime_throw("runtime: out of memory");
	if(p != v)
		runtime_throw("runtime: cannot map pages in arena address space");
}
