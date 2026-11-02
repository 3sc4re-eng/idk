#include "ballistics_demand.h"
using ballistics_demand::required;
// Stale/default lead toggles must not activate disabled consumers.
static_assert(!required({.aim_prediction=true,.trigger_prediction=true,.magnet_prediction=true}));
// Turning Aim lead off must not leave disabled Trigger/Magnet lead active.
static_assert(!required({.aim_enabled=true,.trigger_prediction=true,.magnet_prediction=true}));
// An unused Magnet does not request prediction from a pure Trigger.
static_assert(!required({.trigger_enabled=true,.magnet_prediction=true}));
static_assert(!required({.magnet_enabled=true,.magnet_prediction=true}));
// Preserve each actual consumer independently, including the standalone HUD.
static_assert(required({.aim_enabled=true,.aim_prediction=true}));
static_assert(required({.trigger_enabled=true,.trigger_prediction=true}));
static_assert(required({.trigger_enabled=true,.magnet_enabled=true,.magnet_prediction=true}));
static_assert(required({.debug_display=true}));
