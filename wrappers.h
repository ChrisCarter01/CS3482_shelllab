void unix_error(char *msg);
void app_error(char *msg);
typedef void handler_t(int);

handler_t *Signal(int signum, handler_t *handler);  //sigaction function wrapper
//You'll need to add prototypes for the other wrappers you write

int Fork();
void Exec(char *cmdline, char** argv);
void Sigprocmask(int how, const sigset_t *set, sigset_t *oldset);
void SigFillSet(sigset_t* set);
void SigEmptySet(sigset_t* set);
void SigAddSet(sigset_t* set, int signal);
void Kill(pid_t pid, int signal);
void Killpg(pid_t pid, int signal);