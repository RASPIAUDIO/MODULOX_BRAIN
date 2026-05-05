#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "esp_err.h"
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_partition.h"
#include "esp_psram.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_ota_ops.h"
#include "esp_private/esp_clk.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#define BENCH_PASS_BYTES (1536U * 1024U)
#define BENCH_REPEAT 8U

static inline uint32_t bench_ccount(void)
{
    uint32_t ccount;
    __asm__ __volatile__("rsr.ccount %0" : "=a"(ccount));
    return ccount;
}

static const char *flash_bus_config(void)
{
#if CONFIG_ESPTOOLPY_OCT_FLASH
    return "octal";
#else
    return "quad";
#endif
}

static const char *flash_sample_config(void)
{
#if CONFIG_ESPTOOLPY_FLASH_SAMPLE_MODE_DTR
    return "DTR";
#else
    return "STR";
#endif
}

static const char *flash_freq_config(void)
{
#if CONFIG_ESPTOOLPY_FLASHFREQ_120M
    return "120MHz";
#elif CONFIG_ESPTOOLPY_FLASHFREQ_80M
    return "80MHz";
#elif CONFIG_ESPTOOLPY_FLASHFREQ_40M
    return "40MHz";
#else
    return "other";
#endif
}

static const char *psram_config(void)
{
#if CONFIG_SPIRAM_MODE_OCT
    return "octal";
#elif CONFIG_SPIRAM_MODE_QUAD
    return "quad";
#else
    return "none";
#endif
}

static const char *psram_freq_config(void)
{
#if CONFIG_SPIRAM_SPEED_120M
    return "120MHz";
#elif CONFIG_SPIRAM_SPEED_80M
    return "80MHz";
#elif CONFIG_SPIRAM_SPEED_40M
    return "40MHz";
#else
    return "none";
#endif
}

static uint32_t checksum_bytes(uint32_t checksum, const uint8_t *data, size_t len)
{
    for (size_t i = 0; i < len; ++i) {
        checksum ^= data[i];
        checksum *= 16777619U;
    }
    return checksum;
}

static void run_flash_read_bench_pass(const esp_partition_t *partition, size_t chunk_size)
{
    uint8_t *buffer = heap_caps_malloc(chunk_size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (buffer == NULL) {
        printf("[idfbench] alloc_failed chunk=%u\n", (unsigned)chunk_size);
        return;
    }

    const size_t pass_bytes = partition->size < BENCH_PASS_BYTES ? partition->size : BENCH_PASS_BYTES;
    const size_t total_bytes = pass_bytes * BENCH_REPEAT;
    uint32_t checksum = 2166136261U;

    const int64_t start_us = esp_timer_get_time();
    const uint32_t start_cycles = bench_ccount();

    for (uint32_t repeat = 0; repeat < BENCH_REPEAT; ++repeat) {
        size_t offset = 0;
        while (offset < pass_bytes) {
            const size_t remaining = pass_bytes - offset;
            const size_t to_read = remaining < chunk_size ? remaining : chunk_size;
            const esp_err_t err = esp_partition_read(partition, offset, buffer, to_read);
            if (err != ESP_OK) {
                printf("[idfbench] read_failed chunk=%u offset=%u err=%s\n",
                       (unsigned)chunk_size,
                       (unsigned)offset,
                       esp_err_to_name(err));
                heap_caps_free(buffer);
                return;
            }
            checksum = checksum_bytes(checksum, buffer, to_read);
            offset += to_read;
        }
    }

    const uint32_t elapsed_cycles = bench_ccount() - start_cycles;
    const int64_t elapsed_us = esp_timer_get_time() - start_us;
    const double seconds = elapsed_us > 0 ? (double)elapsed_us / 1000000.0 : 0.0;
    const double mbps = seconds > 0.0 ? ((double)total_bytes / (1024.0 * 1024.0)) / seconds : 0.0;
    const double cycles_per_byte = total_bytes > 0 ? (double)elapsed_cycles / (double)total_bytes : 0.0;

    printf("[idfbench] chunk=%u pass=%u repeat=%u total=%u us=%" PRId64
           " MBps=%.2f cycles_per_byte=%.2f checksum=0x%08" PRIx32 "\n",
           (unsigned)chunk_size,
           (unsigned)pass_bytes,
           (unsigned)BENCH_REPEAT,
           (unsigned)total_bytes,
           elapsed_us,
           mbps,
           cycles_per_byte,
           checksum);

    heap_caps_free(buffer);
}

static void run_flash_read_bench(void)
{
    const esp_partition_t *partition = esp_ota_get_running_partition();
    if (partition == NULL) {
        printf("[idfbench] running_partition_not_found\n");
        return;
    }

    const size_t pass_bytes = partition->size < BENCH_PASS_BYTES ? partition->size : BENCH_PASS_BYTES;
    printf("[idfbench] start label=%s address=0x%08" PRIx32
           " partition_size=%" PRIu32 " pass=%u repeat=%u\n",
           partition->label,
           partition->address,
           partition->size,
           (unsigned)pass_bytes,
           (unsigned)BENCH_REPEAT);

    run_flash_read_bench_pass(partition, 4096);
    run_flash_read_bench_pass(partition, 32768);
}

void app_main(void)
{
    uint32_t flash_size = 0;
    const esp_err_t flash_size_err = esp_flash_get_size(NULL, &flash_size);
    const size_t psram_size =
#if CONFIG_SPIRAM
        esp_psram_get_size();
#else
        0;
#endif

    printf("[idfbench] idf=%s target=%s cpu=%dMHz flash_size=%" PRIu32
           " psram_size=%u flash_bus=%s flash_sample=%s flash_freq=%s psram_bus=%s psram_freq=%s\n",
           esp_get_idf_version(),
           CONFIG_IDF_TARGET,
           esp_clk_cpu_freq() / 1000000,
           flash_size_err == ESP_OK ? flash_size : 0,
           (unsigned)psram_size,
           flash_bus_config(),
           flash_sample_config(),
           flash_freq_config(),
           psram_config(),
           psram_freq_config());

    while (true) {
        run_flash_read_bench();
        vTaskDelay(pdMS_TO_TICKS(30000));
    }
}
