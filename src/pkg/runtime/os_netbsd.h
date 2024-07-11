// Copyright 2010 The Go Authors.  All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#define SIG_DFL ((void*)0)
#define SIG_IGN ((void*)1)

struct sigaction;

void	runtime_sigpanic(void);
void	runtime_sigaltstack(Sigaltstack*, Sigaltstack*);
void	runtime_sigaction(int32, struct sigaction*, struct sigaction*);
void	runtime_setsig(int32, void(*)(int32, Siginfo*, void*, G*), bool);
void	runtime_sighandler(int32 sig, Siginfo *info, void *context, G *gp);
void	runtime_setitimer(int32, Itimerval*, Itimerval*);
int32	runtime_sysctl(uint32*, uint32, byte*, uintptr*, byte*, uintptr);

void	runtime_raisesigpipe(void);

#define	NSIG 33
#define	SI_USER	0
