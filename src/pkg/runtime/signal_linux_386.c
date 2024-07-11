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
	uintptr *sp;
	SigTab *t;

	uc = context;
	r = &uc->uc_mcontext;

	if(sig == SIGPROF) {
		runtime_sigprof((uint8*)r->eip, (uint8*)r->esp, nil, gp);
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
		gp->sigcode1 = ((uintptr*)info)[3];
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

	if(sig < 0 || sig >= NSIG)
		runtime_printf("Signal %d\n", sig);
	else
		runtime_printf("%s\n", runtime_sigtab[sig].name);

	runtime_printf("PC=%X\n", r->eip);
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
	sa.k_sa_handler = fn;
	if(runtime_rt_sigaction(i, &sa, nil, sizeof(sa.sa_mask)) != 0)
		runtime_throw("rt_sigaction failure");
}

#define AT_NULL		0
#define AT_SYSINFO	32
extern uint32 runtime__vdso;

#pragma textflag 7
void
runtime_linux_setup_vdso(int32 argc, void *argv_list)
{
	byte **argv = &argv_list;
	byte **envp;
	uint32 *auxv;

	// skip envp to get to ELF auxiliary vector.
	for(envp = &argv[argc+1]; *envp != nil; envp++)
		;
	envp++;
	
	for(auxv=(uint32*)envp; auxv[0] != AT_NULL; auxv += 2) {
		if(auxv[0] == AT_SYSINFO) {
			runtime__vdso = auxv[1];
			break;
		}
	}
}
