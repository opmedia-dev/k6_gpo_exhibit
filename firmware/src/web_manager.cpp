#include "web_manager.h"
#include "config.h"

#include <WiFi.h>
#include <WebServer.h>
#include <SD.h>
#include <Update.h>

static WebServer server(80);
static Logger* s_logger = nullptr;

// --- HTML UI (served from flash, not SD) ------------------------------------

static const char INDEX_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>K6 GPO Exhibit</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:system-ui,sans-serif;background:#1a1a1a;color:#e0e0e0;padding:16px;max-width:640px;margin:0 auto}
h1{color:#c41e1e;margin-bottom:4px;font-size:1.4em}
h2{font-size:1.1em;margin:16px 0 8px;color:#ccc}
.sub{color:#888;font-size:.85em;margin-bottom:16px}
.card{background:#252525;border-radius:8px;padding:16px;margin-bottom:12px}
.path{font-family:monospace;color:#aaa;font-size:.9em;margin-bottom:8px}
.crumb{color:#6af;cursor:pointer;text-decoration:underline}
.crumb:hover{color:#8cf}
table{width:100%;border-collapse:collapse}
td{padding:6px 8px;border-bottom:1px solid #333;font-size:.9em}
td:first-child{font-family:monospace}
.dir{color:#fc6;cursor:pointer}
.dir:hover{text-decoration:underline}
.file{color:#e0e0e0}
.del{color:#f55;cursor:pointer;font-size:.8em;text-decoration:underline}
.del:hover{color:#f88}
.sz{color:#888;text-align:right;font-size:.8em}
button,input[type=submit]{background:#c41e1e;color:#fff;border:none;padding:8px 16px;border-radius:4px;cursor:pointer;font-size:.9em;margin-top:8px}
button:hover,input[type=submit]:hover{background:#d63030}
button:disabled{background:#555;cursor:wait}
input[type=file]{margin:8px 0;font-size:.9em}
.status{color:#888;font-size:.85em;margin-top:8px}
.warn{color:#fc6}
.ok{color:#6f6}
.err{color:#f55}
#prog{width:100%;height:6px;background:#333;border-radius:3px;margin-top:8px;display:none}
#progbar{height:100%;background:#c41e1e;border-radius:3px;width:0%;transition:width .2s}
</style>
</head>
<body>
<h1>K6 GPO Exhibit</h1>
<p class="sub">SD Card File Manager, Logs &amp; Firmware Update</p>

<div class="card">
<h2>Files</h2>
<div class="path" id="pathbar">/</div>
<table id="filetbl"><tbody></tbody></table>
<div style="margin-top:12px">
<input type="file" id="upfile" multiple>
<button onclick="upload()" id="upbtn">Upload</button>
<button onclick="mkdirPrompt()">New Folder</button>
</div>
<div class="status" id="upstatus"></div>
</div>

<div class="card">
<h2>Firmware Update (OTA)</h2>
<p style="font-size:.85em;color:#aaa;margin-bottom:8px">Upload a compiled .bin file to update the firmware. The device will reboot automatically.</p>
<input type="file" id="otafile" accept=".bin">
<button onclick="otaUpload()" id="otabtn">Flash Firmware</button>
<div id="prog"><div id="progbar"></div></div>
<div class="status" id="otastatus"></div>
</div>

<div class="card">
<h2>Logs</h2>
<div style="margin-bottom:8px">
<button onclick="loadLog('system')" id="lbsys" style="margin-right:4px">System Log</button>
<button onclick="loadLog('calls')" id="lbcall" style="margin-right:4px">Call Log</button>
<button onclick="clearLog()" style="background:#555">Clear</button>
</div>
<pre id="logview" style="background:#111;color:#bfb;padding:12px;border-radius:4px;font-size:.8em;max-height:400px;overflow:auto;white-space:pre-wrap;word-break:break-all">Select a log to view.</pre>
</div>

<div class="card">
<h2>System Status</h2>
<div id="sysinfo" style="font-size:.85em;color:#aaa">Loading...</div>
</div>

<script>
let cwd='/';
function nav(p){cwd=p;loadFiles()}
function loadFiles(){
  fetch('/api/files?path='+encodeURIComponent(cwd))
  .then(r=>r.json()).then(d=>{
    let pb=document.getElementById('pathbar');
    let parts=cwd.split('/').filter(Boolean);
    let html='<span class="crumb" onclick="nav(\'/\')">/</span>';
    let acc='/';
    parts.forEach(p=>{acc+= p+'/';html+=' <span class="crumb" onclick="nav(\''+acc+'\')">'+p+'/</span>'});
    pb.innerHTML=html;
    let tb=document.querySelector('#filetbl tbody');
    tb.innerHTML='';
    d.sort((a,b)=>(b.dir-a.dir)||a.name.localeCompare(b.name));
    d.forEach(f=>{
      let tr=document.createElement('tr');
      if(f.dir){
        tr.innerHTML='<td class="dir" onclick="nav(\''+cwd+f.name+'/\')">'+f.name+'/</td><td class="sz">DIR</td><td></td>';
      }else{
        let sz=f.size<1024?f.size+'B':f.size<1048576?(f.size/1024).toFixed(1)+'KB':(f.size/1048576).toFixed(1)+'MB';
        tr.innerHTML='<td class="file">'+f.name+'</td><td class="sz">'+sz+'</td><td><span class="del" onclick="del(\''+f.name+'\')">delete</span></td>';
      }
      tb.appendChild(tr);
    });
  }).catch(e=>{console.error(e)});
}
function del(name){
  if(!confirm('Delete '+name+'?'))return;
  fetch('/api/delete?path='+encodeURIComponent(cwd+name),{method:'POST'})
  .then(r=>r.json()).then(d=>{
    document.getElementById('upstatus').innerHTML=d.ok?'<span class="ok">Deleted</span>':'<span class="err">'+d.error+'</span>';
    loadFiles();
  });
}
function mkdirPrompt(){
  let n=prompt('Folder name:');
  if(!n)return;
  fetch('/api/mkdir?path='+encodeURIComponent(cwd+n),{method:'POST'})
  .then(r=>r.json()).then(d=>{
    document.getElementById('upstatus').innerHTML=d.ok?'<span class="ok">Created</span>':'<span class="err">'+d.error+'</span>';
    loadFiles();
  });
}
function upload(){
  let files=document.getElementById('upfile').files;
  if(!files.length)return;
  let btn=document.getElementById('upbtn');
  let st=document.getElementById('upstatus');
  btn.disabled=true; st.textContent='Uploading...';
  let done=0;
  Array.from(files).forEach(f=>{
    let fd=new FormData(); fd.append('file',f);
    fetch('/api/upload?path='+encodeURIComponent(cwd),{method:'POST',body:fd})
    .then(r=>r.json()).then(d=>{
      done++;
      if(done===files.length){
        btn.disabled=false;
        st.innerHTML=d.ok?'<span class="ok">Upload complete</span>':'<span class="err">'+d.error+'</span>';
        document.getElementById('upfile').value='';
        loadFiles();
      }
    }).catch(e=>{btn.disabled=false;st.innerHTML='<span class="err">'+e+'</span>'});
  });
}
function otaUpload(){
  let f=document.getElementById('otafile').files[0];
  if(!f)return;
  let btn=document.getElementById('otabtn');
  let st=document.getElementById('otastatus');
  let prog=document.getElementById('prog');
  let bar=document.getElementById('progbar');
  btn.disabled=true; prog.style.display='block'; bar.style.width='0%';
  st.innerHTML='<span class="warn">Uploading firmware... do not disconnect.</span>';
  let xhr=new XMLHttpRequest();
  xhr.open('POST','/api/ota');
  xhr.upload.onprogress=function(e){if(e.lengthComputable)bar.style.width=(e.loaded/e.total*100)+'%'};
  xhr.onload=function(){
    let d=JSON.parse(xhr.responseText);
    if(d.ok){
      st.innerHTML='<span class="ok">Firmware updated! Rebooting...</span>';
      setTimeout(()=>{location.reload()},8000);
    }else{
      st.innerHTML='<span class="err">'+d.error+'</span>';
      btn.disabled=false;
    }
  };
  xhr.onerror=function(){st.innerHTML='<span class="err">Upload failed</span>';btn.disabled=false};
  let fd=new FormData(); fd.append('firmware',f);
  xhr.send(fd);
}
function loadStatus(){
  fetch('/api/status').then(r=>r.json()).then(d=>{
    document.getElementById('sysinfo').innerHTML=
      'Free heap: '+(d.heap/1024).toFixed(0)+'KB<br>'+
      'SD card: '+(d.sd?'OK':'<span class="err">FAIL</span>')+'<br>'+
      'Uptime: '+Math.floor(d.uptime/3600)+'h '+Math.floor(d.uptime%3600/60)+'m<br>'+
      'Firmware: '+d.firmware;
  });
}
let curLog='system';
function loadLog(which){
  curLog=which;
  document.getElementById('logview').textContent='Loading...';
  fetch('/api/logs/'+which).then(r=>r.text()).then(t=>{
    let el=document.getElementById('logview');
    el.textContent=t||'(empty)';
    el.scrollTop=el.scrollHeight;
  });
}
function clearLog(){
  if(!confirm('Clear '+curLog+' log?'))return;
  fetch('/api/logs/clear?log='+curLog,{method:'POST'}).then(()=>loadLog(curLog));
}
loadFiles();
loadStatus();
setInterval(loadStatus,10000);
</script>
</body>
</html>
)rawhtml";

// --- API handlers -----------------------------------------------------------

static void handleIndex() {
    server.send_P(200, "text/html", INDEX_HTML);
}

static void handleFileList() {
    String path = server.arg("path");
    if (path.isEmpty()) path = "/";
    if (!path.endsWith("/")) path += "/";

    File dir = SD.open(path);
    if (!dir || !dir.isDirectory()) {
        server.send(200, "application/json", "[]");
        return;
    }

    String json = "[";
    bool first = true;
    File entry;
    while ((entry = dir.openNextFile())) {
        if (!first) json += ",";
        first = false;
        json += "{\"name\":\"";
        json += entry.name();
        json += "\",\"size\":";
        json += String(entry.size());
        json += ",\"dir\":";
        json += entry.isDirectory() ? "true" : "false";
        json += "}";
        entry.close();
    }
    dir.close();
    json += "]";
    server.send(200, "application/json", json);
}

static void handleUpload() {
    HTTPUpload& upload = server.upload();
    static File uploadFile;

    if (upload.status == UPLOAD_FILE_START) {
        String path = server.arg("path");
        if (!path.endsWith("/")) path += "/";
        path += upload.filename;
        Serial.printf("[web] upload: %s\n", path.c_str());
        uploadFile = SD.open(path, FILE_WRITE);
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (uploadFile) {
            uploadFile.write(upload.buf, upload.currentSize);
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        if (uploadFile) {
            uploadFile.close();
            Serial.printf("[web] upload complete: %u bytes\n", upload.totalSize);
        }
    }
}

static void handleUploadComplete() {
    server.send(200, "application/json", "{\"ok\":true}");
}

static void handleDelete() {
    String path = server.arg("path");
    if (path.isEmpty() || path == "/") {
        server.send(200, "application/json", "{\"ok\":false,\"error\":\"Invalid path\"}");
        return;
    }
    if (SD.exists(path)) {
        SD.remove(path);
        server.send(200, "application/json", "{\"ok\":true}");
    } else {
        server.send(200, "application/json", "{\"ok\":false,\"error\":\"File not found\"}");
    }
}

static void handleMkdir() {
    String path = server.arg("path");
    if (path.isEmpty()) {
        server.send(200, "application/json", "{\"ok\":false,\"error\":\"Invalid path\"}");
        return;
    }
    SD.mkdir(path);
    server.send(200, "application/json", "{\"ok\":true}");
}

static void handleStatus() {
    String json = "{";
    json += "\"heap\":";
    json += String(ESP.getFreeHeap());
    json += ",\"sd\":";
    json += SD.cardType() != CARD_NONE ? "true" : "false";
    json += ",\"uptime\":";
    json += String(millis() / 1000);
    json += ",\"firmware\":\"" FIRMWARE_VERSION "\"";
    json += "}";
    server.send(200, "application/json", json);
}

static void handleOTA() {
    server.send(200, "application/json",
                Update.hasError()
                    ? "{\"ok\":false,\"error\":\"Update failed\"}"
                    : "{\"ok\":true}");
    if (!Update.hasError()) {
        delay(500);
        ESP.restart();
    }
}

static void handleOTAUpload() {
    HTTPUpload& upload = server.upload();

    if (upload.status == UPLOAD_FILE_START) {
        Serial.printf("[web] OTA start: %s\n", upload.filename.c_str());
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
            Update.printError(Serial);
        }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
            Update.printError(Serial);
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) {
            Serial.printf("[web] OTA complete: %u bytes\n", upload.totalSize);
        } else {
            Update.printError(Serial);
        }
    }
}

// --- Log API handlers -------------------------------------------------------

static void handleLogSystem() {
    if (!s_logger) { server.send(200, "text/plain", ""); return; }
    size_t len;
    char* buf = s_logger->readSystemLog(&len);
    if (buf) {
        server.send(200, "text/plain", buf);
        free(buf);
    } else {
        server.send(200, "text/plain", "(empty)");
    }
}

static void handleLogCalls() {
    if (!s_logger) { server.send(200, "text/plain", ""); return; }
    size_t len;
    char* buf = s_logger->readCallLog(&len);
    if (buf) {
        server.send(200, "text/plain", buf);
        free(buf);
    } else {
        server.send(200, "text/plain", "(empty)");
    }
}

static void handleLogClear() {
    if (!s_logger) { server.send(200, "application/json", "{\"ok\":true}"); return; }
    String which = server.arg("log");
    if (which == "system" || which == "all") s_logger->clearSystemLog();
    if (which == "calls"  || which == "all") s_logger->clearCallLog();
    server.send(200, "application/json", "{\"ok\":true}");
}

// --- Public interface -------------------------------------------------------

void WebManager::begin(Logger& logger) {
    s_logger = &logger;

    WiFi.mode(WIFI_AP);
    WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASS);
    delay(100);

    IPAddress ip = WiFi.softAPIP();
    Serial.printf("[web] AP \"%s\" started — http://%s/\n",
                  WIFI_AP_SSID, ip.toString().c_str());

    server.on("/",                HTTP_GET,  handleIndex);
    server.on("/api/files",       HTTP_GET,  handleFileList);
    server.on("/api/upload",      HTTP_POST, handleUploadComplete, handleUpload);
    server.on("/api/delete",      HTTP_POST, handleDelete);
    server.on("/api/mkdir",       HTTP_POST, handleMkdir);
    server.on("/api/status",      HTTP_GET,  handleStatus);
    server.on("/api/ota",         HTTP_POST, handleOTA, handleOTAUpload);
    server.on("/api/logs/system", HTTP_GET,  handleLogSystem);
    server.on("/api/logs/calls",  HTTP_GET,  handleLogCalls);
    server.on("/api/logs/clear",  HTTP_POST, handleLogClear);

    server.begin();
    active_ = true;
    logger.systemLog("Wi-Fi AP started SSID=%s", WIFI_AP_SSID);
    Serial.println("[web] server ready");
}

void WebManager::update() {
    if (active_) server.handleClient();
}
