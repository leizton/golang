#define SIG_DFL ((void*)0)
#define SIG_IGN ((void*)1)

int32	runtime_thr_new(ThrParam*, int32);
void	runtime_sighandler(int32 sig, Siginfo *info, void *context, G *gp);
void	runtime_sigpanic(void);
void	runtime_sigaltstack(Sigaltstack*, Sigaltstack*);
struct	sigaction;
void	runtime_sigaction(int32, struct sigaction*, struct sigaction*);
void	runtime_sigprocmask(Sigset *, Sigset *);
void	runtime_setsig(int32, void(*)(int32, Siginfo*, void*, G*), bool);
void	runtime_setitimer(int32, Itimerval*, Itimerval*);
int32	runtime_sysctl(uint32*, uint32, byte*, uintptr*, byte*, uintptr);

void	runtime_raisesigpipe(void);

#define	NSIG 33
#define	SI_USER	0x10001

#define RLIMIT_AS 10
typedef struct Rlimit Rlimit;
struct Rlimit {
	int64	rlim_cur;
	int64	rlim_max;
};
int32	runtime_getrlimit(int32, Rlimit*);
