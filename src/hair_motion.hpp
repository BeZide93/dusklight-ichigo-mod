#pragma once

#include "mods/svc/hook.h"

namespace ichigo {
ModResult install_hair_motion_hooks(ModError* error);
void shutdown_hair_motion();
}
