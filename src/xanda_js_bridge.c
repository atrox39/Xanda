#include "xanda_internal.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

static int xanda_backend_ready(const XandaBackendOps **backend)
{
  *backend = xanda_backend_get();
  if (!(*backend)->create_element || !(*backend)->create_text || !(*backend)->set_attr || !(*backend)->append_child)
    return 0;
  return 1;
}

#ifdef __EMSCRIPTEN__
EM_JS(void, js_bootstrap_runtime, (), {
  var scope = (typeof globalThis !== "undefined") ? globalThis :
              ((typeof window !== "undefined") ? window : self);
  if (!scope.__xanda)
    scope.__xanda = { next_id: 0, nodes: {} };
});

EM_JS(int, js_create_element, (const char *tag), {
  var scope = (typeof globalThis !== "undefined") ? globalThis :
              ((typeof window !== "undefined") ? window : self);
  if (!scope.__xanda)
    scope.__xanda = { next_id: 0, nodes: {} };
  var tagStr = UTF8ToString(tag || 0) || "div";
  var id = scope.__xanda.next_id++;
  var el = document.createElement(tagStr);
  scope.__xanda.nodes[id] = el;
  return id;
});

EM_JS(int, js_create_text, (const char *text), {
  var scope = (typeof globalThis !== "undefined") ? globalThis :
              ((typeof window !== "undefined") ? window : self);
  if (!scope.__xanda)
    scope.__xanda = { next_id: 0, nodes: {} };
  var value = UTF8ToString(text || 0) || "";
  var id = scope.__xanda.next_id++;
  var el = document.createTextNode(value);
  scope.__xanda.nodes[id] = el;
  return id;
});

EM_JS(int, js_set_attr, (int id, const char *key, const char *value), {
  var scope = (typeof globalThis !== "undefined") ? globalThis :
              ((typeof window !== "undefined") ? window : self);
  if (!scope.__xanda)
    scope.__xanda = { next_id: 0, nodes: {} };
  var el = scope.__xanda.nodes[id];
  if (!el || !el.setAttribute)
    return 0;

  el.setAttribute(UTF8ToString(key), UTF8ToString(value));
  return 1;
});

EM_JS(int, js_append_child, (int parent_id, int child_id), {
  var scope = (typeof globalThis !== "undefined") ? globalThis :
              ((typeof window !== "undefined") ? window : self);
  if (!scope.__xanda)
    scope.__xanda = { next_id: 0, nodes: {} };
  var parent = scope.__xanda.nodes[parent_id];
  var child = scope.__xanda.nodes[child_id];
  if (!parent || !child)
    return 0;

  parent.appendChild(child);
  return 1;
});

EM_JS(int, js_mount, (int root_id, const char *target_id, int replace_target_children), {
  var scope = (typeof globalThis !== "undefined") ? globalThis :
              ((typeof window !== "undefined") ? window : self);
  if (!scope.__xanda)
    scope.__xanda = { next_id: 0, nodes: {} };
  var target = document.getElementById(UTF8ToString(target_id));
  var el = scope.__xanda.nodes[root_id];
  if (!target)
    return -1;
  if (!el)
    return 0;

  if (replace_target_children)
    target.innerHTML = "";

  target.appendChild(el);
  return 1;
});

EM_JS(int, js_set_event, (int id, const char *event_name, const char *c_func_name), {
  var scope = (typeof globalThis !== "undefined") ? globalThis :
              ((typeof window !== "undefined") ? window : self);
  if (!scope.__xanda)
    scope.__xanda = { next_id: 0, nodes: {} };
  var el = scope.__xanda.nodes[id];
  if (!el)
    return 0;

  var event = UTF8ToString(event_name);
  var funcName = UTF8ToString(c_func_name);

  el[event] = function() {
    if (typeof Module !== "undefined" && Module && Module.ccall)
    {
      Module.ccall(funcName, null, [], []);
    }
  };

  return 1;
});

static XandaStatus browser_set_attr(int id, const char *key, const char *value)
{
  if (!js_set_attr(id, key, value))
    return xanda_set_errorf(XANDA_STATUS_RENDER_FAILED, "Xanda: no se pudo aplicar el atributo '%s' al nodo %d.", key, id);
  xanda_clear_error();
  return XANDA_STATUS_OK;
}

static XandaStatus browser_append_child(int parent_id, int child_id)
{
  if (!js_append_child(parent_id, child_id))
    return xanda_set_errorf(XANDA_STATUS_RENDER_FAILED, "Xanda: no se pudo anexar el nodo %d al padre %d.", child_id, parent_id);
  xanda_clear_error();
  return XANDA_STATUS_OK;
}

static XandaStatus browser_mount(int root_id, const char *target_id, int replace_target_children)
{
  int result = js_mount(root_id, target_id, replace_target_children);
  if (result < 0)
    return xanda_set_errorf(XANDA_STATUS_TARGET_NOT_FOUND, "Xanda: no se encontro el contenedor '#%s'.", target_id);
  if (result == 0)
    return xanda_set_errorf(XANDA_STATUS_RENDER_FAILED, "Xanda: no existe un nodo DOM para el id %d.", root_id);

  xanda_clear_error();
  return XANDA_STATUS_OK;
}

static XandaStatus browser_set_event(int id, const char *event_name, const char *c_func_name)
{
  if (!js_set_event(id, event_name, c_func_name))
    return xanda_set_errorf(XANDA_STATUS_EVENT_BIND_FAILED, "Xanda: no se pudo registrar '%s' en el nodo %d.", event_name, id);

  xanda_clear_error();
  return XANDA_STATUS_OK;
}

void xanda_backend_register_browser(void)
{
  XandaBackendOps ops;

  js_bootstrap_runtime();

  ops.create_element = js_create_element;
  ops.create_text = js_create_text;
  ops.set_attr = browser_set_attr;
  ops.append_child = browser_append_child;
  ops.mount = browser_mount;
  ops.set_event = browser_set_event;

  xanda_backend_set(&ops);
}
#endif

int xanda_render_to_dom(XandaNode *node)
{
  const XandaBackendOps *backend;
  int id;
  int i;

  if (!node)
  {
    xanda_set_errorf(XANDA_STATUS_INVALID_ARGUMENT, "Xanda: no se puede renderizar un nodo nulo.");
    return -1;
  }

  if (!xanda_backend_ready(&backend))
  {
    xanda_set_errorf(XANDA_STATUS_BACKEND_UNAVAILABLE, "Xanda: el backend actual no implementa las operaciones de render.");
    return -1;
  }

  if (node->type == XANDA_TEXT)
  {
    id = backend->create_text(node->text ? node->text : "");
    if (id < 0)
    {
      xanda_set_errorf(XANDA_STATUS_RENDER_FAILED, "Xanda: no se pudo crear un nodo de texto.");
      return -1;
    }
  }
  else
  {
    id = backend->create_element(node->tag ? node->tag : "div");
    if (id < 0)
    {
      xanda_set_errorf(XANDA_STATUS_RENDER_FAILED, "Xanda: no se pudo crear un elemento '%s'.", node->tag ? node->tag : "div");
      return -1;
    }

    for (i = 0; i < node->attr_count; ++i)
    {
      if (backend->set_attr(id, node->attr_keys[i], node->attr_values[i]) != XANDA_STATUS_OK)
        return -1;
    }

    for (i = 0; i < node->child_count; ++i)
    {
      int child_id = xanda_render_to_dom(node->children[i]);
      if (child_id < 0)
        return -1;

      if (backend->append_child(id, child_id) != XANDA_STATUS_OK)
        return -1;
    }
  }

  node->dom_id = id;
  xanda_clear_error();
  return id;
}
