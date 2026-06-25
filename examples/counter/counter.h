#ifndef XANDA_COUNTER_H
#define XANDA_COUNTER_H

#include "xanda/xanda.h"

XandaStatus counter_init(XandaApp *app);
XandaStatus counter_restore_pending(void);
XandaStatus counter_mount(void);
void increment_and_render(void);

#endif /* XANDA_COUNTER_H */
