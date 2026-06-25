#include "xanda_internal.h"

#include <stdlib.h>
#include <string.h>

static unsigned int g_next_component_id = 1;
static XandaComponent *g_root_component = NULL;

typedef struct XandaComponentRegistryNode
{
  XandaComponent *component;
  char *boundary_key;
  struct XandaComponentRegistryNode *next;
} XandaComponentRegistryNode;

static XandaComponentRegistryNode *g_component_registry = NULL;

static XandaComponentRegistryNode *xanda_component_registry_find(XandaComponent *component)
{
  XandaComponentRegistryNode *node = g_component_registry;

  while (node)
  {
    if (node->component == component)
      return node;
    node = node->next;
  }

  return NULL;
}

static void xanda_component_mark_tree(XandaNode *node, unsigned int owner_component_id)
{
  int i;

  if (!node)
    return;

  node->owner_component_id = owner_component_id;
  for (i = 0; i < node->child_count; ++i)
    xanda_component_mark_tree(node->children[i], owner_component_id);
}

static XandaStatus xanda_component_validate_definition(const XandaComponentDefinition *definition, const void *state)
{
  if (!definition)
    return xanda_set_errorf(XANDA_STATUS_INVALID_ARGUMENT, "Xanda: la definicion del componente es obligatoria.");

  if (!definition->render)
    return xanda_set_errorf(XANDA_STATUS_INVALID_ARGUMENT, "Xanda: el componente '%s' debe definir un callback de render.", definition->name ? definition->name : "(anonimo)");

  if (definition->state_size > 0 && !state)
    return xanda_set_errorf(XANDA_STATUS_INVALID_ARGUMENT, "Xanda: el componente '%s' declara estado, pero no recibio memoria de estado.", definition->name ? definition->name : "(anonimo)");

  return XANDA_STATUS_OK;
}

XandaStatus xanda_component_init(
    XandaComponent *component,
    XandaApp *app,
    const XandaComponentDefinition *definition,
    void *state,
    void *props,
    void *user_data)
{
  XandaStatus status;

  if (!component || !app || !definition)
    return xanda_set_errorf(XANDA_STATUS_INVALID_ARGUMENT, "Xanda: el componente requiere punteros validos para instancia, app y definicion.");

  if (!app->is_initialized)
    return xanda_set_errorf(XANDA_STATUS_INVALID_ARGUMENT, "Xanda: la app debe inicializarse antes de crear un componente.");

  status = xanda_component_validate_definition(definition, state);
  if (status != XANDA_STATUS_OK)
    return status;

  if (component->is_initialized && component->definition && component->definition->dispose)
    component->definition->dispose(component);

  memset(component, 0, sizeof(*component));
  component->definition = definition;
  component->app = app;
  component->state = state;
  component->props = props;
  component->user_data = user_data;
  component->instance_id = g_next_component_id++;

  if (component->instance_id == 0)
    component->instance_id = g_next_component_id++;

  if (definition->setup)
  {
    status = definition->setup(component);
    if (status != XANDA_STATUS_OK)
      return status;
  }

  component->is_initialized = 1;
  xanda_component_register(component);
  xanda_clear_error();
  return XANDA_STATUS_OK;
}

XandaStatus xanda_component_mount(XandaComponent *component)
{
  XandaNode *root;
  XandaStatus status;

  if (!component || !component->is_initialized || !component->definition)
    return xanda_set_errorf(XANDA_STATUS_INVALID_ARGUMENT, "Xanda: el componente debe inicializarse antes de montarse.");

  root = component->definition->render(component);
  if (!root)
  {
    if (xanda_last_status() == XANDA_STATUS_OK)
      return xanda_set_errorf(XANDA_STATUS_RENDER_FAILED, "Xanda: el componente '%s' devolvio un arbol nulo.", xanda_component_name(component));
    return xanda_last_status();
  }

  if (!root->key && xanda_component_name(component)[0])
  {
    status = xanda_set_key(root, xanda_component_name(component));
    if (status != XANDA_STATUS_OK)
    {
      xanda_free(root);
      return status;
    }
  }

  xanda_component_update_boundary_key(component, root->key);
  xanda_component_mark_tree(root, component->instance_id);
  status = xanda_app_render(component->app, root);
  if (status == XANDA_STATUS_OK && component->definition->bind)
    status = component->definition->bind(component, root);

  if (status == XANDA_STATUS_OK)
  {
    xanda_component_set_root(component);
    component->render_count++;
  }

  xanda_free(root);
  return status;
}

XandaStatus xanda_component_snapshot(const XandaComponent *component, XandaStateSnapshot *snapshot)
{
  if (!component || !component->is_initialized || !component->definition)
    return xanda_set_errorf(XANDA_STATUS_INVALID_ARGUMENT, "Xanda: el componente debe inicializarse antes de capturar un snapshot.");

  if (!snapshot)
    return xanda_set_errorf(XANDA_STATUS_INVALID_ARGUMENT, "Xanda: se requiere un destino valido para el snapshot de estado.");

  if (component->definition->snapshot)
    return component->definition->snapshot(component, snapshot);

  if (!component->state || component->definition->state_size == 0)
  {
    xanda_state_snapshot_reset(snapshot);
    xanda_clear_error();
    return XANDA_STATUS_OK;
  }

  return xanda_state_snapshot_copy(
      snapshot,
      component->state,
      component->definition->state_size,
      component->definition->state_version);
}

XandaStatus xanda_component_restore(XandaComponent *component, const XandaStateSnapshot *snapshot)
{
  if (!component || !component->is_initialized || !component->definition)
    return xanda_set_errorf(XANDA_STATUS_INVALID_ARGUMENT, "Xanda: el componente debe inicializarse antes de restaurar su estado.");

  if (!snapshot)
    return xanda_set_errorf(XANDA_STATUS_INVALID_ARGUMENT, "Xanda: el snapshot de estado no puede ser nulo.");

  if (component->definition->restore)
    return component->definition->restore(component, snapshot);

  if (snapshot->size == 0)
  {
    xanda_clear_error();
    return XANDA_STATUS_OK;
  }

  if (!component->state || component->definition->state_size == 0)
    return xanda_set_errorf(XANDA_STATUS_INVALID_ARGUMENT, "Xanda: el componente '%s' no admite restauracion automatica de estado.", xanda_component_name(component));

  if (component->definition->state_version != snapshot->version)
    return xanda_set_errorf(XANDA_STATUS_INVALID_ARGUMENT, "Xanda: la version del snapshot (%u) no coincide con la version del componente '%s' (%u).", snapshot->version, xanda_component_name(component), component->definition->state_version);

  if (component->definition->state_size != snapshot->size)
    return xanda_set_errorf(XANDA_STATUS_INVALID_ARGUMENT, "Xanda: el tamano del snapshot (%zu) no coincide con el estado esperado por '%s' (%zu).", snapshot->size, xanda_component_name(component), component->definition->state_size);

  memcpy(component->state, snapshot->data, snapshot->size);
  xanda_clear_error();
  return XANDA_STATUS_OK;
}

XandaStatus xanda_component_reload(XandaComponent *component, const XandaComponentDefinition *definition)
{
  XandaStateSnapshot snapshot = {0};
  const XandaComponentDefinition *next_definition = definition ? definition : (component ? component->definition : NULL);
  XandaStatus status;

  if (!component || !component->is_initialized)
    return xanda_set_errorf(XANDA_STATUS_INVALID_ARGUMENT, "Xanda: el componente debe inicializarse antes de recargarse.");

  status = xanda_component_validate_definition(next_definition, component->state);
  if (status != XANDA_STATUS_OK)
    return status;

  status = xanda_component_snapshot(component, &snapshot);
  if (status != XANDA_STATUS_OK)
  {
    xanda_state_snapshot_reset(&snapshot);
    return status;
  }

  component->definition = next_definition;
  if (component->definition->setup)
  {
    status = component->definition->setup(component);
    if (status != XANDA_STATUS_OK)
    {
      xanda_state_snapshot_reset(&snapshot);
      return status;
    }
  }

  status = xanda_component_restore(component, &snapshot);
  if (status == XANDA_STATUS_OK)
    status = xanda_component_mount(component);

  xanda_state_snapshot_reset(&snapshot);
  return status;
}

void xanda_component_reset(XandaComponent *component)
{
  if (!component)
    return;

  if (g_root_component == component)
    g_root_component = NULL;

  xanda_component_unregister(component);

  if (component->is_initialized && component->definition && component->definition->dispose)
    component->definition->dispose(component);

  memset(component, 0, sizeof(*component));
}

unsigned int xanda_component_id(const XandaComponent *component)
{
  return component ? component->instance_id : 0;
}

unsigned int xanda_component_render_count(const XandaComponent *component)
{
  return component ? component->render_count : 0;
}

const char *xanda_component_name(const XandaComponent *component)
{
  if (!component || !component->definition || !component->definition->name)
    return "";
  return component->definition->name;
}

void xanda_component_set_root(XandaComponent *component)
{
  g_root_component = component;
}

XandaComponent *xanda_component_get_root(void)
{
  return g_root_component;
}

void xanda_component_register(XandaComponent *component)
{
  XandaComponentRegistryNode *node;

  if (!component || xanda_component_registry_find(component))
    return;

  node = (XandaComponentRegistryNode *)calloc(1, sizeof(*node));
  if (!node)
    return;

  node->component = component;
  node->next = g_component_registry;
  g_component_registry = node;
}

void xanda_component_unregister(XandaComponent *component)
{
  XandaComponentRegistryNode *node = g_component_registry;
  XandaComponentRegistryNode *previous = NULL;

  while (node)
  {
    if (node->component == component)
    {
      if (previous)
        previous->next = node->next;
      else
        g_component_registry = node->next;

      free(node->boundary_key);
      free(node);
      return;
    }

    previous = node;
    node = node->next;
  }
}

void xanda_component_update_boundary_key(XandaComponent *component, const char *root_key)
{
  XandaComponentRegistryNode *node = xanda_component_registry_find(component);
  char *copy = NULL;

  if (!node)
    return;

  if (root_key && root_key[0])
  {
    copy = xanda_strdup(root_key);
    if (!copy)
      return;
  }

  free(node->boundary_key);
  node->boundary_key = copy;
}

const char *xanda_component_boundary_key(const XandaComponent *component)
{
  XandaComponentRegistryNode *node = xanda_component_registry_find((XandaComponent *)component);

  if (!node || !node->boundary_key)
    return "";

  return node->boundary_key;
}

XandaComponent *xanda_component_registry_first(void)
{
  return g_component_registry ? g_component_registry->component : NULL;
}

XandaComponent *xanda_component_registry_next(const XandaComponent *component)
{
  XandaComponentRegistryNode *node = xanda_component_registry_find((XandaComponent *)component);

  if (!node || !node->next)
    return NULL;

  return node->next->component;
}

unsigned int xanda_component_state_version_internal(const XandaComponent *component)
{
  if (!component || !component->definition)
    return 0;

  return component->definition->state_version;
}

size_t xanda_component_state_size_internal(const XandaComponent *component)
{
  if (!component || !component->definition)
    return 0;

  return component->definition->state_size;
}
