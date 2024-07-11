// Copyright 2009 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "runtime.h"

// Atomic add and return new value.
#pragma textflag 7
uint32
runtime_xadd(uint32 volatile *val, int32 delta)
{
	uint32 oval, nval;

	for(;;){
		oval = *val;
		nval = oval + delta;
		if(runtime_cas(val, oval, nval))
			return nval;
	}
}

#pragma textflag 7
uint32
runtime_xchg(uint32 volatile* addr, uint32 v)
{
	uint32 old;

	for(;;) {
		old = *addr;
		if(runtime_cas(addr, old, v))
			return old;
	}
}

#pragma textflag 7
void
runtime_procyield(uint32 cnt)
{
	uint32 volatile i;

	for(i = 0; i < cnt; i++) {
	}
}

#pragma textflag 7
uint32
runtime_atomicload(uint32 volatile* addr)
{
	return runtime_xadd(addr, 0);
}

#pragma textflag 7
void*
runtime_atomicloadp(void* volatile* addr)
{
	return (void*)runtime_xadd((uint32 volatile*)addr, 0);
}

#pragma textflag 7
void
runtime_atomicstorep(void* volatile* addr, void* v)
{
	void *old;

	for(;;) {
		old = *addr;
		if(runtime_casp(addr, old, v))
			return;
	}
}

#pragma textflag 7
void
runtime_atomicstore(uint32 volatile* addr, uint32 v)
{
	uint32 old;
	
	for(;;) {
		old = *addr;
		if(runtime_cas(addr, old, v))
			return;
	}
}