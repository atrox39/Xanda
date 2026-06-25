#include "counter.h"

#include <stdio.h>

static XandaApp g_app;

static void on_xanda_error(XandaStatus status, const char *message, void *user_data)
{
  (void)status;
  (void)user_data;
  fprintf(stderr, "%s\n", message);
}

int main(void)
{
  xanda_set_error_handler(on_xanda_error, NULL);

  if (xanda_app_init(&g_app, NULL) != XANDA_STATUS_OK)
    return 1;

  if (counter_init(&g_app) != XANDA_STATUS_OK)
    return 1;

  if (counter_restore_pending() != XANDA_STATUS_OK)
    return 1;

  return counter_mount() == XANDA_STATUS_OK ? 0 : 1;
}
