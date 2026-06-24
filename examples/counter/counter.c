#include "xanda/xanda.h"
#include <emscripten.h>
#include <stdio.h>

/* Estado global */
static struct
{
  int count;
} g_state = {.count = 42};

/* Forward declaration SIN static */
void increment_and_render(void);

XandaNode *counter_render(void)
{
  XandaNode *root = xanda_create("div");
  xanda_attr(root, "class", "counter");
  xanda_attr(root, "style",
             "font-family: sans-serif; padding: 20px; "
             "border: 2px solid #333; display: inline-block;");

  XandaNode *h1 = xanda_create("h1");
  char buf[64];
  snprintf(buf, sizeof(buf), "Contador: %d", g_state.count);
  xanda_append(h1, xanda_text(buf));
  xanda_append(root, h1);

  XandaNode *btn = xanda_create("button");
  xanda_attr(btn, "style",
             "padding: 10px 20px; font-size: 16px; cursor: pointer;");
  xanda_append(btn, xanda_text("+1"));
  xanda_append(root, btn);

  return root;
}

/* EMSCRIPTEN_KEEPALIVE para que no sea eliminada */
EMSCRIPTEN_KEEPALIVE
void increment_and_render(void)
{
  g_state.count++;

  XandaNode *vdom = counter_render();
  int root_id = xanda_render_to_dom(vdom);

  /* Registrar evento DESPUÉS de renderizar */
  if (vdom->child_count >= 2)
  {
    XandaNode *btn = vdom->children[1];
    xanda_event(btn, "onclick", "increment_and_render");
  }

  xanda_mount(root_id, "xanda");
  xanda_free(vdom);
}

int main(void)
{
  increment_and_render();
  return 0;
}
