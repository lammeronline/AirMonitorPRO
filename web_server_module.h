#pragma once
#include "data_types.h"
#include <WebServer.h>

namespace WebServerModule {
    void begin();
    void loop();
    void broadcastData(const SensorData& d, const SystemStatus& s);
    uint8_t connectedClients();
    void registerRoute(const String& path, WebServer::THandlerFunction fn);
    // Send JSON response from within a registerRoute handler
    void sendJSON(const String& json);
}
