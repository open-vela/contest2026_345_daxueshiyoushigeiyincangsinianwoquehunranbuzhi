# Contest 2026 team 345 G1 platform smoke test

if(CONFIG_LVX_USE_DEMO_CONTEST2026_345_G1_SMOKE)
  nuttx_add_application(
    NAME g1_smoke
    SRCS g1_smoke_main.c
    STACKSIZE 4096)
endif()
