#pragma once

// Necessary conditions for reading remote ability details. Positions, health,
// identity, visibility, modifier masks and public ultimate state are separate.
// A local pawn always needs its weapon/reload observations for output gates.
namespace ability_read_demand {
struct Consumers {
    bool visuals=false,enemy_esp=false,team_esp=false;
    bool enemy_icons=false,team_icons=false;
    bool enemy_actions=false,team_actions=false;
};
constexpr bool required(Consumers consumers,bool local,bool team_known,bool teammate,bool ultimate_fallback=false) {
    if(local)return true;
    if(!team_known)return false;
    const bool display=consumers.visuals&&(teammate?
        consumers.team_esp&&(consumers.team_icons||ultimate_fallback):
        consumers.enemy_esp&&(consumers.enemy_icons||ultimate_fallback));
    return display||(teammate?consumers.team_actions:consumers.enemy_actions);
}
}
