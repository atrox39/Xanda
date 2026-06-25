#include "xanda_internal.h"

#include <stdlib.h>
#include <string.h>

void xanda_state_snapshot_reset(XandaStateSnapshot *snapshot)
{
  if (!snapshot)
    return;

  free(snapshot->data);
  snapshot->data = NULL;
  snapshot->size = 0;
  snapshot->version = 0;
}

XandaStatus xanda_state_snapshot_copy(XandaStateSnapshot *snapshot, const void *data, size_t size, unsigned int version)
{
  void *copy = NULL;

  if (!snapshot)
    return xanda_set_errorf(XANDA_STATUS_INVALID_ARGUMENT, "Xanda: el snapshot de estado no puede ser nulo.");

  if (size > 0 && !data)
    return xanda_set_errorf(XANDA_STATUS_INVALID_ARGUMENT, "Xanda: no se puede copiar un snapshot sin datos de origen.");

  if (size > 0)
  {
    copy = malloc(size);
    if (!copy)
      return xanda_set_errorf(XANDA_STATUS_OUT_OF_MEMORY, "Xanda: no se pudo reservar memoria para el snapshot de estado.");

    memcpy(copy, data, size);
  }

  xanda_state_snapshot_reset(snapshot);
  snapshot->data = copy;
  snapshot->size = size;
  snapshot->version = version;
  xanda_clear_error();
  return XANDA_STATUS_OK;
}
