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

static XandaNode *counter_render(XandaComponent *component)
{
  CounterState *state = (CounterState *)component->state;
  XandaAttribute root_attrs[] = {
      {"class", "counter"},
      {"style", "font-family: sans-serif; padding: 20px; border: 2px solid #333; display: inline-block;"}};
  XandaAttribute button_attrs[] = {
      {"style", "padding: 10px 20px; font-size: 16px; cursor: pointer;"}};
  XandaNode *root;
  XandaNode *title;
  XandaNode *button;
  char label[64];

  snprintf(label, sizeof(label), "Contador: %d", state->count);

  root = xanda_create("div");
  title = xanda_create_with_text("h1", label);
  button = xanda_create_with_text("button", "+1");

  if (!root || !title || !button)
  {
    xanda_free(root);
    xanda_free(title);
    xanda_free(button);
    return NULL;
  }

  if (xanda_attrs(root, root_attrs, sizeof(root_attrs) / sizeof(root_attrs[0])) != XANDA_STATUS_OK ||
      xanda_attrs(button, button_attrs, sizeof(button_attrs) / sizeof(button_attrs[0])) != XANDA_STATUS_OK)
  {
    xanda_free(root);
    xanda_free(title);
    xanda_free(button);
    return NULL;
  }

  if (xanda_set_key(root, "counter-root") != XANDA_STATUS_OK)
  {
    xanda_free(root);
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

static XandaStatus counter_bind(XandaComponent *component, XandaNode *root)
{
  XandaNode *button = NULL;

  (void)component;

  button = xanda_child(root, 1);
  if (!button)
    return xanda_last_status();

  return xanda_bind_click(button, "increment_and_render");
}

static const XandaComponentDefinition COUNTER_COMPONENT = {
    "counter",
    1,
    sizeof(CounterState),
    NULL,
    counter_render,
    counter_bind,
    NULL,
    NULL,
    NULL};

XandaStatus counter_init(XandaApp *app)
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

XandaStatus counter_mount(void)
{
  return xanda_component_mount(&g_counter.component);
}

XandaStatus counter_restore_pending(void)
{
  return xanda_dev_restore_pending_component(&g_counter.component);
}

EMSCRIPTEN_KEEPALIVE
void increment_and_render(void)
{
  g_counter.state.count++;
  (void)xanda_component_mount(&g_counter.component);
}
