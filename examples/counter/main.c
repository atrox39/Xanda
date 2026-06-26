#include "bootstrap.h"

int main(void)
{
  if (counter_bootstrap_init() != XANDA_STATUS_OK)
    return 1;

  return counter_bootstrap_run() == XANDA_STATUS_OK ? 0 : 1;
}
