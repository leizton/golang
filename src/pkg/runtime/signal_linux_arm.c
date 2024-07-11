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
	runtime_printf("trap    %x\n", r->trap_no);
	runtime_printf("error   %x\n", r->error_code);
	runtime_printf("oldmask %x\n", r->oldmask);
	runtime_printf("r0      %x\n", r->arm_r0);
	runtime_printf("r1      %x\n", r->arm_r1);
	runtime_printf("r2      %x\n", r->arm_r2);
	runtime_printf("r3      %x\n", r->arm_r3);
	runtime_printf("r4      %x\n", r->arm_r4);
	runtime_printf("r5      %x\n", r->arm_r5);
	runtime_printf("r6      %x\n", r->arm_r6);
	runtime_printf("r7      %x\n", r->arm_r7);
	runtime_printf("r8      %x\n", r->arm_r8);
	runtime_printf("r9      %x\n", r->arm_r9);
	runtime_printf("r10     %x\n", r->arm_r10);
	runtime_printf("fp      %x\n", r->arm_fp);
	runtime_printf("ip      %x\n", r->arm_ip);
	runtime_printf("sp      %x\n", r->arm_sp);
	runtime_printf("lr      %x\n", r->arm_lr);
	runtime_printf("pc      %x\n", r->arm_pc);
	runtime_printf("cpsr    %x\n", r->arm_cpsr);
	runtime_printf("fault   %x\n", r->fault_address);
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
	Sigcontext *r;
	SigTab *t;

	uc = context;
	r = &uc->uc_mcontext;

	if(sig == SIGPROF) {
		runtime_sigprof((uint8*)r->arm_pc, (uint8*)r->arm_sp, (uint8*)r->arm_lr, gp);
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
		gp->sigcode1 = r->fault_address;
		gp->sigpc = r->arm_pc;

		// If this is a leaf function, we do smash LR,
		// but we're not going back there anyway.
		// Don't bother smashing if r->arm_pc is 0,
		// which is probably a call to a nil func: the
		// old link register is more useful in the stack trace.
		if(r->arm_pc != 0)
			r->arm_lr = r->arm_pc;
		r->arm_pc = (uintptr)runtime_sigpanic;
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
	if(runtime_panicking)	// traceback already printed
		runtime_exit(2);
	runtime_panicking = 1;

	if(sig < 0 || sig >= NSIG)
		runtime_printf("Signal %d\n", sig);
	else
		runtime_printf("%s\n", runtime_sigtab[sig].name);

	runtime_printf("PC=%x\n", r->arm_pc);
	runtime_printf("\n");

	if(runtime_gotraceback()){
		runtime_traceback((void*)r->arm_pc, (void*)r->arm_sp, (void*)r->arm_lr, gp);
		runtime_tracebackothers(gp);
		runtime_printf("\n");
		runtime_dumpregs(r);
	}

//	breakpoint();
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
