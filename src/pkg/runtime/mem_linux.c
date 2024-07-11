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
	_PAGE_SIZE = 4096,
};

static int32
addrspace_free(void *v, uintptr n)
{
	int32 errval;
	uintptr chunk;
	uintptr off;
	static byte vec[4096];

	for(off = 0; off < n; off += chunk) {
		chunk = _PAGE_SIZE * sizeof vec;
		if(chunk > (n - off))
			chunk = n - off;
		errval = runtime_mincore((int8*)v + off, chunk, vec);
		// errval is 0 if success, or -(error_code) if error.
		if (errval == 0 || errval != -ENOMEM)
			return 0;
	}
	return 1;
}

static void *
mmap_fixed(byte *v, uintptr n, int32 prot, int32 flags, int32 fd, uint32 offset)
{
	void *p;

	p = runtime_mmap(v, n, prot, flags, fd, offset);
	if(p != v && addrspace_free(v, n)) {
		// On some systems, mmap ignores v without
		// MAP_FIXED, so retry if the address space is free.
		if(p > (void*)4096)
			runtime_munmap(p, n);
		p = runtime_mmap(v, n, prot, flags|MAP_FIXED, fd, offset);
	}
	return p;
}

void*
runtime_SysAlloc(uintptr n)
{
	void *p;

	mstats.sys += n;
	p = runtime_mmap(nil, n, PROT_READ|PROT_WRITE|PROT_EXEC, MAP_ANON|MAP_PRIVATE, -1, 0);
	if(p < (void*)4096) {
		if(p == (void*)EACCES) {
			runtime_printf("runtime: mmap: access denied\n");
			runtime_printf("if you're running SELinux, enable execmem for this process.\n");
			runtime_exit(2);
		}
		return nil;
	}
	return p;
}

void
runtime_SysUnused(void *v, uintptr n)
{
	runtime_madvise(v, n, MADV_DONTNEED);
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
	// if we can reserve at least 64K and check the assumption in SysMap.
	// Only user-mode Linux (UML) rejects these requests.
	if(sizeof(void*) == 8 && (uintptr)v >= 0xffffffffU) {
		p = mmap_fixed(v, 64<<10, PROT_NONE, MAP_ANON|MAP_PRIVATE, -1, 0);
		if (p != v)
			return nil;
		runtime_munmap(p, 64<<10);
		return v;
	}
	
	p = runtime_mmap(v, n, PROT_NONE, MAP_ANON|MAP_PRIVATE, -1, 0);
	if((uintptr)p < 4096 || -(uintptr)p < 4096)
		return nil;
	return p;
}

void
runtime_SysMap(void *v, uintptr n)
{
	void *p;
	
	mstats.sys += n;

	// On 64-bit, we don't actually have v reserved, so tread carefully.
	if(sizeof(void*) == 8 && (uintptr)v >= 0xffffffffU) {
		p = mmap_fixed(v, n, PROT_READ|PROT_WRITE|PROT_EXEC, MAP_ANON|MAP_PRIVATE, -1, 0);
		if(p == (void*)ENOMEM)
			runtime_throw("runtime: out of memory");
		if(p != v) {
			runtime_printf("runtime: address space conflict: map(%p) = %p\n", v, p);
			runtime_throw("runtime: address space conflict");
		}
		return;
	}

	p = runtime_mmap(v, n, PROT_READ|PROT_WRITE|PROT_EXEC, MAP_ANON|MAP_FIXED|MAP_PRIVATE, -1, 0);
	if(p == (void*)ENOMEM)
		runtime_throw("runtime: out of memory");
	if(p != v)
		runtime_throw("runtime: cannot map pages in arena address space");
}
