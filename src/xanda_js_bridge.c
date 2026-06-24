#include "xanda/xanda.h"
#include <emscripten.h>

/* ---------- Creación de nodos DOM ---------- */
EM_JS(int, js_create_element, (const char *tag), {
  var tagStr = UTF8ToString(tag);
  if (!tagStr)
    tagStr = "div";
  var id = window.__xanda.next_id++;
  var el = document.createElement(tagStr);
  window.__xanda.nodes[id] = el;
  return id;
});

EM_JS(int, js_create_text, (const char *text), {
  var id = window.__xanda.next_id++;
  var el = document.createTextNode(UTF8ToString(text));
  window.__xanda.nodes[id] = el;
  return id;
});

/* ---------- Manipulación ---------- */
EM_JS(void, js_set_attr, (int id, const char *key, const char *value), {
  var el = window.__xanda.nodes[id];
  if (el && el.setAttribute)
  {
    el.setAttribute(UTF8ToString(key), UTF8ToString(value));
  }
});

EM_JS(void, js_append_child, (int parent_id, int child_id), {
  var parent = window.__xanda.nodes[parent_id];
  var child = window.__xanda.nodes[child_id];
  if (parent && child)
    parent.appendChild(child);
});

/* ---------- Mount ---------- */
EM_JS(void, js_mount, (int root_id, const char *target_id), {
  var target = document.getElementById(UTF8ToString(target_id));
  if (!target)
  {
    console.error("Xanda: no se encontro #" + UTF8ToString(target_id));
    return;
  }
  target.innerHTML = "";
  var el = window.__xanda.nodes[root_id];
  if (el)
    target.appendChild(el);
});

/* ---------- Eventos ---------- */
EM_JS(void, js_set_event, (int id, const char *event_name, const char *c_func_name), {
  var el = window.__xanda.nodes[id];
  if (!el)
    return;
  var event = UTF8ToString(event_name);
  var funcName = UTF8ToString(c_func_name);

  el[event] = function()
  {
    if (Module && Module.ccall)
    {
      Module.ccall(funcName, 'null', [], []);
    }
  };
});

/* ---------- Renderizado recursivo ---------- */
int xanda_render_to_dom(XandaNode *node)
{
  if (!node)
    return -1;

  int id;
  if (node->type == XANDA_TEXT)
  {
    id = js_create_text(node->text ? node->text : "");
  }
  else
  {
    id = js_create_element(node->tag ? node->tag : "div");
    for (int i = 0; i < node->attr_count; i++)
    {
      js_set_attr(id, node->attr_keys[i], node->attr_values[i]);
    }
    for (int i = 0; i < node->child_count; i++)
    {
      int child_id = xanda_render_to_dom(node->children[i]);
      if (child_id >= 0)
        js_append_child(id, child_id);
    }
  }
  node->dom_id = id;
  return id;
}

void xanda_mount(int root_id, const char *target_id)
{
  js_mount(root_id, target_id);
}

void xanda_event(XandaNode *node, const char *event_name, const char *c_func_name)
{
  if (!node || node->type != XANDA_ELEMENT || node->dom_id < 0)
    return;
  js_set_event(node->dom_id, event_name, c_func_name);
}
