#ifndef XANDA_H
#define XANDA_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define XANDA_VERSION_MAJOR 1
#define XANDA_VERSION_MINOR 0
#define XANDA_VERSION_PATCH 0
#define XANDA_VERSION_STRING "1.0.0"
#define XANDA_DEV_PROTOCOL_VERSION 1u

typedef enum
{
  XANDA_STATUS_OK = 0,
  XANDA_STATUS_INVALID_ARGUMENT,
  XANDA_STATUS_OUT_OF_MEMORY,
  XANDA_STATUS_BACKEND_UNAVAILABLE,
  XANDA_STATUS_RENDER_FAILED,
  XANDA_STATUS_TARGET_NOT_FOUND,
  XANDA_STATUS_EVENT_BIND_FAILED
} XandaStatus;

typedef void (*XandaErrorHandler)(XandaStatus status, const char *message, void *user_data);

typedef struct
{
  const char *key;
  const char *value;
} XandaAttribute;

typedef enum
{
  XANDA_ELEMENT,
  XANDA_TEXT
} XandaNodeType;

typedef struct XandaComponent XandaComponent;

typedef struct XandaNode
{
  XandaNodeType type;
  char *tag;
  char *text;
  char **attr_keys;
  char **attr_values;
  int attr_count;
  int attr_capacity;
  struct XandaNode **children;
  int child_count;
  int child_capacity;
  int dom_id;
  char *key;
  unsigned int owner_component_id;
} XandaNode;

typedef struct
{
  const char *mount_target_id;
  int replace_target_children;
} XandaConfig;

typedef struct
{
  XandaConfig config;
  int is_initialized;
} XandaApp;

typedef struct
{
  int (*create_element)(const char *tag);
  int (*create_text)(const char *text);
  XandaStatus (*set_attr)(int id, const char *key, const char *value);
  XandaStatus (*append_child)(int parent_id, int child_id);
  XandaStatus (*mount)(int root_id, const char *target_id, int replace_target_children);
  XandaStatus (*set_event)(int id, const char *event_name, const char *c_func_name);
} XandaBackendOps;

typedef struct
{
  void *data;
  size_t size;
  unsigned int version;
} XandaStateSnapshot;

typedef XandaStatus (*XandaComponentSetupCallback)(XandaComponent *component);
typedef XandaNode *(*XandaComponentRenderCallback)(XandaComponent *component);
typedef XandaStatus (*XandaComponentBindCallback)(XandaComponent *component, XandaNode *root);
typedef XandaStatus (*XandaComponentSnapshotCallback)(const XandaComponent *component, XandaStateSnapshot *snapshot);
typedef XandaStatus (*XandaComponentRestoreCallback)(XandaComponent *component, const XandaStateSnapshot *snapshot);
typedef void (*XandaComponentDisposeCallback)(XandaComponent *component);

typedef struct
{
  const char *name;
  unsigned int state_version;
  size_t state_size;
  XandaComponentSetupCallback setup;
  XandaComponentRenderCallback render;
  XandaComponentBindCallback bind;
  XandaComponentSnapshotCallback snapshot;
  XandaComponentRestoreCallback restore;
  XandaComponentDisposeCallback dispose;
} XandaComponentDefinition;

struct XandaComponent
{
  const XandaComponentDefinition *definition;
  XandaApp *app;
  void *state;
  void *props;
  void *user_data;
  unsigned int instance_id;
  unsigned int render_count;
  int is_initialized;
};

/* ---------- Errores ---------- */
const char *xanda_version_string(void);
const char *xanda_status_message(XandaStatus status);
const char *xanda_last_error(void);
XandaStatus xanda_last_status(void);
void xanda_clear_error(void);
void xanda_set_error_handler(XandaErrorHandler handler, void *user_data);

/* ---------- Configuracion y capa de alto nivel ---------- */
XandaConfig xanda_config_default(void);
XandaStatus xanda_app_init(XandaApp *app, const XandaConfig *config);
XandaStatus xanda_app_render(XandaApp *app, XandaNode *root);
void xanda_app_reset(XandaApp *app);

/* ---------- API de componentes ---------- */
XandaStatus xanda_component_init(
    XandaComponent *component,
    XandaApp *app,
    const XandaComponentDefinition *definition,
    void *state,
    void *props,
    void *user_data);
XandaStatus xanda_component_mount(XandaComponent *component);
XandaStatus xanda_component_snapshot(const XandaComponent *component, XandaStateSnapshot *snapshot);
XandaStatus xanda_component_restore(XandaComponent *component, const XandaStateSnapshot *snapshot);
XandaStatus xanda_component_reload(XandaComponent *component, const XandaComponentDefinition *definition);
void xanda_component_reset(XandaComponent *component);
unsigned int xanda_component_id(const XandaComponent *component);
unsigned int xanda_component_render_count(const XandaComponent *component);
const char *xanda_component_name(const XandaComponent *component);
void xanda_state_snapshot_reset(XandaStateSnapshot *snapshot);
XandaStatus xanda_state_snapshot_copy(XandaStateSnapshot *snapshot, const void *data, size_t size, unsigned int version);

/* ---------- Herramientas de desarrollo ---------- */
unsigned int xanda_dev_protocol_version(void);
XandaStatus xanda_dev_restore_pending_component(XandaComponent *component);
const char *xanda_dev_snapshot_root_component(void);
const char *xanda_dev_describe_root_boundary(void);
const char *xanda_dev_list_boundaries(void);

/* ---------- Backend configurable ---------- */
void xanda_backend_set(const XandaBackendOps *ops);
void xanda_backend_reset(void);

/* ---------- API del VDOM ---------- */
XandaNode *xanda_create(const char *tag);
XandaNode *xanda_create_with_text(const char *tag, const char *text);
XandaNode *xanda_text(const char *text);
void xanda_attr(XandaNode *node, const char *key, const char *value);
XandaStatus xanda_attrs(XandaNode *node, const XandaAttribute *attrs, size_t attr_count);
void xanda_append(XandaNode *parent, XandaNode *child);
XandaStatus xanda_append_text(XandaNode *parent, const char *text);
XandaNode *xanda_child(const XandaNode *parent, int index);
XandaStatus xanda_set_key(XandaNode *node, const char *key);
void xanda_free(XandaNode *node);

/* ---------- Renderizado ---------- */
int xanda_render_to_dom(XandaNode *node);
XandaStatus xanda_mount_target(int root_id, const char *target_id);

/* ---------- Eventos ---------- */
XandaStatus xanda_bind_event(XandaNode *node, const char *event_name, const char *c_func_name);
XandaStatus xanda_bind_click(XandaNode *node, const char *c_func_name);

#ifdef __cplusplus
}
#endif

#endif /* XANDA_H */
