// Copyright 2009 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "runtime.h"
#include "defs_GOOS_GOARCH.h"
#include "os_GOOS.h"
#include "signals_GOOS.h"

void
runtime_dumpregs(Regs32 *r)
{
	runtime_printf("eax     %x\n", r->eax);
	runtime_printf("ebx     %x\n", r->ebx);
	runtime_printf("ecx     %x\n", r->ecx);
	runtime_printf("edx     %x\n", r->edx);
	runtime_printf("edi     %x\n", r->edi);
	runtime_printf("esi     %x\n", r->esi);
	runtime_printf("ebp     %x\n", r->ebp);
	runtime_printf("esp     %x\n", r->esp);
	runtime_printf("eip     %x\n", r->eip);
	runtime_printf("eflags  %x\n", r->eflags);
	runtime_printf("cs      %x\n", r->cs);
	runtime_printf("fs      %x\n", r->fs);
	runtime_printf("gs      %x\n", r->gs);
}

void
runtime_sighandler(int32 sig, Siginfo *info, void *context, G *gp)
{
	Ucontext *uc;
	Mcontext32 *mc;
	Regs32 *r;
	uintptr *sp;
	byte *pc;
	SigTab *t;

	uc = context;
	mc = uc->uc_mcontext;
	r = &mc->ss;

	if(sig == SIGPROF) {
		if(gp != m->g0 && gp != m->gsignal)
			runtime_sigprof((uint8*)r->eip, (uint8*)r->esp, nil, gp);
		return;
	}

	t = &runtime_sigtab[sig];
	if(info->si_code != SI_USER && (t->flags & SigPanic)) {
		if(gp == nil)
			goto Throw;
		// Work around Leopard bug that doesn't set FPE_INTDIV.
		// Look at instruction to see if it is a divide.
		// Not necessary in Snow Leopard (si_code will be != 0).
		if(sig == SIGFPE && info->si_code == 0) {
			pc = (byte*)r->eip;
			if(pc[0] == 0x66)	// 16-bit instruction prefix
				pc++;
			if(pc[0] == 0xF6 || pc[0] == 0xF7)
				info->si_code = FPE_INTDIV;
		}

		// Make it look like a call to the signal func.
		// Have to pass arguments out of band since
		// augmenting the stack frame would break
		// the unwinding code.
		gp->sig = sig;
		gp->sigcode0 = info->si_code;
		gp->sigcode1 = (uintptr)info->si_addr;
		gp->sigpc = r->eip;

		// Only push runtime_sigpanic if r->eip != 0.
		// If r->eip == 0, probably panicked because of a
		// call to a nil func.  Not pushing that onto sp will
		// make the trace look like a call to runtime_sigpanic instead.
		// (Otherwise the trace will end at runtime_sigpanic and we
		// won't get to see who faulted.)
		if(r->eip != 0) {
			sp = (uintptr*)r->esp;
			*--sp = r->eip;
			r->esp = (uintptr)sp;
		}
		r->eip = (uintptr)runtime_sigpanic;
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

	if(sig < 0 || sig >= NSIG){
		runtime_printf("Signal %d\n", sig);
	}else{
		runtime_printf("%s\n", runtime_sigtab[sig].name);
	}

	runtime_printf("pc: %x\n", r->eip);
	runtime_printf("\n");

	if(runtime_gotraceback()){
		runtime_traceback((void*)r->eip, (void*)r->esp, 0, gp);
		runtime_tracebackothers(gp);
		runtime_dumpregs(r);
	}

	runtime_exit(2);
}

void
runtime_signalstack(byte *p, int32 n)
{
	StackT st;

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
	sa.sa_flags = SA_SIGINFO|SA_ONSTACK;
	if(restart)
		sa.sa_flags |= SA_RESTART;
	sa.sa_mask = ~0U;
	sa.sa_tramp = (void*)runtime_sigtramp;	// runtime_sigtramp's job is to call into real handler
	*(uintptr*)sa.__sigaction_u = (uintptr)fn;
	runtime_sigaction(i, &sa, nil);
}
