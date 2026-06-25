#include "xanda_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>

EM_JS(char *, js_dev_consume_pending_snapshot, (), {
  var scope = (typeof globalThis !== "undefined") ? globalThis :
              ((typeof window !== "undefined") ? window : self);
  if (!scope || !scope.sessionStorage)
    return 0;

  var value = scope.sessionStorage.getItem("__xanda_dev_snapshot__");
  if (!value)
    return 0;

  scope.sessionStorage.removeItem("__xanda_dev_snapshot__");
  return stringToNewUTF8(value);
});
#endif

static char *g_snapshot_string = NULL;
static char *g_boundary_string = NULL;
static char *g_boundary_list_string = NULL;

unsigned int xanda_dev_protocol_version(void)
{
  return XANDA_DEV_PROTOCOL_VERSION;
}

static int xanda_dev_hex_value(char ch)
{
  if (ch >= '0' && ch <= '9')
    return ch - '0';
  if (ch >= 'a' && ch <= 'f')
    return 10 + (ch - 'a');
  if (ch >= 'A' && ch <= 'F')
    return 10 + (ch - 'A');
  return -1;
}

static XandaStatus xanda_dev_encode_snapshot(const char *component_name, const XandaStateSnapshot *snapshot, char **output)
{
  size_t name_length;
  size_t total_length;
  char *buffer;
  char *cursor;
  size_t i;
  static const char hex_digits[] = "0123456789abcdef";

  if (!output)
    return xanda_set_errorf(XANDA_STATUS_INVALID_ARGUMENT, "Xanda: el buffer de salida del snapshot dev no puede ser nulo.");

  if (!component_name)
    component_name = "";

  name_length = strlen(component_name);
  total_length = name_length + 32 + (snapshot->size * 2) + 1;
  buffer = (char *)malloc(total_length);
  if (!buffer)
    return xanda_set_errorf(XANDA_STATUS_OUT_OF_MEMORY, "Xanda: no se pudo codificar el snapshot dev.");

  cursor = buffer;
  memcpy(cursor, component_name, name_length);
  cursor += name_length;
  *cursor++ = '|';
  cursor += sprintf(cursor, "%u|%zu|", snapshot->version, snapshot->size);

  for (i = 0; i < snapshot->size; ++i)
  {
    unsigned char value = ((const unsigned char *)snapshot->data)[i];
    *cursor++ = hex_digits[(value >> 4) & 0x0F];
    *cursor++ = hex_digits[value & 0x0F];
  }

  *cursor = '\0';
  *output = buffer;
  xanda_clear_error();
  return XANDA_STATUS_OK;
}

static XandaStatus xanda_dev_encode_boundary(const XandaComponent *component, char **output)
{
  const char *name;
  const char *key;
  size_t total_length;
  char *buffer;

  if (!component || !output)
    return xanda_set_errorf(XANDA_STATUS_INVALID_ARGUMENT, "Xanda: no se pudo describir el boundary solicitado.");

  name = xanda_component_name(component);
  key = xanda_component_boundary_key(component);
  total_length = strlen(name) + strlen(key) + 96;
  buffer = (char *)malloc(total_length);
  if (!buffer)
    return xanda_set_errorf(XANDA_STATUS_OUT_OF_MEMORY, "Xanda: no se pudo codificar la metadata del boundary.");

  snprintf(
      buffer,
      total_length,
      "%s|%s|%u|%u|%u|%zu",
      name,
      key,
      xanda_component_id(component),
      xanda_component_render_count(component),
      xanda_component_state_version_internal(component),
      xanda_component_state_size_internal(component));

  *output = buffer;
  xanda_clear_error();
  return XANDA_STATUS_OK;
}

static XandaStatus xanda_dev_decode_snapshot(const char *encoded, const char *expected_component_name, XandaStateSnapshot *snapshot)
{
  const char *first_sep;
  const char *second_sep;
  const char *third_sep;
  size_t name_length;
  char *version_end;
  char *size_end;
  unsigned long version_value;
  unsigned long size_value;
  size_t hex_length;
  unsigned char *data = NULL;
  size_t i;

  if (!encoded || !snapshot)
    return xanda_set_errorf(XANDA_STATUS_INVALID_ARGUMENT, "Xanda: el snapshot dev recibido es invalido.");

  first_sep = strchr(encoded, '|');
  second_sep = first_sep ? strchr(first_sep + 1, '|') : NULL;
  third_sep = second_sep ? strchr(second_sep + 1, '|') : NULL;
  if (!first_sep || !second_sep || !third_sep)
    return xanda_set_errorf(XANDA_STATUS_INVALID_ARGUMENT, "Xanda: el snapshot dev no respeta el formato esperado.");

  name_length = (size_t)(first_sep - encoded);
  if (expected_component_name && expected_component_name[0])
  {
    if (strlen(expected_component_name) != name_length ||
        strncmp(encoded, expected_component_name, name_length) != 0)
      return xanda_set_errorf(XANDA_STATUS_INVALID_ARGUMENT, "Xanda: el snapshot dev pertenece a '%.*s' y no al componente '%s'.", (int)name_length, encoded, expected_component_name);
  }

  version_value = strtoul(first_sep + 1, &version_end, 10);
  if (version_end != second_sep)
    return xanda_set_errorf(XANDA_STATUS_INVALID_ARGUMENT, "Xanda: la version del snapshot dev es invalida.");

  size_value = strtoul(second_sep + 1, &size_end, 10);
  if (size_end != third_sep)
    return xanda_set_errorf(XANDA_STATUS_INVALID_ARGUMENT, "Xanda: el tamano del snapshot dev es invalido.");

  hex_length = strlen(third_sep + 1);
  if (hex_length != (size_t)(size_value * 2))
    return xanda_set_errorf(XANDA_STATUS_INVALID_ARGUMENT, "Xanda: el payload del snapshot dev tiene un tamano inconsistente.");

  if (size_value > 0)
  {
    data = (unsigned char *)malloc((size_t)size_value);
    if (!data)
      return xanda_set_errorf(XANDA_STATUS_OUT_OF_MEMORY, "Xanda: no se pudo reconstruir el snapshot dev.");

    for (i = 0; i < (size_t)size_value; ++i)
    {
      int hi = xanda_dev_hex_value((third_sep + 1)[i * 2]);
      int lo = xanda_dev_hex_value((third_sep + 1)[i * 2 + 1]);
      if (hi < 0 || lo < 0)
      {
        free(data);
        return xanda_set_errorf(XANDA_STATUS_INVALID_ARGUMENT, "Xanda: el payload hexadecimal del snapshot dev es invalido.");
      }

      data[i] = (unsigned char)((hi << 4) | lo);
    }
  }

  xanda_state_snapshot_reset(snapshot);
  snapshot->data = data;
  snapshot->size = (size_t)size_value;
  snapshot->version = (unsigned int)version_value;
  xanda_clear_error();
  return XANDA_STATUS_OK;
}

XandaStatus xanda_dev_restore_pending_component(XandaComponent *component)
{
  XandaStateSnapshot snapshot = {0};
  XandaStatus status;
  char *encoded = NULL;

  if (!component)
    return xanda_set_errorf(XANDA_STATUS_INVALID_ARGUMENT, "Xanda: no se puede restaurar estado dev sobre un componente nulo.");

#ifdef __EMSCRIPTEN__
  encoded = js_dev_consume_pending_snapshot();
#endif
  if (!encoded)
  {
    xanda_clear_error();
    return XANDA_STATUS_OK;
  }

  status = xanda_dev_decode_snapshot(encoded, xanda_component_name(component), &snapshot);
  free(encoded);
  if (status != XANDA_STATUS_OK)
  {
    xanda_state_snapshot_reset(&snapshot);
    return status;
  }

  status = xanda_component_restore(component, &snapshot);
  xanda_state_snapshot_reset(&snapshot);
  return status;
}

const char *xanda_dev_snapshot_root_component(void)
{
  XandaComponent *component = xanda_component_get_root();
  XandaStateSnapshot snapshot = {0};
  XandaStatus status;

  free(g_snapshot_string);
  g_snapshot_string = NULL;

  if (!component)
  {
    xanda_set_errorf(XANDA_STATUS_INVALID_ARGUMENT, "Xanda: no hay un componente raiz disponible para snapshot dev.");
    return NULL;
  }

  status = xanda_component_snapshot(component, &snapshot);
  if (status != XANDA_STATUS_OK)
  {
    xanda_state_snapshot_reset(&snapshot);
    return NULL;
  }

  status = xanda_dev_encode_snapshot(xanda_component_name(component), &snapshot, &g_snapshot_string);
  xanda_state_snapshot_reset(&snapshot);
  if (status != XANDA_STATUS_OK)
    return NULL;

  return g_snapshot_string;
}

const char *xanda_dev_describe_root_boundary(void)
{
  XandaComponent *component = xanda_component_get_root();
  XandaStatus status;

  free(g_boundary_string);
  g_boundary_string = NULL;

  if (!component)
  {
    xanda_set_errorf(XANDA_STATUS_INVALID_ARGUMENT, "Xanda: no hay un boundary raiz disponible.");
    return NULL;
  }

  status = xanda_dev_encode_boundary(component, &g_boundary_string);
  if (status != XANDA_STATUS_OK)
    return NULL;

  return g_boundary_string;
}

const char *xanda_dev_list_boundaries(void)
{
  XandaComponent *component = xanda_component_registry_first();
  size_t total_length = 1;
  XandaStatus status;

  free(g_boundary_list_string);
  g_boundary_list_string = NULL;

  while (component)
  {
    const char *name = xanda_component_name(component);
    const char *key = xanda_component_boundary_key(component);
    total_length += strlen(name) + strlen(key) + 96;
    component = xanda_component_registry_next(component);
  }

  g_boundary_list_string = (char *)calloc(total_length, sizeof(char));
  if (!g_boundary_list_string)
  {
    xanda_set_errorf(XANDA_STATUS_OUT_OF_MEMORY, "Xanda: no se pudo construir la lista de boundaries.");
    return NULL;
  }

  component = xanda_component_registry_first();
  while (component)
  {
    char *encoded = NULL;
    status = xanda_dev_encode_boundary(component, &encoded);
    if (status != XANDA_STATUS_OK)
    {
      free(encoded);
      free(g_boundary_list_string);
      g_boundary_list_string = NULL;
      return NULL;
    }

    if (g_boundary_list_string[0])
      strcat(g_boundary_list_string, "\n");
    strcat(g_boundary_list_string, encoded);
    free(encoded);
    component = xanda_component_registry_next(component);
  }

  xanda_clear_error();
  return g_boundary_list_string;
}
