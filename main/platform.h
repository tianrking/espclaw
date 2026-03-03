/*
 * ESPClaw - platform.h
 * Conditional compilation macros for ESP32-C3 / ESP32-S3 target adaptation.
 * Every source file should include this header to know platform capabilities.
 */
#ifndef PLATFORM_H
#define PLATFORM_H

#include "sdkconfig.h"

/* -----------------------------------------------------------------------
 * Target detection
 * ----------------------------------------------------------------------- */
#if defined(CONFIG_IDF_TARGET_ESP32S3)
  #define ESPCLAW_TARGET_S3        1
  #define ESPCLAW_TARGET_C3        0
  #define ESPCLAW_DUAL_CORE        1
  #define ESPCLAW_HAS_PSRAM        1
  #define ESPCLAW_HAS_SPI_HAL      1
  #define ESPCLAW_HAS_LITTLEFS     1
  #define ESPCLAW_HAS_WEBSOCKET    1
  #define ESPCLAW_HAS_HEARTBEAT    1
  #define ESPCLAW_HAS_HTTP_PROXY   1
  #define ESPCLAW_HAS_FILE_TOOLS   1
  #define ESPCLAW_FLASH_SIZE_MB    16
  #define ESPCLAW_TARGET_NAME      "esp32s3"
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
  #define ESPCLAW_TARGET_S3        0
  #define ESPCLAW_TARGET_C3        1
  #define ESPCLAW_DUAL_CORE        0
  #define ESPCLAW_HAS_PSRAM        0
  #define ESPCLAW_HAS_SPI_HAL      0
  #define ESPCLAW_HAS_LITTLEFS     0
  #define ESPCLAW_HAS_WEBSOCKET    0
  #define ESPCLAW_HAS_HEARTBEAT    0
  #define ESPCLAW_HAS_HTTP_PROXY   0
  #define ESPCLAW_HAS_FILE_TOOLS   0
  #define ESPCLAW_FLASH_SIZE_MB    4
  #define ESPCLAW_TARGET_NAME      "esp32c3"
#elif defined(CONFIG_IDF_TARGET_ESP32C5)
  #define ESPCLAW_TARGET_S3        0
  #define ESPCLAW_TARGET_C3        0
  #define ESPCLAW_TARGET_C5        1
  #define ESPCLAW_DUAL_CORE        0
  #define ESPCLAW_HAS_PSRAM        0
  #define ESPCLAW_HAS_SPI_HAL      0
  #define ESPCLAW_HAS_LITTLEFS     0
  #define ESPCLAW_HAS_WEBSOCKET    0
  #define ESPCLAW_HAS_HEARTBEAT    0
  #define ESPCLAW_HAS_HTTP_PROXY   0
  #define ESPCLAW_HAS_FILE_TOOLS   0
  #define ESPCLAW_FLASH_SIZE_MB    4
  #define ESPCLAW_TARGET_NAME      "esp32c5"
#else
  #error "Unsupported target. ESPClaw supports ESP32-C3, ESP32-C5, and ESP32-S3."
#endif

/* Give C3/C5 a default so all targets compile cleanly */
#ifndef ESPCLAW_TARGET_C5
  #define ESPCLAW_TARGET_C5        0
#endif

/* -----------------------------------------------------------------------
 * Memory allocation helpers
 * ----------------------------------------------------------------------- */
#if ESPCLAW_HAS_PSRAM
  #include "esp_heap_caps.h"
  #define ESPCLAW_MALLOC(sz)       heap_caps_malloc((sz), MALLOC_CAP_SPIRAM)
  #define ESPCLAW_CALLOC(n, sz)    heap_caps_calloc((n), (sz), MALLOC_CAP_SPIRAM)
  #define ESPCLAW_REALLOC(p, sz)   heap_caps_realloc((p), (sz), MALLOC_CAP_SPIRAM)
  #define ESPCLAW_FREE(ptr)        free(ptr)
#else
  #include <stdlib.h>
  #define ESPCLAW_MALLOC(sz)       malloc(sz)
  #define ESPCLAW_CALLOC(n, sz)    calloc((n), (sz))
  #define ESPCLAW_REALLOC(p, sz)   realloc((p), (sz))
  #define ESPCLAW_FREE(ptr)        free(ptr)
#endif

/* -----------------------------------------------------------------------
 * Task core affinity
 * ----------------------------------------------------------------------- */
#if ESPCLAW_DUAL_CORE
  #define ESPCLAW_CORE_IO          0    /* Core 0: I/O tasks */
  #define ESPCLAW_CORE_AGENT       1    /* Core 1: Agent ReAct loop */
  #define ESPCLAW_CREATE_PINNED(name, fn, stack, param, prio, handle, core) \
      xTaskCreatePinnedToCore(fn, name, stack, param, prio, handle, core)
#else
  #define ESPCLAW_CORE_IO          0
  #define ESPCLAW_CORE_AGENT       0
  #define ESPCLAW_CREATE_PINNED(name, fn, stack, param, prio, handle, core) \
      xTaskCreate(fn, name, stack, param, prio, handle)
#endif

/* -----------------------------------------------------------------------
 * Utility macros
 * ----------------------------------------------------------------------- */
#define ARRAY_SIZE(a)  (sizeof(a) / sizeof((a)[0]))

#endif /* PLATFORM_H */
