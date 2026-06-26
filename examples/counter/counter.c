#include "counter.h"

#include <emscripten.h>
#include <stdio.h>

typedef struct
{
  int count;
} CounterState;

typedef struct
{
  XandaComponent component;
  CounterState state;
} CounterFeature;

static CounterFeature g_counter = {0};

static XandaNode *counter_render_view(XandaComponent *component)
{
  CounterState *state = (CounterState *)component->state;
  XandaAttribute shell_attrs[] = {
      {"class", "app-shell"}};
  XandaAttribute container_attrs[] = {
      {"class", "app-container stack stack--lg"}};
  XandaAttribute card_attrs[] = {
      {"class", "surface-card stack stack--md counter-card"}};
  XandaAttribute eyebrow_attrs[] = {
      {"class", "eyebrow"}};
  XandaAttribute title_attrs[] = {
      {"class", "hero-title"}};
  XandaAttribute description_attrs[] = {
      {"class", "hero-copy"}};
  XandaAttribute button_attrs[] = {
      {"class", "button button--primary"}};
  XandaNode *shell;
  XandaNode *container;
  XandaNode *card;
  XandaNode *eyebrow;
  XandaNode *title;
  XandaNode *description;
  XandaNode *button;
  char label[64];

  snprintf(label, sizeof(label), "Contador: %d", state->count);

  shell = xanda_create("main");
  container = xanda_create("section");
  card = xanda_create("article");
  eyebrow = xanda_create_with_text("span", "Estado interactivo");
  title = xanda_create_with_text("h1", label);
  description = xanda_create_with_text("p", "Ejemplo de feature escalable con bootstrap, estado y estilos modulares.");
  button = xanda_create_with_text("button", "+1");

  if (!shell || !container || !card || !eyebrow || !title || !description || !button)
  {
    xanda_free(shell);
    xanda_free(container);
    xanda_free(card);
    xanda_free(eyebrow);
    xanda_free(title);
    xanda_free(description);
    xanda_free(button);
    return NULL;
  }

  if (xanda_attrs(shell, shell_attrs, sizeof(shell_attrs) / sizeof(shell_attrs[0])) != XANDA_STATUS_OK ||
      xanda_attrs(container, container_attrs, sizeof(container_attrs) / sizeof(container_attrs[0])) != XANDA_STATUS_OK ||
      xanda_attrs(card, card_attrs, sizeof(card_attrs) / sizeof(card_attrs[0])) != XANDA_STATUS_OK ||
      xanda_attrs(eyebrow, eyebrow_attrs, sizeof(eyebrow_attrs) / sizeof(eyebrow_attrs[0])) != XANDA_STATUS_OK ||
      xanda_attrs(title, title_attrs, sizeof(title_attrs) / sizeof(title_attrs[0])) != XANDA_STATUS_OK ||
      xanda_attrs(description, description_attrs, sizeof(description_attrs) / sizeof(description_attrs[0])) != XANDA_STATUS_OK ||
      xanda_attrs(button, button_attrs, sizeof(button_attrs) / sizeof(button_attrs[0])) != XANDA_STATUS_OK)
  {
    xanda_free(shell);
    xanda_free(container);
    xanda_free(card);
    xanda_free(eyebrow);
    xanda_free(title);
    xanda_free(description);
    xanda_free(button);
    return NULL;
  }

  if (xanda_set_key(shell, "counter-root") != XANDA_STATUS_OK)
  {
    xanda_free(shell);
    return NULL;
  }

  xanda_append(card, eyebrow);
  xanda_append(card, title);
  xanda_append(card, description);
  xanda_append(card, button);
  xanda_append(container, card);
  xanda_append(shell, container);
  if (xanda_last_status() != XANDA_STATUS_OK)
  {
    xanda_free(shell);
    return NULL;
  }

  return shell;
}

static XandaStatus counter_bind_events(XandaComponent *component, XandaNode *root)
{
  XandaNode *container = NULL;
  XandaNode *card = NULL;
  XandaNode *button = NULL;

  (void)component;

  container = xanda_child(root, 0);
  if (!container)
    return xanda_last_status();

  card = xanda_child(container, 0);
  if (!card)
    return xanda_last_status();

  button = xanda_child(card, 3);
  if (!button)
    return xanda_last_status();

  return xanda_bind_click(button, "increment_and_render");
}

static const XandaComponentDefinition COUNTER_COMPONENT = {
    "counter",
    1,
    sizeof(CounterState),
    NULL,
    counter_render_view,
    counter_bind_events,
    NULL,
    NULL,
    NULL};

XandaStatus counter_feature_init(XandaApp *app)
{
  g_counter.state.count = 42;
  return xanda_component_init(
      &g_counter.component,
      app,
      &COUNTER_COMPONENT,
      &g_counter.state,
      NULL,
      NULL);
}

XandaStatus counter_feature_mount(void)
{
  return xanda_component_mount(&g_counter.component);
}

XandaStatus counter_feature_restore_pending(void)
{
  return xanda_dev_restore_pending_component(&g_counter.component);
}

EMSCRIPTEN_KEEPALIVE
void increment_and_render(void)
{
  g_counter.state.count++;
  (void)xanda_component_mount(&g_counter.component);
}
