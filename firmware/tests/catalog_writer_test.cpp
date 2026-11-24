#include "catalog_writer.h"
#include <array>
#include <cassert>
#include <cstdio>
#include <string>

int main()
{
    std::array<char, 24576> data{};
    auto* metadata = cJSON_CreateObject(); cJSON_AddBoolToObject(metadata, "tb303", true);
    CatalogWriter writer(data.data(), data.size());
    for (unsigned i = 0; i < 34; ++i) {
        auto* type = cJSON_CreateObject();
        cJSON_AddStringToObject(type, "type", ("Type " + std::to_string(i)).c_str());
        auto* parameters = cJSON_AddArrayToObject(type, "parameters");
        for (unsigned j = 0; j < 5; ++j) {
            auto* param = cJSON_CreateObject(); cJSON_AddItemToArray(parameters, param);
            cJSON_AddStringToObject(param, "name", "Parameter"); cJSON_AddNumberToObject(param, "min", 0);
            cJSON_AddNumberToObject(param, "max", 100); cJSON_AddNumberToObject(param, "initial", 50);
        }
        assert(writer.append(type)); cJSON_Delete(type);
    }
    assert(writer.finish(metadata)); cJSON_Delete(metadata);
    assert(writer.size() == std::strlen(data.data()));
    auto* parsed = cJSON_Parse(data.data()); assert(parsed);
    assert(cJSON_GetArraySize(cJSON_GetObjectItem(parsed, "modules")) == 34);
    assert(cJSON_IsTrue(cJSON_GetObjectItem(parsed, "tb303"))); cJSON_Delete(parsed);
    CatalogWriter empty(data.data(), data.size()); metadata = cJSON_CreateObject();
    assert(empty.finish(metadata)); cJSON_Delete(metadata);
    parsed = cJSON_Parse(data.data()); assert(parsed);
    assert(cJSON_GetArraySize(cJSON_GetObjectItem(parsed, "modules")) == 0); cJSON_Delete(parsed);
    char tiny[20]; CatalogWriter overflow(tiny, sizeof(tiny)); metadata = cJSON_CreateObject();
    cJSON_AddStringToObject(metadata, "large", "too much data for this response buffer");
    assert(!overflow.append(metadata)); assert(!overflow.finish(metadata)); cJSON_Delete(metadata);
    std::puts("PASS: bounded incremental catalog, module ownership, merged metadata, empty catalog and overflow rejection");
}
