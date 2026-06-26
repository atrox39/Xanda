#include "minimal.h"

typedef struct
{
  XandaComponent component;
} MinimalFeature;

static MinimalFeature g_minimal_feature = {0};

static XandaNode *minimal_render_view(XandaComponent *component)
{
  XandaAttribute shell_attrs[] = {
      {"class", "app-shell"}};
  XandaAttribute container_attrs[] = {
      {"class", "app-container stack stack--lg"}};
  XandaAttribute hero_attrs[] = {
      {"class", "surface-card stack stack--md"}};
  XandaAttribute eyebrow_attrs[] = {
      {"class", "eyebrow"}};
  XandaAttribute title_attrs[] = {
      {"class", "hero-title"}};
  XandaAttribute description_attrs[] = {
      {"class", "hero-copy"}};
  XandaNode *shell;
  XandaNode *container;
  XandaNode *hero;
  XandaNode *eyebrow;
  XandaNode *title;
  XandaNode *description;

  (void)component;

  shell = xanda_create("main");
  container = xanda_create("section");
  hero = xanda_create("article");
  eyebrow = xanda_create_with_text("span", "Plantilla base");
  title = xanda_create_with_text("h1", "Hola desde Xanda");
  description = xanda_create_with_text("p", "Proyecto minimo con entry point, bootstrap y estilos escalables listos para crecer.");
  if (!shell || !container || !hero || !eyebrow || !title || !description)
  {
    xanda_free(shell);
    xanda_free(container);
    xanda_free(hero);
    xanda_free(eyebrow);
    xanda_free(title);
    xanda_free(description);
    return NULL;
  }

  if (xanda_attrs(shell, shell_attrs, sizeof(shell_attrs) / sizeof(shell_attrs[0])) != XANDA_STATUS_OK ||
      xanda_attrs(container, container_attrs, sizeof(container_attrs) / sizeof(container_attrs[0])) != XANDA_STATUS_OK ||
      xanda_attrs(hero, hero_attrs, sizeof(hero_attrs) / sizeof(hero_attrs[0])) != XANDA_STATUS_OK ||
      xanda_attrs(eyebrow, eyebrow_attrs, sizeof(eyebrow_attrs) / sizeof(eyebrow_attrs[0])) != XANDA_STATUS_OK ||
      xanda_attrs(title, title_attrs, sizeof(title_attrs) / sizeof(title_attrs[0])) != XANDA_STATUS_OK ||
      xanda_attrs(description, description_attrs, sizeof(description_attrs) / sizeof(description_attrs[0])) != XANDA_STATUS_OK)
  {
    xanda_free(shell);
    xanda_free(container);
    xanda_free(hero);
    xanda_free(eyebrow);
    xanda_free(title);
    xanda_free(description);
    return NULL;
  }

  xanda_append(hero, eyebrow);
  xanda_append(hero, title);
  xanda_append(hero, description);
  xanda_append(container, hero);
  xanda_append(shell, container);
  if (xanda_last_status() != XANDA_STATUS_OK)
  {
    xanda_free(shell);
    return NULL;
  }

  if (xanda_set_key(shell, "minimal-root") != XANDA_STATUS_OK)
  {
    xanda_free(shell);
    return NULL;
  }

  return shell;
}

static const XandaComponentDefinition MINIMAL_COMPONENT = {
    "minimal",
    0,
    0,
    NULL,
    minimal_render_view,
    NULL,
    NULL,
    NULL,
    NULL};

XandaStatus minimal_feature_init(XandaApp *app)
{
  return xanda_component_init(
      &g_minimal_feature.component,
      app,
      &MINIMAL_COMPONENT,
      NULL,
      NULL,
      NULL);
}

XandaStatus minimal_feature_restore_pending(void)
{
  return xanda_dev_restore_pending_component(&g_minimal_feature.component);
}

XandaStatus minimal_feature_mount(void)
{
  return xanda_component_mount(&g_minimal_feature.component);
}
