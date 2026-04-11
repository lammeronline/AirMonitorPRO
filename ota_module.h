#pragma once
#include <WebServer.h>

// ============================================================
//  OTA — upload firmware via HTTP POST /update
//  (registered on the given WebServer instance)
// ============================================================

namespace OTAModule {
    void registerRoutes(WebServer& server);
}
