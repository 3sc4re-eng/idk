#pragma once
#include "world_labels.h"
#include <array>
#include <cstddef>
#include <string_view>

// Semantic groups use ONLY identical labels already verified in world_labels.
// Generic breakable variants are deliberately not guessed from model names.
namespace world_style_catalog {
struct Category { const char* key; const char* label; const char* help; };
inline constexpr std::array<Category,16> categories={{
    {"ability_pickup","Ability pickup","Ability pickups."},
    {"breakable","Breakable","Generic breakable props. Statue, cauldron and heavy-box variants are not identified separately."},
    {"crate","Crate","Item crates. Heavy-box variants are not identified separately."},
    {"currency_bundle","Currency bundle","Grouped currency pickups."},
    {"currency_pickup","Currency pickup","Currency pickups."},
    {"death_pickup","Death pickup","Death pickups; no further subtype is identified."},
    {"drawing","Drawing","Drawing pickups."},
    {"gold_pickup","Gold pickup","Gold pickups. Generic golden-statue props are not identified separately."},
    {"candy","Halloween candy","Halloween-candy pickups."},
    {"health_pickup","Health pickup","Health pickups. Snack variants are not identified separately."},
    {"idol","Idol","Idol pickups."},
    {"item_pickup","Item pickup","Item pickups."},
    {"pickup","Pickup","Generic pickup class; no more specific subtype is inferred."},
    {"powerup_pickup","Power-up pickup","Power-up pickups."},
    {"punchable_troll","Punchable troll","Punchable troll props."},
    {"rejuvenator","Rejuvenator","Rejuvenator pickups, including hero testing."}
}};
inline constexpr std::size_t count=categories.size(), missing=count;
constexpr std::size_t index(const world_labels::Definition& definition) {
    for(std::size_t i=0;i<count;++i)if(std::string_view(categories[i].label)==definition.label)return i;
    return missing;
}
constexpr std::size_t index(std::string_view exact_class) {
    const auto* definition=world_labels::find(exact_class);return definition?index(*definition):missing;
}
constexpr std::size_t class_count(std::size_t category) {
    std::size_t total=0;for(const auto& definition:world_labels::definitions)if(index(definition)==category)++total;return total;
}
constexpr bool complete() {
    for(const auto& definition:world_labels::definitions)if(index(definition)==missing)return false;
    for(std::size_t i=0;i<count;++i){
        if(!class_count(i))return false;
        for(std::size_t j=0;j<i;++j)if(std::string_view(categories[i].key)==categories[j].key||std::string_view(categories[i].label)==categories[j].label)return false;
    }
    return true;
}
static_assert(complete(),"Revalidate semantic world categories when verified labels change");
}
