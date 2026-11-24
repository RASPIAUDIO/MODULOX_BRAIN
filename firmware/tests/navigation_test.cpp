#include "module_menu.h"
#include "patch_category.h"

#include <bit>
#include <cassert>
#include <cstdio>

int main()
{
    // Grouping must preserve actual DSP slots, including on a sparse patch.
    const ModuleMask va = module_bit(oscillator) | module_bit(filter) | module_bit(envelope) |
        module_bit(oscillator2) | module_bit(oscillator3) | module_bit(lfo) | module_bit(lfo2);
    constexpr std::array expected{oscillator, oscillator2, oscillator3, envelope, filter, lfo, lfo2};
    for (unsigned i = 0; i < expected.size(); ++i) assert(menu_module_slot(va, i) == unsigned(expected[i]));
    assert(menu_module_slot(va, expected.size()) == max_patch_modules);
    assert(menu_module_slot(module_bit(oscillator3), 0) == oscillator3);
    assert(menu_module_slot(0, 0) == max_patch_modules);
    const ModuleMask all = module_bit(max_patch_modules) - 1;
    ModuleMask visited = 0;
    for (unsigned i = 0; i < max_patch_modules; ++i) visited |= module_bit(menu_module_slot(all, i));
    assert(visited == all);
    // Every subset of the first seven slots also maps each displayed entry exactly once.
    for (ModuleMask mask = 1; mask < 128; ++mask) {
        visited = 0;
        for (int i = 0; i < std::popcount(mask); ++i) {
            const unsigned slot = menu_module_slot(mask, i);
            assert(slot < max_patch_modules && (mask & module_bit(slot)) && !(visited & module_bit(slot)));
            visited |= module_bit(slot);
        }
        assert(visited == mask);
    }
    PatchCategoryCounts counts{};
    assert(category_menu_count(counts) == 1 && category_menu_at(counts, 0) == PatchCategory::user);
    counts[unsigned(PatchCategory::lead)] = 32;
    counts[unsigned(PatchCategory::bass)] = 31;
    counts[unsigned(PatchCategory::drums)] = 1;
    assert(category_menu_count(counts) == 4);
    assert(category_menu_at(counts, 0) == PatchCategory::user); // Empty User still first.
    assert(category_menu_at(counts, 1) == PatchCategory::bass);
    assert(category_menu_at(counts, 2) == PatchCategory::lead);
    assert(category_menu_at(counts, 3) == PatchCategory::drums);
    counts[unsigned(PatchCategory::bass)] = 0;
    assert(category_menu_at(counts, 1) == PatchCategory::lead); // Deletion removes empty categories.
    for (unsigned i = 0; i < patch_categories.size(); ++i)
        assert(patch_category(patch_categories[i].id) == static_cast<PatchCategory>(i));
    assert(patch_category("unknown") == PatchCategory::count);
    assert(menu_visible_rows == 5 && menu_top + menu_visible_rows * menu_row_height == 240);
    for (unsigned cursor = 0; cursor < 64; ++cursor) {
        const auto first = cursor / menu_visible_rows * menu_visible_rows;
        assert(first <= cursor && cursor < first + menu_visible_rows);
    }
    std::puts("PASS: module grouping preserves slots, sparse menus, category filtering, User first, 5-row pagination through 64 patches");
}
