#include "bootstrap.h"

#include "minimal.h"
#include "xanda/xanda.h"

#include <stdio.h>

static XandaApp g_minimal_app = {0};

static void on_minimal_error(XandaStatus status, const char *message, void *user_data)
{
  (void)status;
  (void)user_data;
  fprintf(stderr, "%s\n", message);
}

XandaStatus minimal_bootstrap_init(void)
{
  xanda_set_error_handler(on_minimal_error, NULL);

  if (xanda_app_init(&g_minimal_app, NULL) != XANDA_STATUS_OK)
    return xanda_last_status();

  return minimal_feature_init(&g_minimal_app);
}

XandaStatus minimal_bootstrap_run(void)
{
  if (minimal_feature_restore_pending() != XANDA_STATUS_OK)
    return xanda_last_status();

  return minimal_feature_mount();
}
