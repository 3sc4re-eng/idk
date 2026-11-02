#include "profile_selection_index.h"
using namespace profile_selection_index;
static_assert(valid_prefix("deadlock")&&valid_prefix("cod")&&valid_prefix("marathon")&&valid_prefix("cs2")&&valid_prefix("overwatch"));
static_assert(!valid_prefix("unknown")&&!valid_prefix("../deadlock")&&!valid_prefix("Deadlock"));
static_assert(valid_name("default")&&valid_name("Test-40_X"));
static_assert(!valid_name("")&&!valid_name("../other")&&!valid_name("a/b")&&!valid_name("a\\b")&&!valid_name("a.json"));
static_assert(valid_name("1234567890123456789012345678901234567890"));
static_assert(!valid_name("12345678901234567890123456789012345678901"));
static_assert(!may_remember(true,true)&&!may_remember(true,false)&&!may_remember(false,false));
static_assert(may_remember(false,true));
static_assert(maximum_bytes==16384);
static_assert(valid_document_bytes("{\"version\":1,\"active_profiles\":{}}"));
static_assert(!valid_document_bytes({}));
static_assert(!valid_document_bytes(std::string_view("{}\0garbage",10)));
// Compile-only fixture: no entry point, filesystem call or executable.
