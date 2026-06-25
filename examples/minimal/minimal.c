#include "xanda/xanda.h"

static XandaApp g_app;
static XandaComponent g_component;

static XandaNode *minimal_render(XandaComponent *component)
{
  XandaAttribute root_attrs[] = {
      {"style", "font-family: sans-serif; padding: 24px; line-height: 1.5;"}};
  XandaNode *root;
  XandaNode *title;

  (void)component;

  root = xanda_create("main");
  title = xanda_create_with_text("h1", "Hola desde Xanda");
  if (!root || !title)
  {
    xanda_free(root);
    xanda_free(title);
    return NULL;
  }

  if (xanda_attrs(root, root_attrs, sizeof(root_attrs) / sizeof(root_attrs[0])) != XANDA_STATUS_OK)
  {
    xanda_free(root);
    xanda_free(title);
    return NULL;
  }

  xanda_append(root, title);
  if (xanda_last_status() != XANDA_STATUS_OK || xanda_append_text(root, "Render con configuracion por defecto y sin boilerplate extra.") != XANDA_STATUS_OK)
  {
    xanda_free(root);
    return NULL;
  }

  if (xanda_set_key(root, "minimal-root") != XANDA_STATUS_OK)
  {
    xanda_free(root);
    return NULL;
  }

  return root;
}

int main(void)
{
  XandaComponentDefinition definition = {
      "minimal",
      0,
      0,
      NULL,
      minimal_render,
      NULL,
      NULL,
      NULL,
      NULL};

  if (xanda_app_init(&g_app, NULL) != XANDA_STATUS_OK)
    return 1;

  if (xanda_component_init(&g_component, &g_app, &definition, NULL, NULL, NULL) != XANDA_STATUS_OK)
    return 1;

  if (xanda_dev_restore_pending_component(&g_component) != XANDA_STATUS_OK)
    return 1;

  return xanda_component_mount(&g_component) == XANDA_STATUS_OK ? 0 : 1;
}
