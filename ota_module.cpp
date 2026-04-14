#include "ota_module.h"
#include "config.h"
#include <Update.h>

namespace OTAModule {

// -------------------------------------------------------
// Registers only the firmware-upload endpoint.
// The OTA UI is embedded in the main web interface (web_ui.h).
// -------------------------------------------------------
void registerRoutes(WebServer& server) {

    server.on("/do_update", HTTP_POST,
        // onComplete
        [&]() {
            if (Update.hasError()) {
                server.send(500, "text/plain", Update.errorString());
            } else {
                server.send(200, "text/plain", "OK");
                delay(500);
                ESP.restart();
            }
        },
        // onData (multipart)
        [&]() {
            HTTPUpload& upload = server.upload();
            if (upload.status == UPLOAD_FILE_START) {
                DBGF("[OTA] Start: %s\n", upload.filename.c_str());
                if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
                    DBGLN("[OTA] begin() failed");
                }
            } else if (upload.status == UPLOAD_FILE_WRITE) {
                if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
                    DBGLN("[OTA] write() failed");
                }
            } else if (upload.status == UPLOAD_FILE_END) {
                if (Update.end(true)) {
                    DBGF("[OTA] Done: %u bytes\n", upload.totalSize);
                } else {
                    DBGLN("[OTA] end() failed");
                }
            }
        }
    );
}

} // namespace OTAModule
