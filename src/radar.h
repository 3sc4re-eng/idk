#pragma once
#include "utils.h"
namespace radar{void start(bool loopback=false);void stop();bool running();const char* current_pin();std::string status_text();int current_port();bool loopback_only();}
