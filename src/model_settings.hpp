#pragma once
#include "mods/svc/ui.h"

namespace ichigo {
ModResult init_model_settings();
ModResult build_model_settings(ModContext* ctx, UiElementHandle pane);
void shutdown_model_settings();
bool face_overlay_enabled(const char* group);
}
