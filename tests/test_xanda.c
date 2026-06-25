#include "xanda/xanda.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
  int next_id;
  int mount_calls;
  int event_calls;
  int append_calls;
  int attr_calls;
  int last_root_id;
  int last_replace_flag;
  char last_target[32];
  char last_event[32];
  char last_function[64];
} FakeBackendState;

typedef struct
{
  int count;
  int dispose_calls;
  unsigned int last_owner_id;
  unsigned int last_render_count;
  char last_root_key[64];
} FakeComponentState;

static FakeBackendState g_backend_state;

static void assert_true(int condition, const char *message)
{
  if (!condition)
  {
    fprintf(stderr, "Fallo: %s\n", message);
    exit(1);
  }
}

static int fake_create_element(const char *tag)
{
  assert_true(tag != NULL, "El backend falso debe recibir una etiqueta.");
  return ++g_backend_state.next_id;
}

static int fake_create_text(const char *text)
{
  assert_true(text != NULL, "El backend falso debe recibir texto.");
  return ++g_backend_state.next_id;
}

static XandaStatus fake_set_attr(int id, const char *key, const char *value)
{
  (void)id;
  assert_true(key != NULL && value != NULL, "Los atributos no deben ser nulos.");
  g_backend_state.attr_calls++;
  return XANDA_STATUS_OK;
}

static XandaStatus fake_append_child(int parent_id, int child_id)
{
  assert_true(parent_id > 0 && child_id > 0, "Los ids del backend falso deben ser positivos.");
  g_backend_state.append_calls++;
  return XANDA_STATUS_OK;
}

static XandaStatus fake_mount(int root_id, const char *target_id, int replace_target_children)
{
  g_backend_state.mount_calls++;
  g_backend_state.last_root_id = root_id;
  g_backend_state.last_replace_flag = replace_target_children;
  snprintf(g_backend_state.last_target, sizeof(g_backend_state.last_target), "%s", target_id);
  return XANDA_STATUS_OK;
}

static XandaStatus fake_set_event(int id, const char *event_name, const char *c_func_name)
{
  assert_true(id > 0, "El evento debe registrarse sobre un id valido.");
  g_backend_state.event_calls++;
  snprintf(g_backend_state.last_event, sizeof(g_backend_state.last_event), "%s", event_name);
  snprintf(g_backend_state.last_function, sizeof(g_backend_state.last_function), "%s", c_func_name);
  return XANDA_STATUS_OK;
}

static void install_fake_backend(void)
{
  XandaBackendOps ops;

  memset(&g_backend_state, 0, sizeof(g_backend_state));
  ops.create_element = fake_create_element;
  ops.create_text = fake_create_text;
  ops.set_attr = fake_set_attr;
  ops.append_child = fake_append_child;
  ops.mount = fake_mount;
  ops.set_event = fake_set_event;
  xanda_backend_set(&ops);
}

static XandaNode *build_sample_tree(const char *title_text, const char *button_text)
{
  XandaAttribute root_attrs[] = {
      {"class", "sample"},
      {"style", "padding: 8px;"}};
  XandaNode *root;
  XandaNode *title;
  XandaNode *button;

  root = xanda_create("div");
  title = xanda_create_with_text("h1", title_text);
  button = xanda_create_with_text("button", button_text);

  if (!root || !title || !button)
  {
    xanda_free(root);
    xanda_free(title);
    xanda_free(button);
    return NULL;
  }

  if (xanda_attrs(root, root_attrs, sizeof(root_attrs) / sizeof(root_attrs[0])) != XANDA_STATUS_OK)
  {
    xanda_free(root);
    xanda_free(title);
    xanda_free(button);
    return NULL;
  }

  xanda_append(root, title);
  xanda_append(root, button);
  if (xanda_last_status() != XANDA_STATUS_OK)
  {
    xanda_free(root);
    return NULL;
  }

  return root;
}

static XandaNode *render_sample_component(XandaComponent *component)
{
  FakeComponentState *state = (FakeComponentState *)component->state;
  XandaNode *root;
  char label[64];

  snprintf(label, sizeof(label), "Componente %u (%d)", xanda_component_id(component), state ? state->count : 0);
  root = build_sample_tree(label, "Incrementar");
  if (!root)
    return NULL;

  if (xanda_set_key(root, "sample-component-root") != XANDA_STATUS_OK)
  {
    xanda_free(root);
    return NULL;
  }
  return root;
}

static XandaStatus bind_sample_component(XandaComponent *component, XandaNode *root)
{
  FakeComponentState *state = (FakeComponentState *)component->state;
  XandaNode *button = xanda_child(root, 1);

  assert_true(button != NULL, "El componente debe exponer un boton.");
  assert_true(root->owner_component_id == xanda_component_id(component), "El arbol debe quedar marcado con el id del componente.");
  assert_true(button->owner_component_id == xanda_component_id(component), "Los hijos deben heredar el owner del componente.");
  assert_true(root->key != NULL && strcmp(root->key, "sample-component-root") == 0, "La clave estable del root debe conservarse.");

  if (state)
  {
    state->last_owner_id = root->owner_component_id;
    state->last_render_count = xanda_component_render_count(component);
    snprintf(state->last_root_key, sizeof(state->last_root_key), "%s", root->key);
  }

  return xanda_bind_click(button, "on_component_click");
}

static void dispose_sample_component(XandaComponent *component)
{
  FakeComponentState *state = (FakeComponentState *)component->state;

  if (state)
    state->dispose_calls++;
}

static void test_default_config(void)
{
  XandaConfig config = xanda_config_default();

  assert_true(strcmp(xanda_version_string(), XANDA_VERSION_STRING) == 0, "La version publica debe coincidir con la macro del header.");
  assert_true(xanda_dev_protocol_version() == XANDA_DEV_PROTOCOL_VERSION, "La version del protocolo dev debe ser estable y publica.");
  assert_true(strcmp(config.mount_target_id, "xanda") == 0, "La configuracion por defecto debe montar en #xanda.");
  assert_true(config.replace_target_children == 1, "La configuracion por defecto debe reemplazar el contenido del target.");
}

static void test_vdom_helpers(void)
{
  XandaAttribute attrs[] = {
      {"class", "card"},
      {"data-role", "demo"}};
  XandaNode *root = xanda_create("section");
  XandaNode *title = xanda_create_with_text("h2", "Hola");

  assert_true(root != NULL, "Debe crear un elemento.");
  assert_true(title != NULL, "Debe crear un elemento con texto.");
  assert_true(xanda_attrs(root, attrs, sizeof(attrs) / sizeof(attrs[0])) == XANDA_STATUS_OK, "Debe aplicar atributos en lote.");

  xanda_append(root, title);
  assert_true(xanda_last_status() == XANDA_STATUS_OK, "Debe anexar el titulo.");
  assert_true(xanda_append_text(root, "Detalle") == XANDA_STATUS_OK, "Debe anexar texto de forma concisa.");
  assert_true(xanda_set_key(root, "card-root") == XANDA_STATUS_OK, "Debe permitir claves estables en nodos.");
  assert_true(root->attr_count == 2, "Debe registrar dos atributos.");
  assert_true(root->child_count == 2, "Debe registrar dos hijos.");
  assert_true(root->key != NULL && strcmp(root->key, "card-root") == 0, "Debe conservar la clave del nodo.");
  assert_true(xanda_child(root, 0) == title, "Debe recuperar el primer hijo.");

  xanda_free(root);
}

static void test_app_render_with_fake_backend(void)
{
  XandaApp app;
  XandaNode *root = build_sample_tree("Titulo", "Accion");
  XandaNode *button;

  install_fake_backend();

  assert_true(xanda_app_init(&app, NULL) == XANDA_STATUS_OK, "La app debe inicializarse con la configuracion por defecto.");
  assert_true(root != NULL, "Debe construir un arbol de prueba.");
  assert_true(xanda_app_render(&app, root) == XANDA_STATUS_OK, "La app debe renderizar el arbol correctamente.");
  button = xanda_child(root, 1);
  assert_true(button != NULL, "El arbol directo debe exponer el boton.");
  assert_true(xanda_bind_click(button, "on_click") == XANDA_STATUS_OK, "El render directo debe permitir registrar eventos despues del render.");
  assert_true(g_backend_state.mount_calls == 1, "Debe montar exactamente una vez.");
  assert_true(g_backend_state.event_calls == 1, "Debe registrar un evento.");
  assert_true(g_backend_state.attr_calls >= 2, "Debe aplicar atributos.");
  assert_true(g_backend_state.append_calls >= 2, "Debe anexar hijos al backend.");
  assert_true(strcmp(g_backend_state.last_target, "xanda") == 0, "Debe usar el target por defecto.");
  assert_true(strcmp(g_backend_state.last_event, "onclick") == 0, "Debe registrar onclick.");
  assert_true(strcmp(g_backend_state.last_function, "on_click") == 0, "Debe apuntar a la funcion C esperada.");
  xanda_free(root);
}

static void test_component_api(void)
{
  XandaApp app;
  XandaComponent component = {0};
  FakeComponentState state = {5, 0, 0, 0, ""};
  XandaComponentDefinition definition = {
      "sample-component",
      1,
      sizeof(FakeComponentState),
      NULL,
      render_sample_component,
      bind_sample_component,
      NULL,
      NULL,
      dispose_sample_component};
  unsigned int component_id;

  install_fake_backend();

  assert_true(xanda_app_init(&app, NULL) == XANDA_STATUS_OK, "La app debe inicializarse antes de crear componentes.");
  assert_true(xanda_component_init(&component, &app, &definition, &state, NULL, NULL) == XANDA_STATUS_OK, "El componente debe inicializarse correctamente.");
  component_id = xanda_component_id(&component);
  assert_true(component_id > 0, "El componente debe recibir un id estable.");
  assert_true(strcmp(xanda_component_name(&component), "sample-component") == 0, "El nombre del componente debe conservarse.");
  assert_true(xanda_component_mount(&component) == XANDA_STATUS_OK, "El componente debe montarse correctamente.");
  assert_true(xanda_component_render_count(&component) == 1, "El contador de renders debe incrementarse.");
  assert_true(state.last_owner_id == component_id, "El owner del arbol debe coincidir con el componente.");
  assert_true(strcmp(state.last_root_key, "sample-component-root") == 0, "La prueba debe observar la clave estable del root.");
  assert_true(g_backend_state.mount_calls == 1, "El montaje del componente debe delegar en la app.");
  assert_true(g_backend_state.event_calls == 1, "El bind del componente debe registrar eventos.");

  assert_true(xanda_component_mount(&component) == XANDA_STATUS_OK, "Debe poder rerenderizar el mismo componente.");
  assert_true(xanda_component_id(&component) == component_id, "La identidad del componente debe permanecer estable entre renders.");
  assert_true(xanda_component_render_count(&component) == 2, "El contador de renders debe acumular rerenders.");

  xanda_component_reset(&component);
  assert_true(state.dispose_calls == 1, "Reset debe invocar dispose una vez.");
  assert_true(xanda_component_id(&component) == 0, "Reset debe limpiar la identidad publica.");
}

static void test_component_snapshot_and_reload(void)
{
  XandaApp app;
  XandaComponent component = {0};
  FakeComponentState state = {7, 0, 0, 0, ""};
  XandaStateSnapshot snapshot = {0};
  XandaComponentDefinition definition_v1 = {
      "stateful-component",
      1,
      sizeof(FakeComponentState),
      NULL,
      render_sample_component,
      bind_sample_component,
      NULL,
      NULL,
      dispose_sample_component};
  XandaComponentDefinition definition_v2 = {
      "stateful-component",
      1,
      sizeof(FakeComponentState),
      NULL,
      render_sample_component,
      bind_sample_component,
      NULL,
      NULL,
      dispose_sample_component};
  XandaComponentDefinition incompatible_definition = {
      "stateful-component",
      2,
      sizeof(FakeComponentState),
      NULL,
      render_sample_component,
      bind_sample_component,
      NULL,
      NULL,
      dispose_sample_component};
  unsigned int stable_id;

  install_fake_backend();

  assert_true(xanda_app_init(&app, NULL) == XANDA_STATUS_OK, "La app debe inicializarse para probar snapshots.");
  assert_true(xanda_component_init(&component, &app, &definition_v1, &state, NULL, NULL) == XANDA_STATUS_OK, "El componente stateful debe inicializarse.");
  assert_true(xanda_component_mount(&component) == XANDA_STATUS_OK, "El componente stateful debe montarse.");
  stable_id = xanda_component_id(&component);

  assert_true(xanda_component_snapshot(&component, &snapshot) == XANDA_STATUS_OK, "Debe poder capturar un snapshot del estado.");
  assert_true(snapshot.size == sizeof(FakeComponentState), "El snapshot debe copiar el estado completo.");
  assert_true(snapshot.version == 1, "El snapshot debe conservar la version del estado.");

  state.count = 41;
  assert_true(xanda_component_restore(&component, &snapshot) == XANDA_STATUS_OK, "Debe poder restaurar el estado desde el snapshot.");
  assert_true(state.count == 7, "La restauracion debe reponer el estado previo.");

  state.count = 99;
  assert_true(xanda_component_reload(&component, &definition_v2) == XANDA_STATUS_OK, "Debe poder recargar una definicion compatible.");
  assert_true(xanda_component_id(&component) == stable_id, "La recarga compatible debe conservar la identidad del componente.");
  assert_true(state.count == 99, "La recarga compatible debe preservar el estado actual.");

  assert_true(xanda_component_reload(&component, &incompatible_definition) == XANDA_STATUS_INVALID_ARGUMENT, "Una definicion incompatible debe rechazarse.");
  assert_true(strstr(xanda_last_error(), "version del snapshot") != NULL, "El error de incompatibilidad debe ser descriptivo.");

  xanda_state_snapshot_reset(&snapshot);
  xanda_component_reset(&component);
}

static void test_dev_snapshot_protocol(void)
{
  XandaApp app;
  XandaComponent component = {0};
  FakeComponentState state = {12, 0, 0, 0, ""};
  XandaComponentDefinition definition = {
      "dev-component",
      3,
      sizeof(FakeComponentState),
      NULL,
      render_sample_component,
      bind_sample_component,
      NULL,
      NULL,
      dispose_sample_component};
  const char *encoded;

  install_fake_backend();

  assert_true(xanda_app_init(&app, NULL) == XANDA_STATUS_OK, "La app debe inicializarse para el protocolo dev.");
  assert_true(xanda_component_init(&component, &app, &definition, &state, NULL, NULL) == XANDA_STATUS_OK, "El componente dev debe inicializarse.");
  assert_true(xanda_component_mount(&component) == XANDA_STATUS_OK, "El componente dev debe montarse.");

  encoded = xanda_dev_snapshot_root_component();
  assert_true(encoded != NULL, "El protocolo dev debe devolver un snapshot textual.");
  assert_true(strstr(encoded, "dev-component|3|") == encoded, "El snapshot textual debe incluir nombre y version.");

  xanda_component_reset(&component);
}

static void test_boundary_registry_protocol(void)
{
  XandaApp app;
  XandaComponent component = {0};
  FakeComponentState state = {21, 0, 0, 0, ""};
  XandaComponentDefinition definition = {
      "boundary-component",
      4,
      sizeof(FakeComponentState),
      NULL,
      render_sample_component,
      bind_sample_component,
      NULL,
      NULL,
      dispose_sample_component};
  const char *root_boundary;
  const char *boundary_list;

  install_fake_backend();

  assert_true(xanda_app_init(&app, NULL) == XANDA_STATUS_OK, "La app debe inicializarse para la prueba de boundaries.");
  assert_true(xanda_component_init(&component, &app, &definition, &state, NULL, NULL) == XANDA_STATUS_OK, "El componente boundary debe inicializarse.");
  assert_true(xanda_component_mount(&component) == XANDA_STATUS_OK, "El componente boundary debe montarse.");

  root_boundary = xanda_dev_describe_root_boundary();
  assert_true(root_boundary != NULL, "Debe describir el boundary raiz activo.");
  assert_true(strstr(root_boundary, "boundary-component|sample-component-root|") == root_boundary, "El descriptor del boundary raiz debe incluir nombre y clave.");

  boundary_list = xanda_dev_list_boundaries();
  assert_true(boundary_list != NULL, "Debe poder listar boundaries registrados.");
  assert_true(strstr(boundary_list, "boundary-component|sample-component-root|") != NULL, "La lista debe incluir el boundary montado.");

  xanda_component_reset(&component);
  boundary_list = xanda_dev_list_boundaries();
  assert_true(boundary_list != NULL, "La lista de boundaries debe seguir disponible tras reset.");
  assert_true(boundary_list[0] == '\0', "La lista debe quedar vacia al desregistrar el unico boundary.");
}

static void test_event_requires_render(void)
{
  XandaNode *button = xanda_create_with_text("button", "Sin render");

  assert_true(button != NULL, "Debe crear el boton para la prueba de error.");
  assert_true(xanda_bind_click(button, "on_click") == XANDA_STATUS_EVENT_BIND_FAILED, "No debe permitir registrar eventos antes de renderizar.");
  assert_true(strstr(xanda_last_error(), "antes de renderizar") != NULL, "Debe reportar un error descriptivo.");

  xanda_free(button);
}

int main(void)
{
  test_default_config();
  test_vdom_helpers();
  test_app_render_with_fake_backend();
  test_component_api();
  test_component_snapshot_and_reload();
  test_dev_snapshot_protocol();
  test_boundary_registry_protocol();
  test_event_requires_render();
  puts("OK");
  return 0;
}
