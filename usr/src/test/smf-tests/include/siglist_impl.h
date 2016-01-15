#include <signal.h>

#if !defined(SIGLIST_IMPL_H)
#define SIGLIST_IMPL_H

#ifdef __cplusplus
extern "C" {
#endif

const struct sigelt {
	const int signum;
	const char *text;
} sigarray[] = {
 { SIGHUP, "SIGHUP" },
 { SIGINT, "SIGINT" },
 { SIGQUIT, "SIGQUIT" },
 { SIGILL, "SIGILL" },
 { SIGTRAP, "SIGTRAP" },
 { SIGIOT, "SIGIOT" },
 { SIGABRT, "SIGABRT" },
 { SIGEMT, "SIGEMT" },
 { SIGFPE, "SIGFPE" },
 { SIGKILL, "SIGKILL" },
 { SIGBUS, "SIGBUS" },
 { SIGSEGV, "SIGSEGV" },
 { SIGSYS, "SIGSYS" },
 { SIGPIPE, "SIGPIPE" },
 { SIGALRM, "SIGALRM" },
 { SIGTERM, "SIGTERM" },
 { SIGUSR1, "SIGUSR1" },
 { SIGUSR2, "SIGUSR2" },
 { SIGCLD, "SIGCLD" },
 { SIGCHLD, "SIGCHLD" },
 { SIGPWR, "SIGPWR" },
 { SIGWINCH, "SIGWINCH" },
 { SIGURG, "SIGURG" },
 { SIGPOLL, "SIGPOLL" },
 { SIGIO, "SIGIO" },
 { SIGSTOP, "SIGSTOP" },
 { SIGTSTP, "SIGTSTP" },
 { SIGCONT, "SIGCONT" },
 { SIGTTIN, "SIGTTIN" },
 { SIGTTOU, "SIGTTOU" },
 { SIGVTALRM, "SIGVTALRM" },
 { SIGPROF, "SIGPROF" },
 { SIGXCPU, "SIGXCPU" },
 { SIGXFSZ, "SIGXFSZ" },
 { SIGWAITING, "SIGWAITING" },
 { SIGLWP, "SIGLWP" },
 { SIGFREEZE, "SIGFREEZE" },
 { SIGTHAW, "SIGTHAW" },
 { SIGCANCEL, "SIGCANCEL" },
 { SIGLOST, "SIGLOST" },
 { SIGXRES, "SIGXRES" },
 { SIGJVM1, "SIGJVM1" },
 { SIGJVM2, "SIGJVM2" },
 { -1, NULL }
};

#ifdef __cplusplus
}
#endif

#endif /* SIGLIST_IMPL_H */
