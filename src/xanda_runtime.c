#include "xanda_internal.h"

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

typedef struct
{
  XandaStatus status;
  char message[512];
  XandaErrorHandler handler;
  void *handler_user_data;
} XandaErrorState;

static XandaErrorState g_error_state = {XANDA_STATUS_OK, "", NULL, NULL};
static XandaBackendOps g_backend_ops = {0};
static int g_backend_initialized = 0;

static const char *XANDA_DEFAULT_TARGET_ID = "xanda";

#ifdef __EMSCRIPTEN__
void xanda_backend_register_browser(void);
#endif

char *xanda_strdup(const char *value)
{
  size_t length;
  char *copy;

  if (!value)
    return NULL;

  length = strlen(value) + 1;
  copy = (char *)malloc(length);
  if (!copy)
    return NULL;

  memcpy(copy, value, length);
  return copy;
}

const char *xanda_version_string(void)
{
  return XANDA_VERSION_STRING;
}

const char *xanda_status_message(XandaStatus status)
{
  switch (status)
  {
  case XANDA_STATUS_OK:
    return "Sin errores.";
  case XANDA_STATUS_INVALID_ARGUMENT:
    return "Argumento invalido.";
  case XANDA_STATUS_OUT_OF_MEMORY:
    return "No hay memoria suficiente para completar la operacion.";
  case XANDA_STATUS_BACKEND_UNAVAILABLE:
    return "No hay un backend de render disponible.";
  case XANDA_STATUS_RENDER_FAILED:
    return "Fallo el renderizado del arbol.";
  case XANDA_STATUS_TARGET_NOT_FOUND:
    return "No se encontro el elemento de destino para montar la vista.";
  case XANDA_STATUS_EVENT_BIND_FAILED:
    return "No se pudo registrar el evento solicitado.";
  default:
    return "Estado desconocido.";
  }
}

XandaStatus xanda_set_errorf(XandaStatus status, const char *fmt, ...)
{
  va_list args;

  g_error_state.status = status;

  if (!fmt || !fmt[0])
  {
    snprintf(g_error_state.message, sizeof(g_error_state.message), "%s", xanda_status_message(status));
  }
  else
  {
    va_start(args, fmt);
    vsnprintf(g_error_state.message, sizeof(g_error_state.message), fmt, args);
    va_end(args);
  }

  if (g_error_state.handler)
    g_error_state.handler(status, g_error_state.message, g_error_state.handler_user_data);

  return status;
}

const char *xanda_last_error(void)
{
  return g_error_state.message[0] ? g_error_state.message : xanda_status_message(g_error_state.status);
}

XandaStatus xanda_last_status(void)
{
  return g_error_state.status;
}

void xanda_clear_error(void)
{
  g_error_state.status = XANDA_STATUS_OK;
  g_error_state.message[0] = '\0';
}

void xanda_set_error_handler(XandaErrorHandler handler, void *user_data)
{
  g_error_state.handler = handler;
  g_error_state.handler_user_data = user_data;
}

void xanda_backend_ensure_default(void)
{
  if (g_backend_initialized)
    return;

#ifdef __EMSCRIPTEN__
  xanda_backend_register_browser();
#endif
  g_backend_initialized = 1;
}

const XandaBackendOps *xanda_backend_get(void)
{
  xanda_backend_ensure_default();
  return &g_backend_ops;
}

void xanda_backend_set(const XandaBackendOps *ops)
{
  if (!ops)
  {
    memset(&g_backend_ops, 0, sizeof(g_backend_ops));
  }
  else
  {
    g_backend_ops = *ops;
  }

  g_backend_initialized = 1;
}

void xanda_backend_reset(void)
{
  memset(&g_backend_ops, 0, sizeof(g_backend_ops));
  g_backend_initialized = 0;
}

XandaConfig xanda_config_default(void)
{
  XandaConfig config;

  config.mount_target_id = XANDA_DEFAULT_TARGET_ID;
  config.replace_target_children = 1;
  return config;
}

XandaStatus xanda_app_init(XandaApp *app, const XandaConfig *config)
{
  if (!app)
    return xanda_set_errorf(XANDA_STATUS_INVALID_ARGUMENT, "Xanda: la aplicacion no puede inicializarse sobre un puntero nulo.");

  app->config = config ? *config : xanda_config_default();
  if (!app->config.mount_target_id || !app->config.mount_target_id[0])
    app->config.mount_target_id = XANDA_DEFAULT_TARGET_ID;

  app->is_initialized = 1;
  xanda_backend_ensure_default();
  xanda_clear_error();
  return XANDA_STATUS_OK;
}

void xanda_app_reset(XandaApp *app)
{
  if (!app)
    return;

  app->config = xanda_config_default();
  app->is_initialized = 0;
}

XandaStatus xanda_mount_target(int root_id, const char *target_id)
{
  const XandaBackendOps *backend = xanda_backend_get();
  const char *safe_target_id = target_id;

  if (root_id < 0)
    return xanda_set_errorf(XANDA_STATUS_INVALID_ARGUMENT, "Xanda: el identificador DOM %d es invalido.", root_id);

  if (!safe_target_id || !safe_target_id[0])
    safe_target_id = XANDA_DEFAULT_TARGET_ID;

  if (!backend->mount)
    return xanda_set_errorf(XANDA_STATUS_BACKEND_UNAVAILABLE, "Xanda: no hay backend para montar en '%s'.", safe_target_id);

  return backend->mount(root_id, safe_target_id, 1);
}

XandaStatus xanda_app_render(XandaApp *app, XandaNode *root)
{
  const XandaBackendOps *backend;
  int root_id;
  XandaStatus status;

  if (!app || !app->is_initialized)
    return xanda_set_errorf(XANDA_STATUS_INVALID_ARGUMENT, "Xanda: la aplicacion debe inicializarse antes de renderizar.");

  if (!root)
    return xanda_set_errorf(XANDA_STATUS_INVALID_ARGUMENT, "Xanda: no se puede renderizar un arbol nulo.");

  root_id = xanda_render_to_dom(root);
  if (root_id < 0)
    return xanda_last_status();

  backend = xanda_backend_get();
  if (!backend->mount)
    return xanda_set_errorf(XANDA_STATUS_BACKEND_UNAVAILABLE, "Xanda: no hay backend para montar en '%s'.", app->config.mount_target_id ? app->config.mount_target_id : XANDA_DEFAULT_TARGET_ID);

  status = backend->mount(
      root_id,
      (app->config.mount_target_id && app->config.mount_target_id[0]) ? app->config.mount_target_id : XANDA_DEFAULT_TARGET_ID,
      app->config.replace_target_children);
  if (status != XANDA_STATUS_OK)
    return status;

  xanda_clear_error();
  return XANDA_STATUS_OK;
}

XandaStatus xanda_bind_event(XandaNode *node, const char *event_name, const char *c_func_name)
{
  const XandaBackendOps *backend = xanda_backend_get();

  if (!node || node->type != XANDA_ELEMENT)
    return xanda_set_errorf(XANDA_STATUS_INVALID_ARGUMENT, "Xanda: el nodo del evento debe ser un elemento valido.");

  if (node->dom_id < 0)
    return xanda_set_errorf(XANDA_STATUS_EVENT_BIND_FAILED, "Xanda: no se puede registrar '%s' antes de renderizar el nodo.", event_name ? event_name : "(null)");

  if (!event_name || !event_name[0] || !c_func_name || !c_func_name[0])
    return xanda_set_errorf(XANDA_STATUS_INVALID_ARGUMENT, "Xanda: el nombre del evento y la funcion C son obligatorios.");

  if (!backend->set_event)
    return xanda_set_errorf(XANDA_STATUS_BACKEND_UNAVAILABLE, "Xanda: no hay backend para registrar eventos.");

  return backend->set_event(node->dom_id, event_name, c_func_name);
}

XandaStatus xanda_bind_click(XandaNode *node, const char *c_func_name)
{
  return xanda_bind_event(node, "onclick", c_func_name);
}
