#include "runtime_diagnostics.h"

#include <cstdio>

#include "esp_attr.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace {

struct ResetTrace {
    unsigned magic;
    unsigned patch;
    PatchLoadStage stage;
    DriverOperation operation;
    esp_err_t error;
    unsigned heap_at_load;
    unsigned largest_at_load;
    unsigned controls_at_load;
};

constexpr unsigned trace_magic = 0x4d4f4458;
constexpr auto heap_caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
RTC_NOINIT_ATTR volatile ResetTrace retained;
ResetTrace previous{};

unsigned unused_stack(const char* name)
{
    const TaskHandle_t task = xTaskGetHandle(name);
    return task ? uxTaskGetStackHighWaterMark(task) : 0;
}

}  // namespace

void start_runtime_diagnostics()
{
    if (esp_reset_reason() != ESP_RST_POWERON && retained.magic == trace_magic) {
        previous = {trace_magic, retained.patch, retained.stage, retained.operation, retained.error,
                    retained.heap_at_load, retained.largest_at_load, retained.controls_at_load};
    }
    retained.patch = 0;
    retained.stage = PatchLoadStage::idle;
    retained.operation = DriverOperation::none;
    retained.error = ESP_OK;
    retained.heap_at_load = 0;
    retained.largest_at_load = 0;
    retained.controls_at_load = 0;
    retained.magic = trace_magic;
}

void record_patch_load(unsigned index, PatchLoadStage stage)
{
    retained.patch = index;
    retained.stage = stage;
    if (stage == PatchLoadStage::read) {
        retained.heap_at_load = heap_caps_get_free_size(heap_caps);
        retained.largest_at_load = heap_caps_get_largest_free_block(heap_caps);
        retained.controls_at_load = uxTaskGetStackHighWaterMark(nullptr);
    }
}

void record_driver_error(DriverOperation operation, esp_err_t error)
{
    retained.operation = operation;
    retained.error = error;
}

int format_runtime_diagnostics(char* buffer, unsigned capacity)
{
    return std::snprintf(buffer, capacity,
        "OK reset=%u patch=%u stage=%u driver=%u error=0x%x before_heap=%u before_largest=%u before_controls=%u"
        " heap=%u largest=%u min=%u controls=%u display=%u audio=%u\n",
        unsigned(esp_reset_reason()), previous.patch, unsigned(previous.stage), unsigned(previous.operation),
        unsigned(previous.error), previous.heap_at_load, previous.largest_at_load, previous.controls_at_load,
        unsigned(heap_caps_get_free_size(heap_caps)), unsigned(heap_caps_get_largest_free_block(heap_caps)),
        unsigned(heap_caps_get_minimum_free_size(heap_caps)),
        unused_stack("controls"), unused_stack("display"), unused_stack("audio"));
}
