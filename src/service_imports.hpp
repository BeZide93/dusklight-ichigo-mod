#pragma once

#include "mods/svc/config.h"
#include "mods/svc/host.h"
#include "mods/svc/http.h"
#include "mods/svc/log.h"
#include "mods/svc/overlay.h"
#include "mods/svc/ui.h"

extern const ConfigService* svc_config;
extern const HttpService* svc_http;
extern const LogService* svc_log;
extern const UiService* svc_ui;
extern const OverlayService* svc_overlay;
