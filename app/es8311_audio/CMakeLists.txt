# Contest 2026 team 345 ES8311 I2S Audio test utility

if(CONFIG_LVX_USE_DEMO_CONTEST2026_345_ES8311_AUDIO)
  nuttx_add_application(
    NAME
    es8311_audio
    SRCS
    es8311_audio_main.c
    STACKSIZE
    4096
    PRIORITY
    ${CONFIG_DEFAULT_TASK_PRIORITY})
endif()
