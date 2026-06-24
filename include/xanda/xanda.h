#ifndef XANDA_H
#define XANDA_H

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- Tipos ---------- */
typedef enum {
    XANDA_ELEMENT,
    XANDA_TEXT
} XandaNodeType;

typedef struct XandaNode {
    XandaNodeType type;
    char *tag;
    char *text;
    char **attr_keys;
    char **attr_values;
    int attr_count;
    int attr_capacity;
    struct XandaNode **children;
    int child_count;
    int child_capacity;
    int dom_id;
} XandaNode;

/* ---------- API del VDOM ---------- */
XandaNode* xanda_create(const char *tag);
XandaNode* xanda_text(const char *text);
void xanda_attr(XandaNode *node, const char *key, const char *value);
void xanda_append(XandaNode *parent, XandaNode *child);
void xanda_free(XandaNode *node);

/* ---------- Renderizado ---------- */
int xanda_render_to_dom(XandaNode *node);
void xanda_mount(int root_id, const char *target_id);

/* ---------- Eventos ---------- */
void xanda_event(XandaNode *node, const char *event_name, const char *c_func_name);

#ifdef __cplusplus
}
#endif

#endif /* XANDA_H */
