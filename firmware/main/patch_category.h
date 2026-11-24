#pragma once

#include <array>
#include <cstdint>
#include <string_view>

enum class PatchCategory : uint8_t { user, bass, lead, pad, keys, pluck, arp, fx, drums, count };
struct PatchCategoryInfo { const char* id; const char* label; };
inline constexpr std::array<PatchCategoryInfo, 9> patch_categories{{
    {"user", "User"}, {"bass", "Bass"}, {"lead", "Lead"},
    {"pad", "Pads"}, {"keys", "Keys"}, {"pluck", "Pluck"}, {"arp", "Arp"},
    {"fx", "FX"}, {"drums", "Drums"},
}};
using PatchCategoryCounts = std::array<unsigned, patch_categories.size()>;

inline constexpr PatchCategory patch_category(std::string_view id)
{
    for (unsigned i = 0; i < patch_categories.size(); ++i)
        if (id == patch_categories[i].id) return static_cast<PatchCategory>(i);
    return PatchCategory::count;
}

// User stays first, including when empty; only populated instrument categories follow.
inline constexpr unsigned category_menu_count(const PatchCategoryCounts& counts)
{
    unsigned count = 1;
    for (unsigned i = 1; i < counts.size(); ++i) count += counts[i] != 0;
    return count;
}

inline constexpr PatchCategory category_menu_at(const PatchCategoryCounts& counts, unsigned position)
{
    if (!position) return PatchCategory::user;
    for (unsigned i = 1; i < counts.size(); ++i)
        if (counts[i] && --position == 0) return static_cast<PatchCategory>(i);
    return PatchCategory::user;
}
static_assert(patch_categories.size() == static_cast<unsigned>(PatchCategory::count));
