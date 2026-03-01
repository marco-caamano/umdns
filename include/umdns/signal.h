#ifndef UMDNS_SIGNAL_H
#define UMDNS_SIGNAL_H

#include <stdbool.h>

int umdns_signal_install_handlers(void);
bool umdns_signal_should_terminate(void);
void umdns_signal_request_terminate(void);

#endif
