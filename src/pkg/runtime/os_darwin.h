// Copyright 2009 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#define SIG_DFL ((void*)0)
#define SIG_IGN ((void*)1)

int32	runtime_bsdthread_create(void*, M*, G*, void(*)(void));
void	runtime_bsdthread_register(void);
int32	runtime_mach_msg_trap(MachHeader*, int32, uint32, uint32, uint32, uint32, uint32);
uint32	runtime_mach_reply_port(void);
int32	runtime_mach_semacquire(uint32, int64);
uint32	runtime_mach_semcreate(void);
void	runtime_mach_semdestroy(uint32);
void	runtime_mach_semrelease(uint32);
void	runtime_mach_semreset(uint32);
uint32	runtime_mach_task_self(void);
uint32	runtime_mach_task_self(void);
uint32	runtime_mach_thread_self(void);
uint32	runtime_mach_thread_self(void);
int32	runtime_sysctl(uint32*, uint32, byte*, uintptr*, byte*, uintptr);

typedef uint32 Sigset;
void	runtime_sigprocmask(int32, Sigset*, Sigset*);

struct Sigaction;
void	runtime_sigaction(uintptr, struct Sigaction*, struct Sigaction*);
void	runtime_setsig(int32, void(*)(int32, Siginfo*, void*, G*), bool);
void	runtime_sighandler(int32 sig, Siginfo *info, void *context, G *gp);

struct StackT;
void	runtime_sigaltstack(struct StackT*, struct StackT*);
void	runtime_sigtramp(void);
void	runtime_sigpanic(void);
void	runtime_setitimer(int32, Itimerval*, Itimerval*);

void	runtime_raisesigpipe(void);

#define	NSIG 32
#define	SI_USER	0  /* empirically true, but not what headers say */
#define	SIG_BLOCK 1
#define	SIG_UNBLOCK 2
#define	SIG_SETMASK 3

