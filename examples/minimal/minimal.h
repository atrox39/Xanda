#ifndef XANDA_MINIMAL_H
#define XANDA_MINIMAL_H

#include "xanda/xanda.h"

XandaStatus minimal_feature_init(XandaApp *app);
XandaStatus minimal_feature_restore_pending(void);
XandaStatus minimal_feature_mount(void);

#endif /* XANDA_MINIMAL_H */
