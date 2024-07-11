// Copyright 2011 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "runtime.h"
#include "defs_GOOS_GOARCH.h"
#include "os_GOOS.h"

void
runtime_dumpregs(Context *r)
{
	runtime_printf("rax     %X\n", r->Rax);
	runtime_printf("rbx     %X\n", r->Rbx);
	runtime_printf("rcx     %X\n", r->Rcx);
	runtime_printf("rdx     %X\n", r->Rdx);
	runtime_printf("rdi     %X\n", r->Rdi);
	runtime_printf("rsi     %X\n", r->Rsi);
	runtime_printf("rbp     %X\n", r->Rbp);
	runtime_printf("rsp     %X\n", r->Rsp);
	runtime_printf("r8      %X\n", r->R8 );
	runtime_printf("r9      %X\n", r->R9 );
	runtime_printf("r10     %X\n", r->R10);
	runtime_printf("r11     %X\n", r->R11);
	runtime_printf("r12     %X\n", r->R12);
	runtime_printf("r13     %X\n", r->R13);
	runtime_printf("r14     %X\n", r->R14);
	runtime_printf("r15     %X\n", r->R15);
	runtime_printf("rip     %X\n", r->Rip);
	runtime_printf("rflags  %X\n", r->EFlags);
	runtime_printf("cs      %X\n", (uint64)r->SegCs);
	runtime_printf("fs      %X\n", (uint64)r->SegFs);
	runtime_printf("gs      %X\n", (uint64)r->SegGs);
}

uint32
runtime_sighandler(ExceptionRecord *info, Context *r, G *gp)
{
	uintptr *sp;

	switch(info->ExceptionCode) {
	case EXCEPTION_BREAKPOINT:
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
		gp->sigpc = r->Rip;

		// Only push runtime_sigpanic if r->rip != 0.
		// If r->rip == 0, probably panicked because of a
		// call to a nil func.  Not pushing that onto sp will
		// make the trace look like a call to runtime_sigpanic instead.
		// (Otherwise the trace will end at runtime_sigpanic and we
		// won't get to see who faulted.)
		if(r->Rip != 0) {
			sp = (uintptr*)r->Rsp;
			*--sp = r->Rip;
			r->Rsp = (uintptr)sp;
		}
		r->Rip = (uintptr)runtime_sigpanic;
		return 0;
	}

	if(runtime_panicking)	// traceback already printed
		runtime_exit(2);
	runtime_panicking = 1;

	runtime_printf("Exception %x %p %p\n", info->ExceptionCode,
		info->ExceptionInformation[0], info->ExceptionInformation[1]);

	runtime_printf("PC=%X\n", r->Rip);
	runtime_printf("\n");

	if(runtime_gotraceback()){
		runtime_traceback((void*)r->Rip, (void*)r->Rsp, 0, gp);
		runtime_tracebackothers(gp);
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
	runtime_sigprof((uint8*)r->Rip, (uint8*)r->Rsp, nil, gp);
}
