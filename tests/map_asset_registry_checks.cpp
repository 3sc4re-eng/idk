// Source-only behavioral checks; no main, device calls or file loading.
#include "map_asset_registry.h"
#include <cassert>
void map_asset_registry_checks(){
    using namespace collision;
    assert(find_map_asset({"deadlock","dl_midtown",11193905}));
    assert(find_map_asset({"deadlock","hero_testing",11193905}));
    assert(!find_map_asset({"overwatch","dl_midtown",11193905}));
    assert(!find_map_asset({"cs2","hero_testing",11193905}));
    assert(!find_map_asset({"deadlock","dl_midtown",0}));
    assert(!find_map_asset({"deadlock","dl_midtown",11193906}));
    assert(!find_map_asset({"deadlock","dl_hideout",11193905}));
    assert(!find_map_asset({"","dl_midtown",11193905}));
}
