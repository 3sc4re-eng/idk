#pragma once
#include <cstdint>
#include <string>
namespace collision {
// A map name alone is never enough to select geometry for another game/build.
struct GameMapKey {
    std::string game_id,map_id;
    std::uint32_t build_revision=0;
    bool operator==(const GameMapKey&)const=default;
};
struct MapAsset {const char* path;std::uint32_t asset_revision;};
inline const MapAsset* find_map_asset(const GameMapKey& key){
    struct Registered {const char* game;const char* map;std::uint32_t build;MapAsset asset;};
    // These are the only actual native collision assets currently distributed.
    // Add another game only with real geometry and independently verified metadata.
    static constexpr Registered assets[]={
        {"deadlock","dl_midtown",11193905,{"assets/collision/dl_midtown.acecol",11193905}},
        {"deadlock","hero_testing",11193905,{"assets/collision/hero_testing.acecol",11193905}}
    };
    for(const auto& asset:assets)if(key.game_id==asset.game&&key.map_id==asset.map&&key.build_revision==asset.build)return &asset.asset;
    return nullptr;
}
}
