#include "umdns/signal.h"

#include <signal.h>

static volatile sig_atomic_t g_terminate = 0;

static void umdns_signal_handler(int signo) {
    (void)signo;
    g_terminate = 1;
}

int umdns_signal_install_handlers(void) {
    struct sigaction action;

    action.sa_handler = umdns_signal_handler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;

    if (sigaction(SIGINT, &action, NULL) != 0) {
        return -1;
    }
    if (sigaction(SIGTERM, &action, NULL) != 0) {
        return -1;
    }

    return 0;
}

bool umdns_signal_should_terminate(void) {
    return g_terminate != 0;
}

void umdns_signal_request_terminate(void) {
    g_terminate = 1;
}
