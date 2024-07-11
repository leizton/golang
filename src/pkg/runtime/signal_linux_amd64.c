// Copyright 2009 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "runtime.h"
#include "defs_GOOS_GOARCH.h"
#include "signals_GOOS.h"
#include "os_GOOS.h"

void
runtime_dumpregs(Sigcontext *r)
{
	runtime_printf("rax     %X\n", r->rax);
	runtime_printf("rbx     %X\n", r->rbx);
	runtime_printf("rcx     %X\n", r->rcx);
	runtime_printf("rdx     %X\n", r->rdx);
	runtime_printf("rdi     %X\n", r->rdi);
	runtime_printf("rsi     %X\n", r->rsi);
	runtime_printf("rbp     %X\n", r->rbp);
	runtime_printf("rsp     %X\n", r->rsp);
	runtime_printf("r8      %X\n", r->r8 );
	runtime_printf("r9      %X\n", r->r9 );
	runtime_printf("r10     %X\n", r->r10);
	runtime_printf("r11     %X\n", r->r11);
	runtime_printf("r12     %X\n", r->r12);
	runtime_printf("r13     %X\n", r->r13);
	runtime_printf("r14     %X\n", r->r14);
	runtime_printf("r15     %X\n", r->r15);
	runtime_printf("rip     %X\n", r->rip);
	runtime_printf("rflags  %X\n", r->eflags);
	runtime_printf("cs      %X\n", (uint64)r->cs);
	runtime_printf("fs      %X\n", (uint64)r->fs);
	runtime_printf("gs      %X\n", (uint64)r->gs);
}

/*
 * This assembler routine takes the args from registers, puts them on the stack,
 * and calls sighandler().
 */
extern void runtime_sigtramp(void);
extern void runtime_sigreturn(void);	// calls runtime_sigreturn

void
runtime_sighandler(int32 sig, Siginfo *info, void *context, G *gp)
{
	Ucontext *uc;
	Mcontext *mc;
	Sigcontext *r;
	uintptr *sp;
	SigTab *t;

	uc = context;
	mc = &uc->uc_mcontext;
	r = (Sigcontext*)mc;	// same layout, more conveient names

	if(sig == SIGPROF) {
		runtime_sigprof((uint8*)r->rip, (uint8*)r->rsp, nil, gp);
		return;
	}

	t = &runtime_sigtab[sig];
	if(info->si_code != SI_USER && (t->flags & SigPanic)) {
		if(gp == nil)
			goto Throw;
		// Make it look like a call to the signal func.
		// Have to pass arguments out of band since
		// augmenting the stack frame would break
		// the unwinding code.
		gp->sig = sig;
		gp->sigcode0 = info->si_code;
		gp->sigcode1 = ((uintptr*)info)[2];
		gp->sigpc = r->rip;

		// Only push runtime_sigpanic if r->rip != 0.
		// If r->rip == 0, probably panicked because of a
		// call to a nil func.  Not pushing that onto sp will
		// make the trace look like a call to runtime_sigpanic instead.
		// (Otherwise the trace will end at runtime_sigpanic and we
		// won't get to see who faulted.)
		if(r->rip != 0) {
			sp = (uintptr*)r->rsp;
			*--sp = r->rip;
			r->rsp = (uintptr)sp;
		}
		r->rip = (uintptr)runtime_sigpanic;
		return;
	}

	if(info->si_code == SI_USER || (t->flags & SigNotify))
		if(runtime_sigsend(sig))
			return;
	if(t->flags & SigKill)
		runtime_exit(2);
	if(!(t->flags & SigThrow))
		return;

Throw:
	runtime_startpanic();

	if(sig < 0 || sig >= NSIG)
		runtime_printf("Signal %d\n", sig);
	else
		runtime_printf("%s\n", runtime_sigtab[sig].name);

	runtime_printf("PC=%X\n", r->rip);
	runtime_printf("\n");

	if(runtime_gotraceback()){
		runtime_traceback((void*)r->rip, (void*)r->rsp, 0, gp);
		runtime_tracebackothers(gp);
		runtime_dumpregs(r);
	}

	runtime_exit(2);
}

void
runtime_signalstack(byte *p, int32 n)
{
	Sigaltstack st;

	st.ss_sp = p;
	st.ss_size = n;
	st.ss_flags = 0;
	runtime_sigaltstack(&st, nil);
}

void
runtime_setsig(int32 i, void (*fn)(int32, Siginfo*, void*, G*), bool restart)
{
	Sigaction sa;

	runtime_memclr((byte*)&sa, sizeof sa);
	sa.sa_flags = SA_ONSTACK | SA_SIGINFO | SA_RESTORER;
	if(restart)
		sa.sa_flags |= SA_RESTART;
	sa.sa_mask = ~0ULL;
	sa.sa_restorer = (void*)runtime_sigreturn;
	if(fn == runtime_sighandler)
		fn = (void*)runtime_sigtramp;
	sa.sa_handler = fn;
	if(runtime_rt_sigaction(i, &sa, nil, sizeof(sa.sa_mask)) != 0)
		runtime_throw("rt_sigaction failure");
}
