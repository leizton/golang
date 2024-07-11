// Copyright 2009 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "runtime.h"
#include "defs_GOOS_GOARCH.h"
#include "os_GOOS.h"

void
runtime_dumpregs(Context *r)
{
	runtime_printf("eax     %x\n", r->Eax);
	runtime_printf("ebx     %x\n", r->Ebx);
	runtime_printf("ecx     %x\n", r->Ecx);
	runtime_printf("edx     %x\n", r->Edx);
	runtime_printf("edi     %x\n", r->Edi);
	runtime_printf("esi     %x\n", r->Esi);
	runtime_printf("ebp     %x\n", r->Ebp);
	runtime_printf("esp     %x\n", r->Esp);
	runtime_printf("eip     %x\n", r->Eip);
	runtime_printf("eflags  %x\n", r->EFlags);
	runtime_printf("cs      %x\n", r->SegCs);
	runtime_printf("fs      %x\n", r->SegFs);
	runtime_printf("gs      %x\n", r->SegGs);
}

uint32
runtime_sighandler(ExceptionRecord *info, Context *r, G *gp)
{
	uintptr *sp;

	switch(info->ExceptionCode) {
	case EXCEPTION_BREAKPOINT:
		r->Eip--;	// because 8l generates 2 bytes for INT3
		return 1;
	}

	if(gp != nil && runtime_issigpanic(info->ExceptionCode)) {
		// Make it look like a call to the signal func.
		// Have to pass arguments out of band since
		// augmenting the stack frame would break
		// the unwinding code.
		gp->sig = info->ExceptionCode;
		gp->sigcode0 = info->ExceptionInformation[0];
		gp->sigcode1 = info->ExceptionInformation[1];
		gp->sigpc = r->Eip;

		// Only push runtime_sigpanic if r->eip != 0.
		// If r->eip == 0, probably panicked because of a
		// call to a nil func.  Not pushing that onto sp will
		// make the trace look like a call to runtime_sigpanic instead.
		// (Otherwise the trace will end at runtime_sigpanic and we
		// won't get to see who faulted.)
		if(r->Eip != 0) {
			sp = (uintptr*)r->Esp;
			*--sp = r->Eip;
			r->Esp = (uintptr)sp;
		}
		r->Eip = (uintptr)runtime_sigpanic;
		return 0;
	}

	if(runtime_panicking)	// traceback already printed
		runtime_exit(2);
	runtime_panicking = 1;

	runtime_printf("Exception %x %p %p\n", info->ExceptionCode,
		info->ExceptionInformation[0], info->ExceptionInformation[1]);

	runtime_printf("PC=%x\n", r->Eip);
	runtime_printf("\n");

	if(runtime_gotraceback()){
		runtime_traceback((void*)r->Eip, (void*)r->Esp, 0, m->curg);
		runtime_tracebackothers(m->curg);
		runtime_dumpregs(r);
	}

	runtime_exit(2);
	return 0;
}

void
runtime_sigenable(uint32 sig)
{
	USED(sig);
}

void
runtime_dosigprof(Context *r, G *gp)
{
	runtime_sigprof((uint8*)r->Eip, (uint8*)r->Esp, nil, gp);
}
