#ifndef XANDA_COUNTER_H
#define XANDA_COUNTER_H

#include "xanda/xanda.h"

XandaStatus counter_feature_init(XandaApp *app);
XandaStatus counter_feature_restore_pending(void);
XandaStatus counter_feature_mount(void);
void increment_and_render(void);

#endif /* XANDA_COUNTER_H */
