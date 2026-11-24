#pragma once
#include "cJSON.h"

// Read on the control core; audio publishes only small atomic counters.
bool resource_info(cJSON* object);
