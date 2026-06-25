#include "xanda_internal.h"

#include <stdlib.h>
#include <string.h>

enum
{
  XANDA_INITIAL_CAPACITY = 4
};

static XandaNode *xanda_alloc_node(XandaNodeType type)
{
  XandaNode *node = (XandaNode *)calloc(1, sizeof(XandaNode));
  if (!node)
  {
    xanda_set_errorf(XANDA_STATUS_OUT_OF_MEMORY, "Xanda: no se pudo reservar memoria para el nodo.");
    return NULL;
  }

  node->type = type;
  node->dom_id = -1;
  node->owner_component_id = 0;
  return node;
}

static XandaStatus xanda_reserve_attrs(XandaNode *node)
{
  if (!node)
    return xanda_set_errorf(XANDA_STATUS_INVALID_ARGUMENT, "Xanda: el nodo para atributos es nulo.");

  if (node->attr_capacity > 0)
    return XANDA_STATUS_OK;

  node->attr_capacity = XANDA_INITIAL_CAPACITY;
  node->attr_keys = (char **)calloc((size_t)node->attr_capacity, sizeof(char *));
  node->attr_values = (char **)calloc((size_t)node->attr_capacity, sizeof(char *));
  if (!node->attr_keys || !node->attr_values)
  {
    free(node->attr_keys);
    free(node->attr_values);
    node->attr_keys = NULL;
    node->attr_values = NULL;
    node->attr_capacity = 0;
    return xanda_set_errorf(XANDA_STATUS_OUT_OF_MEMORY, "Xanda: no se pudo inicializar el almacenamiento de atributos.");
  }

  return XANDA_STATUS_OK;
}

static XandaStatus xanda_reserve_children(XandaNode *node)
{
  if (!node)
    return xanda_set_errorf(XANDA_STATUS_INVALID_ARGUMENT, "Xanda: el nodo para hijos es nulo.");

  if (node->child_capacity > 0)
    return XANDA_STATUS_OK;

  node->child_capacity = XANDA_INITIAL_CAPACITY;
  node->children = (XandaNode **)calloc((size_t)node->child_capacity, sizeof(XandaNode *));
  if (!node->children)
  {
    node->child_capacity = 0;
    return xanda_set_errorf(XANDA_STATUS_OUT_OF_MEMORY, "Xanda: no se pudo inicializar el almacenamiento de hijos.");
  }

  return XANDA_STATUS_OK;
}

static XandaStatus xanda_grow_strings(char ***items, int *capacity)
{
  int next_capacity = (*capacity > 0) ? (*capacity * 2) : XANDA_INITIAL_CAPACITY;
  char **next_items = (char **)realloc(*items, (size_t)next_capacity * sizeof(char *));
  if (!next_items)
    return xanda_set_errorf(XANDA_STATUS_OUT_OF_MEMORY, "Xanda: no se pudo ampliar memoria dinamica.");

  for (int i = *capacity; i < next_capacity; ++i)
    next_items[i] = NULL;

  *items = next_items;
  *capacity = next_capacity;
  return XANDA_STATUS_OK;
}

static XandaStatus xanda_grow_attr_storage(XandaNode *node)
{
  int original_capacity;
  int target_capacity;

  original_capacity = node->attr_capacity;
  if (xanda_grow_strings(&node->attr_keys, &node->attr_capacity) != XANDA_STATUS_OK)
    return xanda_last_status();

  target_capacity = node->attr_capacity;
  node->attr_capacity = original_capacity;
  if (xanda_grow_strings(&node->attr_values, &node->attr_capacity) != XANDA_STATUS_OK)
    return xanda_last_status();

  node->attr_capacity = target_capacity;
  return XANDA_STATUS_OK;
}

static XandaStatus xanda_grow_children(XandaNode ***items, int *capacity)
{
  int next_capacity = (*capacity > 0) ? (*capacity * 2) : XANDA_INITIAL_CAPACITY;
  XandaNode **next_items = (XandaNode **)realloc(*items, (size_t)next_capacity * sizeof(XandaNode *));
  if (!next_items)
    return xanda_set_errorf(XANDA_STATUS_OUT_OF_MEMORY, "Xanda: no se pudo ampliar la lista de hijos.");

  for (int i = *capacity; i < next_capacity; ++i)
    next_items[i] = NULL;

  *items = next_items;
  *capacity = next_capacity;
  return XANDA_STATUS_OK;
}

XandaNode *xanda_create(const char *tag)
{
  XandaNode *node = xanda_alloc_node(XANDA_ELEMENT);
  if (!node)
    return NULL;

  node->tag = xanda_strdup(tag ? tag : "div");
  if (!node->tag)
  {
    xanda_free(node);
    xanda_set_errorf(XANDA_STATUS_OUT_OF_MEMORY, "Xanda: no se pudo copiar la etiqueta del nodo.");
    return NULL;
  }

  if (xanda_reserve_attrs(node) != XANDA_STATUS_OK || xanda_reserve_children(node) != XANDA_STATUS_OK)
  {
    xanda_free(node);
    return NULL;
  }

  xanda_clear_error();
  return node;
}

XandaNode *xanda_create_with_text(const char *tag, const char *text)
{
  XandaNode *node = xanda_create(tag);
  if (!node)
    return NULL;

  if (xanda_append_text(node, text) != XANDA_STATUS_OK)
  {
    xanda_free(node);
    return NULL;
  }

  return node;
}

XandaNode *xanda_text(const char *text)
{
  XandaNode *node = xanda_alloc_node(XANDA_TEXT);
  if (!node)
    return NULL;

  node->text = xanda_strdup(text ? text : "");
  if (!node->text)
  {
    xanda_free(node);
    xanda_set_errorf(XANDA_STATUS_OUT_OF_MEMORY, "Xanda: no se pudo copiar el contenido del texto.");
    return NULL;
  }

  xanda_clear_error();
  return node;
}

XandaStatus xanda_attrs(XandaNode *node, const XandaAttribute *attrs, size_t attr_count)
{
  if (!node || node->type != XANDA_ELEMENT)
    return xanda_set_errorf(XANDA_STATUS_INVALID_ARGUMENT, "Xanda: solo los nodos de tipo elemento aceptan atributos.");

  if (!attrs && attr_count > 0)
    return xanda_set_errorf(XANDA_STATUS_INVALID_ARGUMENT, "Xanda: la lista de atributos es invalida.");

  if (xanda_reserve_attrs(node) != XANDA_STATUS_OK)
    return xanda_last_status();

  for (size_t i = 0; i < attr_count; ++i)
  {
    const char *key = attrs[i].key;
    const char *value = attrs[i].value;
    char *key_copy = NULL;
    char *value_copy = NULL;

    if (!key || !value)
      return xanda_set_errorf(XANDA_STATUS_INVALID_ARGUMENT, "Xanda: las claves y valores de atributos no pueden ser nulos.");

    if (node->attr_count >= node->attr_capacity)
    {
      if (xanda_grow_attr_storage(node) != XANDA_STATUS_OK)
        return xanda_last_status();
    }

    key_copy = xanda_strdup(key);
    value_copy = xanda_strdup(value);
    if (!key_copy || !value_copy)
    {
      free(key_copy);
      free(value_copy);
      return xanda_set_errorf(XANDA_STATUS_OUT_OF_MEMORY, "Xanda: no se pudo copiar un atributo.");
    }

    node->attr_keys[node->attr_count] = key_copy;
    node->attr_values[node->attr_count] = value_copy;
    node->attr_count++;
  }

  xanda_clear_error();
  return XANDA_STATUS_OK;
}

void xanda_attr(XandaNode *node, const char *key, const char *value)
{
  XandaAttribute attr = {key, value};
  (void)xanda_attrs(node, &attr, 1);
}

void xanda_append(XandaNode *parent, XandaNode *child)
{
  if (!parent || !child)
  {
    xanda_set_errorf(XANDA_STATUS_INVALID_ARGUMENT, "Xanda: no se puede anexar un hijo nulo.");
    return;
  }

  if (parent->type != XANDA_ELEMENT)
  {
    xanda_set_errorf(XANDA_STATUS_INVALID_ARGUMENT, "Xanda: solo los nodos de tipo elemento pueden contener hijos.");
    return;
  }

  if (xanda_reserve_children(parent) != XANDA_STATUS_OK)
    return;

  if (parent->child_count >= parent->child_capacity)
  {
    if (xanda_grow_children(&parent->children, &parent->child_capacity) != XANDA_STATUS_OK)
      return;
  }

  parent->children[parent->child_count++] = child;
  xanda_clear_error();
}

XandaStatus xanda_append_text(XandaNode *parent, const char *text)
{
  XandaNode *child = xanda_text(text);
  if (!child)
    return xanda_last_status();

  xanda_append(parent, child);
  if (xanda_last_status() != XANDA_STATUS_OK)
  {
    xanda_free(child);
    return xanda_last_status();
  }

  return XANDA_STATUS_OK;
}

XandaNode *xanda_child(const XandaNode *parent, int index)
{
  if (!parent)
  {
    xanda_set_errorf(XANDA_STATUS_INVALID_ARGUMENT, "Xanda: no se puede leer un hijo desde un nodo nulo.");
    return NULL;
  }

  if (index < 0 || index >= parent->child_count)
  {
    xanda_set_errorf(XANDA_STATUS_INVALID_ARGUMENT, "Xanda: el indice de hijo %d esta fuera de rango.", index);
    return NULL;
  }

  xanda_clear_error();
  return parent->children[index];
}

XandaStatus xanda_set_key(XandaNode *node, const char *key)
{
  char *copy;

  if (!node)
    return xanda_set_errorf(XANDA_STATUS_INVALID_ARGUMENT, "Xanda: no se puede asignar una clave a un nodo nulo.");

  if (!key || !key[0])
  {
    free(node->key);
    node->key = NULL;
    xanda_clear_error();
    return XANDA_STATUS_OK;
  }

  copy = xanda_strdup(key);
  if (!copy)
    return xanda_set_errorf(XANDA_STATUS_OUT_OF_MEMORY, "Xanda: no se pudo copiar la clave del nodo.");

  free(node->key);
  node->key = copy;
  xanda_clear_error();
  return XANDA_STATUS_OK;
}

void xanda_free(XandaNode *node)
{
  int i;

  if (!node)
    return;

  free(node->tag);
  free(node->text);
  free(node->key);

  for (i = 0; i < node->attr_count; ++i)
  {
    free(node->attr_keys[i]);
    free(node->attr_values[i]);
  }

  free(node->attr_keys);
  free(node->attr_values);

  for (i = 0; i < node->child_count; ++i)
    xanda_free(node->children[i]);

  free(node->children);
  free(node);
}
