// Copyright 2010 The Go Authors.  All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "runtime.h"
#include "arch_GOARCH.h"
#include "os_GOOS.h"
#include "defs_GOOS_GOARCH.h"
#include "malloc.h"

enum {
	MEM_COMMIT = 0x1000,
	MEM_RESERVE = 0x2000,
	MEM_RELEASE = 0x8000,
	
	PAGE_EXECUTE_READWRITE = 0x40,
};

#pragma dynimport runtime_VirtualAlloc VirtualAlloc "kernel32.dll"
#pragma dynimport runtime_VirtualFree VirtualFree "kernel32.dll"
extern void *runtime_VirtualAlloc;
extern void *runtime_VirtualFree;

void*
runtime_SysAlloc(uintptr n)
{
	mstats.sys += n;
	return runtime_stdcall(runtime_VirtualAlloc, 4, nil, n, (uintptr)(MEM_COMMIT|MEM_RESERVE), (uintptr)PAGE_EXECUTE_READWRITE);
}

void
runtime_SysUnused(void *v, uintptr n)
{
	USED(v);
	USED(n);
}

void
runtime_SysFree(void *v, uintptr n)
{
	uintptr r;

	mstats.sys -= n;
	r = (uintptr)runtime_stdcall(runtime_VirtualFree, 3, v, (uintptr)0, (uintptr)MEM_RELEASE);
	if(r == 0)
		runtime_throw("runtime: failed to release pages");
}

void*
runtime_SysReserve(void *v, uintptr n)
{
	// v is just a hint.
	// First try at v.
	v = runtime_stdcall(runtime_VirtualAlloc, 4, v, n, (uintptr)MEM_RESERVE, (uintptr)PAGE_EXECUTE_READWRITE);
	if(v != nil)
		return v;
	
	// Next let the kernel choose the address.
	return runtime_stdcall(runtime_VirtualAlloc, 4, nil, n, (uintptr)MEM_RESERVE, (uintptr)PAGE_EXECUTE_READWRITE);
}

void
runtime_SysMap(void *v, uintptr n)
{
	void *p;
	
	mstats.sys += n;
	p = runtime_stdcall(runtime_VirtualAlloc, 4, v, n, (uintptr)MEM_COMMIT, (uintptr)PAGE_EXECUTE_READWRITE);
	if(p != v)
		runtime_throw("runtime: cannot map pages in arena address space");
}
