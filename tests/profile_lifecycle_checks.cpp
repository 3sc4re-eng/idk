#include "profile_lifecycle_policy.h"
using namespace profile_lifecycle_policy;
static_assert(same_name("default","Default"));
static_assert(same_name("FAST_40-Ab","fast_40-aB"));
static_assert(!same_name("a","aa"));
static_assert(!may_delete("Default","default"));
static_assert(may_delete("copy","default"));
static_assert(before_load(false,true,false,false)==LoadAction::load_now);
static_assert(before_load(true,true,false,false)==LoadAction::save_current);
static_assert(before_load(true,false,false,false)==LoadAction::confirm_discard);
static_assert(before_load(true,true,true,false)==LoadAction::confirm_discard);
static_assert(before_load(true,true,false,true)==LoadAction::confirm_discard);
static_assert(may_autosave(true,false,false,true));
static_assert(!may_autosave(true,true,false,true));
static_assert(!may_autosave(true,false,true,true));
static_assert(!may_autosave(true,false,false,false));
// Syntax-only translation unit; intentionally has no executable entry point.
