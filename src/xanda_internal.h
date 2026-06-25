#ifndef XANDA_INTERNAL_H
#define XANDA_INTERNAL_H

#include "xanda/xanda.h"

char *xanda_strdup(const char *value);
XandaStatus xanda_set_errorf(XandaStatus status, const char *fmt, ...);
void xanda_backend_ensure_default(void);
const XandaBackendOps *xanda_backend_get(void);
void xanda_component_set_root(XandaComponent *component);
XandaComponent *xanda_component_get_root(void);
void xanda_component_register(XandaComponent *component);
void xanda_component_unregister(XandaComponent *component);
void xanda_component_update_boundary_key(XandaComponent *component, const char *root_key);
const char *xanda_component_boundary_key(const XandaComponent *component);
XandaComponent *xanda_component_registry_first(void);
XandaComponent *xanda_component_registry_next(const XandaComponent *component);
unsigned int xanda_component_state_version_internal(const XandaComponent *component);
size_t xanda_component_state_size_internal(const XandaComponent *component);

#endif /* XANDA_INTERNAL_H */
