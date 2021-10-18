void unix_error(char *msg);
void app_error(char *msg);
typedef void handler_t(int);

handler_t *Signal(int signum, handler_t *handler);  //sigaction function wrapper
//You'll need to add prototypes for the other wrappers you write
