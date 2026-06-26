#include "bootstrap.h"

int main(void)
{
  if (minimal_bootstrap_init() != XANDA_STATUS_OK)
    return 1;

  return minimal_bootstrap_run() == XANDA_STATUS_OK ? 0 : 1;
}
