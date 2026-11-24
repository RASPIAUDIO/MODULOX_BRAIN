#pragma once

#include "cJSON.h"
#include <cstring>

// Serialize one type at a time: the cJSON tree never contains the entire
// growing module catalog in internal SRAM. The response itself is bounded.
class CatalogWriter {
public:
    CatalogWriter(char* data, unsigned capacity) : data_(data), capacity_(capacity)
    {
        constexpr char prefix[] = "{\"modules\":[";
        valid_ = capacity > sizeof(prefix);
        if (valid_) { std::memcpy(data, prefix, sizeof(prefix)); used_ = sizeof(prefix) - 1; }
    }
    bool append(const cJSON* type)
    {
        if (!valid_ || used_ + 2 >= capacity_) return false;
        if (count_++) data_[used_++] = ',';
        if (!cJSON_PrintPreallocated(const_cast<cJSON*>(type), data_ + used_, capacity_ - used_, false)) return valid_ = false;
        used_ += std::strlen(data_ + used_);
        return true;
    }
    bool finish(cJSON* metadata)
    {
        if (!valid_ || !cJSON_IsObject(metadata) || used_ + 4 >= capacity_) return false;
        data_[used_++] = ']';
        if (!metadata->child) { data_[used_++] = '}'; data_[used_] = 0; return true; }
        data_[used_++] = ',';
        if (!cJSON_PrintPreallocated(metadata, data_ + used_, capacity_ - used_, false)) return valid_ = false;
        const unsigned length = std::strlen(data_ + used_);
        // Metadata begins with '{': merge its fields into the enclosing object.
        std::memmove(data_ + used_, data_ + used_ + 1, length);
        used_ += length - 1;
        return true;
    }
    unsigned size() const { return used_; }
private:
    char* data_;
    unsigned capacity_, used_ = 0, count_ = 0;
    bool valid_ = false;
};
