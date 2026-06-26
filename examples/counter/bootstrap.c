#include "bootstrap.h"

#include "counter.h"

#include <stdio.h>

static XandaApp g_counter_app = {0};

static void on_counter_error(XandaStatus status, const char *message, void *user_data)
{
  (void)status;
  (void)user_data;
  fprintf(stderr, "%s\n", message);
}

XandaStatus counter_bootstrap_init(void)
{
  xanda_set_error_handler(on_counter_error, NULL);

  if (xanda_app_init(&g_counter_app, NULL) != XANDA_STATUS_OK)
    return xanda_last_status();

  return counter_feature_init(&g_counter_app);
}

XandaStatus counter_bootstrap_run(void)
{
  if (counter_feature_restore_pending() != XANDA_STATUS_OK)
    return xanda_last_status();

  return counter_feature_mount();
}
