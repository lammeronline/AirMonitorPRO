#include "ota_module.h"
#include "config.h"
#include <Update.h>

namespace OTAModule {

static const char OTA_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>OTA Update</title>
<style>
body{font-family:sans-serif;background:#111;color:#eee;display:flex;flex-direction:column;align-items:center;padding:40px}
h2{color:#4fc3f7}
form{background:#1e1e1e;padding:24px;border-radius:12px;border:1px solid #333}
input[type=file]{color:#eee;margin:12px 0}
button{background:#4fc3f7;color:#000;border:none;padding:10px 28px;border-radius:8px;font-size:16px;cursor:pointer;margin-top:8px}
button:hover{background:#81d4fa}
#progress{margin-top:16px;font-size:14px;color:#90caf9}
</style></head><body>
<h2>🔧 OTA Firmware Update</h2>
<form id="f" enctype="multipart/form-data">
  <label>Select .bin firmware file:</label><br>
  <input type="file" id="fw" name="firmware" accept=".bin"><br>
  <button onclick="upload(event)">Upload & Flash</button>
</form>
<div id="progress"></div>
<script>
function upload(e){
  e.preventDefault();
  const file=document.getElementById('fw').files[0];
  if(!file){alert('Select a file');return;}
  const fd=new FormData();fd.append('firmware',file);
  const xhr=new XMLHttpRequest();
  xhr.upload.onprogress=function(ev){
    if(ev.lengthComputable){
      document.getElementById('progress').textContent=
        'Uploading: '+Math.round(ev.loaded/ev.total*100)+'%';
    }
  };
  xhr.onload=function(){
    if(xhr.status===200){
      document.getElementById('progress').textContent=
        '✅ Update OK! Rebooting...';
    } else {
      document.getElementById('progress').textContent=
        '❌ Update failed: '+xhr.responseText;
    }
  };
  xhr.open('POST','/do_update');xhr.send(fd);
}
</script></body></html>
)rawliteral";

// -------------------------------------------------------
void registerRoutes(WebServer& server) {

    server.on("/update", HTTP_GET, [&]() {
        server.send_P(200, "text/html", OTA_PAGE);
    });

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
