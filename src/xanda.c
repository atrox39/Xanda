#include "xanda/xanda.h"
#include <string.h>

static char *str_dup(const char *s)
{
  if (!s)
    return NULL;
  size_t len = strlen(s) + 1;
  char *copy = malloc(len);
  if (copy)
    memcpy(copy, s, len);
  return copy;
}

XandaNode *xanda_create(const char *tag)
{
  XandaNode *n = calloc(1, sizeof(XandaNode));
  n->type = XANDA_ELEMENT;
  n->tag = str_dup(tag);
  n->attr_capacity = 4;
  n->attr_keys = calloc(n->attr_capacity, sizeof(char *));
  n->attr_values = calloc(n->attr_capacity, sizeof(char *));
  n->child_capacity = 4;
  n->children = calloc(n->child_capacity, sizeof(XandaNode *));
  n->dom_id = -1;
  return n;
}

XandaNode *xanda_text(const char *text)
{
  XandaNode *n = calloc(1, sizeof(XandaNode));
  n->type = XANDA_TEXT;
  n->text = str_dup(text);
  n->dom_id = -1;
  return n;
}

void xanda_attr(XandaNode *node, const char *key, const char *value)
{
  if (!node || node->type != XANDA_ELEMENT)
    return;
  if (node->attr_count >= node->attr_capacity)
  {
    node->attr_capacity *= 2;
    node->attr_keys = realloc(node->attr_keys, node->attr_capacity * sizeof(char *));
    node->attr_values = realloc(node->attr_values, node->attr_capacity * sizeof(char *));
  }
  node->attr_keys[node->attr_count] = str_dup(key);
  node->attr_values[node->attr_count] = str_dup(value);
  node->attr_count++;
}

void xanda_append(XandaNode *parent, XandaNode *child)
{
  if (!parent || !child)
    return;
  if (parent->child_count >= parent->child_capacity)
  {
    parent->child_capacity *= 2;
    parent->children = realloc(parent->children, parent->child_capacity * sizeof(XandaNode *));
  }
  parent->children[parent->child_count++] = child;
}

void xanda_free(XandaNode *node)
{
  if (!node)
    return;
  free(node->tag);
  free(node->text);
  for (int i = 0; i < node->attr_count; i++)
  {
    free(node->attr_keys[i]);
    free(node->attr_values[i]);
  }
  free(node->attr_keys);
  free(node->attr_values);
  for (int i = 0; i < node->child_count; i++)
  {
    xanda_free(node->children[i]);
  }
  free(node->children);
  free(node);
}
