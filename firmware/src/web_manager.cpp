#include "web_manager.h"
#include "phone_controller.h"
#include "config.h"

#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <SD.h>
#include <Update.h>
#include <ArduinoJson.h>

static WebServer server(80);
static Logger* s_logger = nullptr;
static StatsTracker* s_stats = nullptr;
static PhoneController* s_phone = nullptr;

static void saveSettings();

// --- HTML UI (served from flash, not SD) ------------------------------------

static const char INDEX_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="apple-mobile-web-app-capable" content="yes">
<meta name="apple-mobile-web-app-status-bar-style" content="black-translucent">
<meta name="theme-color" content="#1a1a1a">
<link rel="manifest" href="/manifest.json">
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
.play{color:#6f6;cursor:pointer;font-size:.8em;text-decoration:underline;margin-right:8px}
.play:hover{color:#8f8}
.sz{color:#888;text-align:right;font-size:.8em}
button,input[type=submit]{background:#c41e1e;color:#fff;border:none;padding:8px 16px;border-radius:4px;cursor:pointer;font-size:.9em;margin-top:8px}
button:hover,input[type=submit]:hover{background:#d63030}
button:disabled{background:#555;cursor:wait}
input[type=file]{margin:8px 0;font-size:.9em}
input[type=range]{width:100%;margin:8px 0}
input[type=number]{width:70px;background:#333;color:#e0e0e0;border:1px solid #555;border-radius:4px;padding:4px 8px;font-size:.9em}
.status{color:#888;font-size:.85em;margin-top:8px}
.warn{color:#fc6}
.ok{color:#6f6}
.err{color:#f55}
.row{display:flex;align-items:center;gap:8px;margin:6px 0;font-size:.9em}
.row label{min-width:80px;color:#aaa}
.stat{display:inline-block;background:#333;border-radius:4px;padding:4px 10px;margin:2px;font-size:.85em}
.stat b{color:#fc6}
#prog{width:100%;height:6px;background:#333;border-radius:3px;margin-top:8px;display:none}
#progbar{height:100%;background:#c41e1e;border-radius:3px;width:0%;transition:width .2s}
.topnum{font-family:monospace;color:#6af}
</style>
</head>
<body>
<h1>K6 GPO Exhibit</h1>
<p class="sub">Control Panel, File Manager &amp; Firmware Update</p>

<div class="card">
<h2>Controls</h2>
<div class="row"><label>Volume</label><input type="range" id="vol" min="0" max="21" value="15" oninput="setVol(this.value)"><span id="vollbl">15</span></div>
<div class="row"><label>Bell</label><input type="range" id="bell" min="0" max="255" value="255" oninput="setBell(this.value)"><span id="belllbl">255</span></div>
<div class="row"><label>Auto-ring</label>
<span>Min <input type="number" id="armin" value="5" min="1" max="120"> min</span>
<span>Max <input type="number" id="armax" value="30" min="1" max="120"> min</span>
<button onclick="setAutoRing()" style="margin:0">Set</button>
</div>
<div class="row"><label>Ring count</label><input type="number" id="ringmax" value="10" min="0" max="60" style="width:70px"><button onclick="setRingCount()" style="margin:0">Set</button><span style="color:#888;font-size:.8em;margin-left:4px">(0=unlimited)</span></div>
<div class="row"><label>Mode</label><span id="modelbl">—</span></div>
<div class="row"><label>State</label><span id="statelbl">—</span></div>
<div class="row"><label>Playing</label><span id="playlbl" style="font-family:monospace;color:#6af">—</span></div>
<div class="row"><label>Call timer</label><span id="calltimer" style="font-family:monospace;color:#fc6">—</span></div>
<div style="margin-top:8px">
<button onclick="ringNow()">Ring Now</button>
<button onclick="toggleMode()" id="modebtn">Toggle Mode</button>
</div>
</div>

<div class="card">
<h2>Visitor Statistics</h2>
<div id="statsbox">Loading...</div>
<div style="margin-top:8px"><button onclick="resetStats()" style="background:#555">Reset Stats</button></div>
</div>

<div class="card">
<h2>Files</h2>
<div class="path" id="pathbar">/</div>
<table id="filetbl"><tbody></tbody></table>
<div style="margin-top:12px">
<input type="file" id="upfile" multiple accept=".mp3,.MP3">
<button onclick="upload()" id="upbtn">Upload</button>
<button onclick="mkdirPrompt()">New Folder</button>
</div>
<div class="status" id="upstatus"></div>
</div>

<div class="card">
<h2>Number Aliases</h2>
<p style="font-size:.85em;color:#aaa;margin-bottom:8px">Map dialled numbers to audio file names. E.g. 999 &rarr; emergency plays /numbers/emergency.mp3</p>
<table id="aliastbl"><thead><tr><td style="color:#aaa">Number</td><td style="color:#aaa">Alias</td><td></td></tr></thead><tbody></tbody></table>
<div style="margin-top:8px;display:flex;gap:4px;align-items:center">
<input type="text" id="anew_num" placeholder="Number" style="width:90px;background:#333;color:#e0e0e0;border:1px solid #555;border-radius:4px;padding:4px 8px;font-size:.9em">
<input type="text" id="anew_name" placeholder="Alias name" style="width:140px;background:#333;color:#e0e0e0;border:1px solid #555;border-radius:4px;padding:4px 8px;font-size:.9em">
<button onclick="addAlias()" style="margin:0">Add</button>
</div>
<div class="status" id="aliasstatus"></div>
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
<button onclick="loadLog('system')" style="margin-right:4px">System Log</button>
<button onclick="loadLog('calls')" style="margin-right:4px">Call Log</button>
<button onclick="clearLog()" style="background:#555">Clear</button>
</div>
<pre id="logview" style="background:#111;color:#bfb;padding:12px;border-radius:4px;font-size:.8em;max-height:400px;overflow:auto;white-space:pre-wrap;word-break:break-all">Select a log to view.</pre>
</div>

<div class="card">
<h2>System Status</h2>
<div id="sysinfo" style="font-size:.85em;color:#aaa">Loading...</div>
<div style="margin-top:8px"><button onclick="rebootDevice()" style="background:#555">Restart Device</button></div>
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
        let mp3=f.name.toLowerCase().endsWith('.mp3');
        let acts=mp3?'<span class="play" onclick="preview(\''+cwd+f.name+'\')">play</span> ':'';
        acts+='<span class="del" onclick="del(\''+f.name+'\')">delete</span>';
        tr.innerHTML='<td class="file">'+f.name+'</td><td class="sz">'+sz+'</td><td>'+acts+'</td>';
      }
      tb.appendChild(tr);
    });
  }).catch(e=>{console.error(e)});
}
let previewAudio=null;
function preview(path){
  if(previewAudio){previewAudio.pause();previewAudio=null;}
  previewAudio=new Audio('/api/preview?path='+encodeURIComponent(path));
  previewAudio.play();
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
  let done=0,errs=[];
  Array.from(files).forEach(f=>{
    let fd=new FormData(); fd.append('file',f);
    fetch('/api/upload?path='+encodeURIComponent(cwd),{method:'POST',body:fd})
    .then(r=>r.json()).then(d=>{
      done++;
      if(!d.ok) errs.push(f.name+': '+(d.error||'failed'));
      if(done===files.length){
        btn.disabled=false;
        if(errs.length) st.innerHTML='<span class="err">'+errs.join('<br>')+'</span>';
        else st.innerHTML='<span class="ok">Upload complete</span>';
        document.getElementById('upfile').value='';
        loadFiles();
      }
    }).catch(e=>{done++;btn.disabled=false;st.innerHTML='<span class="err">'+e+'</span>'});
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
function setVol(v){
  document.getElementById('vollbl').textContent=v;
  fetch('/api/volume?v='+v,{method:'POST'});
}
function setBell(v){
  document.getElementById('belllbl').textContent=v;
  fetch('/api/bellvol?v='+v,{method:'POST'});
}
function setRingCount(){
  let n=document.getElementById('ringmax').value;
  fetch('/api/ringcount?n='+n,{method:'POST'});
}
function setAutoRing(){
  let mn=document.getElementById('armin').value;
  let mx=document.getElementById('armax').value;
  fetch('/api/autoring?min='+mn+'&max='+mx,{method:'POST'})
  .then(r=>r.json()).then(d=>{
    if(d.ok) alert('Auto-ring set to '+mn+'-'+mx+' min');
  });
}
function ringNow(){fetch('/api/ring',{method:'POST'})}
function toggleMode(){fetch('/api/mode',{method:'POST'}).then(()=>loadStatus())}
function rebootDevice(){
  if(!confirm('Restart the device? Active calls will be dropped.'))return;
  fetch('/api/reboot',{method:'POST'}).then(()=>{
    document.getElementById('sysinfo').innerHTML='<span class="warn">Rebooting...</span>';
    setTimeout(()=>{location.reload()},8000);
  });
}
function resetStats(){
  if(!confirm('Reset all visitor statistics?'))return;
  fetch('/api/stats/reset',{method:'POST'}).then(()=>loadStats());
}
function loadStatus(){
  fetch('/api/status').then(r=>r.json()).then(d=>{
    document.getElementById('sysinfo').innerHTML=
      'Free heap: '+(d.heap/1024).toFixed(0)+'KB<br>'+
      'SD card: '+(d.sd?'OK':'<span class="err">FAIL</span>')+
      (d.sd_total?' ('+d.sd_used+'MB / '+d.sd_total+'MB)':'')+
      '<br>Uptime: '+Math.floor(d.uptime/3600)+'h '+Math.floor(d.uptime%3600/60)+'m<br>'+
      'Firmware: '+d.firmware;
    document.getElementById('vol').value=d.volume;
    document.getElementById('vollbl').textContent=d.volume;
    document.getElementById('bell').value=d.bell_vol;
    document.getElementById('belllbl').textContent=d.bell_vol;
    if(d.ring_max!==undefined) document.getElementById('ringmax').value=d.ring_max;
    document.getElementById('armin').value=Math.round(d.ar_min/60000);
    document.getElementById('armax').value=Math.round(d.ar_max/60000);
    document.getElementById('modelbl').innerHTML=d.mode=='AUTO'?'<span class="ok">AUTO</span>':'MANUAL';
    document.getElementById('statelbl').textContent=d.state;
    document.getElementById('playlbl').textContent=d.playing||'\u2014';
    let ct=document.getElementById('calltimer');
    if(d.state!=='IDLE'&&d.call_secs>=0){
      let m=Math.floor(d.call_secs/60),s=d.call_secs%60;
      ct.textContent=m+':'+(s<10?'0':'')+s;
    } else { ct.textContent='\u2014'; }
  });
}
function loadStats(){
  fetch('/api/stats').then(r=>r.json()).then(d=>{
    let h='<span class="stat">Incoming: <b>'+d.incoming+'</b></span> '+
          '<span class="stat">Answered: <b>'+d.answered+'</b></span> '+
          '<span class="stat">Outgoing: <b>'+d.outgoing+'</b></span> '+
          '<span class="stat">Not recognised: <b>'+d.not_recognised+'</b></span>';
    if(d.coin_collected>0) h+=' <span class="stat">Coins collected: <b>'+d.coin_collected+'</b></span>';
    if(d.coin_refunded>0) h+=' <span class="stat">Coins refunded: <b>'+d.coin_refunded+'</b></span>';
    if(d.call_count>0){
      let avgM=Math.floor(d.avg_call/60), avgS=d.avg_call%60;
      let lonM=Math.floor(d.longest_call/60), lonS=d.longest_call%60;
      h+='<br><span class="stat">Avg call: <b>'+avgM+'m '+avgS+'s</b></span> ';
      h+='<span class="stat">Longest: <b>'+lonM+'m '+lonS+'s</b></span> ';
      let totM=Math.floor(d.call_seconds/60);
      h+='<span class="stat">Total talk: <b>'+totM+' min</b></span>';
    }
    let uH=Math.floor(d.total_uptime/3600), uM=Math.floor(d.total_uptime%3600/60);
    h+='<br><span class="stat">Total uptime: <b>'+uH+'h '+uM+'m</b></span>';
    if(d.top_numbers&&d.top_numbers.length){
      h+='<br><br><b style="color:#ccc">Most Dialled:</b><br>';
      d.top_numbers.forEach((n,i)=>{
        h+='<span class="stat"><span class="topnum">'+n.number+'</span> &times;'+n.count+'</span> ';
      });
    }
    document.getElementById('statsbox').innerHTML=h;
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
let aliases=[];
function loadAliases(){
  fetch('/api/aliases').then(r=>r.json()).then(d=>{
    aliases=d||[];
    let tb=document.querySelector('#aliastbl tbody');
    tb.innerHTML='';
    aliases.forEach((a,i)=>{
      let tr=document.createElement('tr');
      tr.innerHTML='<td class="topnum">'+a.number+'</td><td>'+a.name+'</td><td><span class="del" onclick="delAlias('+i+')">delete</span></td>';
      tb.appendChild(tr);
    });
  });
}
function addAlias(){
  let num=document.getElementById('anew_num').value.trim();
  let name=document.getElementById('anew_name').value.trim();
  if(!num||!name)return;
  let existing=aliases.findIndex(a=>a.number===num);
  if(existing>=0) aliases[existing].name=name;
  else aliases.push({number:num,name:name});
  saveAliases();
  document.getElementById('anew_num').value='';
  document.getElementById('anew_name').value='';
}
function delAlias(i){
  aliases.splice(i,1);
  saveAliases();
}
function saveAliases(){
  fetch('/api/aliases',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(aliases)})
  .then(r=>r.json()).then(d=>{
    document.getElementById('aliasstatus').innerHTML=d.ok?'<span class="ok">Saved</span>':'<span class="err">'+d.error+'</span>';
    loadAliases();
  });
}
if('serviceWorker' in navigator){navigator.serviceWorker.register('/sw.js').catch(()=>{})}
loadFiles();loadStatus();loadStats();loadAliases();
setInterval(loadStatus,5000);
setInterval(loadStats,30000);
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

// MP3 sync word check: valid MP3 frames start with 0xFF 0xFB/FA/F3/F2
// (11 sync bits set).  We also accept ID3 tags (start with "ID3").
static bool looksLikeMp3(const uint8_t* buf, size_t len) {
    if (len < 3) return false;
    // ID3v2 tag header
    if (buf[0] == 'I' && buf[1] == 'D' && buf[2] == '3') return true;
    // MPEG sync word: first byte 0xFF, second byte has upper 3 bits set (0xE0)
    if (buf[0] == 0xFF && (buf[1] & 0xE0) == 0xE0) return true;
    return false;
}

static bool s_upload_valid = true;
static String s_upload_path;

static void handleUpload() {
    HTTPUpload& upload = server.upload();
    static File uploadFile;

    if (upload.status == UPLOAD_FILE_START) {
        String path = server.arg("path");
        if (!path.endsWith("/")) path += "/";
        path += upload.filename;
        s_upload_path = path;
        s_upload_valid = true;
        Serial.printf("[web] upload: %s\n", path.c_str());
        uploadFile = SD.open(path, FILE_WRITE);
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (uploadFile) {
            // Validate first chunk of .mp3 files.
            if (s_upload_valid && upload.totalSize == 0 &&
                (s_upload_path.endsWith(".mp3") || s_upload_path.endsWith(".MP3"))) {
                if (!looksLikeMp3(upload.buf, upload.currentSize)) {
                    s_upload_valid = false;
                    Serial.printf("[web] REJECTED: not a valid MP3: %s\n", s_upload_path.c_str());
                }
            }
            uploadFile.write(upload.buf, upload.currentSize);
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        if (uploadFile) {
            uploadFile.close();
            if (!s_upload_valid) {
                // Remove invalid file.
                SD.remove(s_upload_path);
                Serial.printf("[web] removed invalid MP3: %s\n", s_upload_path.c_str());
            } else {
                Serial.printf("[web] upload complete: %u bytes\n", upload.totalSize);
            }
        }
    }
}

static void handleUploadComplete() {
    if (!s_upload_valid) {
        server.send(200, "application/json",
                    "{\"ok\":false,\"error\":\"Invalid MP3 file — not a valid audio file\"}");
    } else {
        server.send(200, "application/json", "{\"ok\":true}");
    }
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
    if (!s_phone) {
        server.send(200, "application/json", "{\"error\":\"not ready\"}");
        return;
    }

    String json = "{";
    json += "\"heap\":";     json += String(ESP.getFreeHeap());
    json += ",\"sd\":";      json += SD.cardType() != CARD_NONE ? "true" : "false";
    json += ",\"sd_total\":"; json += String((uint32_t)(SD.totalBytes() / (1024 * 1024)));
    json += ",\"sd_used\":";  json += String((uint32_t)(SD.usedBytes() / (1024 * 1024)));
    json += ",\"uptime\":";  json += String(millis() / 1000);
    json += ",\"firmware\":\"" FIRMWARE_VERSION "\"";
    json += ",\"volume\":";  json += String(s_phone->player().getVolume());
    json += ",\"bell_vol\":"; json += String(s_phone->bell().bellVolume());
    json += ",\"ar_min\":";  json += String(s_phone->autoRingMinMs());
    json += ",\"ar_max\":";  json += String(s_phone->autoRingMaxMs());
    json += ",\"ring_max\":"; json += String(s_phone->maxRingCadences());
    json += ",\"mode\":\"";  json += s_phone->autoRingEnabled() ? "AUTO" : "MANUAL";
    json += "\",\"state\":\""; json += s_phone->stateName();
    json += "\",\"playing\":\"";
    if (s_phone->player().isPlaying()) {
        json += s_phone->player().currentFile();
    }
    json += "\",\"call_secs\":";
    if (s_phone->state() != PhoneState::IDLE) {
        json += String((millis() - s_phone->stateEnterTime()) / 1000);
    } else {
        json += "-1";
    }
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

// --- Control API handlers ---------------------------------------------------

static void handleVolume() {
    if (!s_phone) { server.send(200, "application/json", "{\"ok\":false}"); return; }
    int v = server.arg("v").toInt();
    if (v < 0) v = 0;
    if (v > 21) v = 21;
    s_phone->player().setVolume(v);
    saveSettings();
    if (s_logger) s_logger->systemLog("Volume set to %d via web", v);
    server.send(200, "application/json", "{\"ok\":true}");
}

static void handleBellVolume() {
    if (!s_phone) { server.send(200, "application/json", "{\"ok\":false}"); return; }
    int v = server.arg("v").toInt();
    if (v < 0) v = 0;
    if (v > 255) v = 255;
    s_phone->bell().setBellVolume(v);
    saveSettings();
    if (s_logger) s_logger->systemLog("Bell volume set to %d via web", v);
    server.send(200, "application/json", "{\"ok\":true}");
}

static void handleAutoRing() {
    if (!s_phone) { server.send(200, "application/json", "{\"ok\":false}"); return; }
    unsigned long minMin = server.arg("min").toInt();
    unsigned long maxMin = server.arg("max").toInt();
    if (minMin < 1) minMin = 1;
    if (maxMin < minMin) maxMin = minMin;
    if (maxMin > 120) maxMin = 120;
    s_phone->setAutoRingInterval(minMin * 60000, maxMin * 60000);
    saveSettings();
    if (s_logger) s_logger->systemLog("Auto-ring set to %lu-%lu min via web", minMin, maxMin);
    server.send(200, "application/json", "{\"ok\":true}");
}

static void handleRingNow() {
    if (!s_phone) { server.send(200, "application/json", "{\"ok\":false}"); return; }
    s_phone->ring();
    if (s_logger) s_logger->systemLog("Ring triggered via web");
    server.send(200, "application/json", "{\"ok\":true}");
}

static void handleRingCount() {
    if (!s_phone) { server.send(200, "application/json", "{\"ok\":false}"); return; }
    int n = server.arg("n").toInt();
    if (n < 0) n = 0;
    if (n > 60) n = 60;
    s_phone->setMaxRingCadences(n);
    saveSettings();
    if (s_logger) s_logger->systemLog("Ring count set to %d via web", n);
    server.send(200, "application/json", "{\"ok\":true}");
}

static void handleToggleMode() {
    if (!s_phone) { server.send(200, "application/json", "{\"ok\":false}"); return; }
    s_phone->toggleAutoRing();
    if (s_logger) s_logger->systemLog("Mode toggled to %s via web",
                                       s_phone->autoRingEnabled() ? "AUTO" : "MANUAL");
    server.send(200, "application/json", "{\"ok\":true}");
}

// --- Stats API handlers -----------------------------------------------------

static void handleStats() {
    if (!s_stats) { server.send(200, "application/json", "{}"); return; }

    const CallStats& st = s_stats->stats();
    unsigned long session_secs = millis() / 1000;

    String json = "{";
    json += "\"incoming\":";       json += String(st.total_incoming);
    json += ",\"outgoing\":";      json += String(st.total_outgoing);
    json += ",\"answered\":";      json += String(st.total_answered);
    json += ",\"not_recognised\":"; json += String(st.total_not_recognised);
    json += ",\"coin_collected\":"; json += String(st.total_coin_collected);
    json += ",\"coin_refunded\":";  json += String(st.total_coin_refunded);
    json += ",\"total_uptime\":";   json += String(st.uptime_seconds + session_secs);
    json += ",\"call_seconds\":";  json += String(st.total_call_seconds);
    json += ",\"longest_call\":";  json += String(st.longest_call_seconds);
    json += ",\"avg_call\":";      json += String(s_stats->avgCallSeconds());
    json += ",\"call_count\":";    json += String(st.call_count);

    StatsTracker::NumberEntry top[5];
    int n = s_stats->topNumbers(top, 5);
    json += ",\"top_numbers\":[";
    for (int i = 0; i < n; i++) {
        if (i > 0) json += ",";
        json += "{\"number\":\"";
        json += top[i].number;
        json += "\",\"count\":";
        json += String(top[i].count);
        json += "}";
    }
    json += "]}";

    server.send(200, "application/json", json);
}

static void handleStatsReset() {
    if (s_stats) {
        // Remove stats file and reinitialize.
        SD.remove("/logs/stats.json");
        s_stats->begin();
        if (s_logger) s_logger->systemLog("Stats reset via web");
    }
    server.send(200, "application/json", "{\"ok\":true}");
}

// --- Audio preview handler --------------------------------------------------

static void handlePreview() {
    String path = server.arg("path");
    if (path.length() == 0) {
        server.send(400, "text/plain", "Missing path");
        return;
    }

    File f = SD.open(path, FILE_READ);
    if (!f) {
        server.send(404, "text/plain", "File not found");
        return;
    }

    server.streamFile(f, "audio/mpeg");
    f.close();
}

// --- Settings persistence ---------------------------------------------------

static const char* SETTINGS_FILE = "/system/settings.json";

static void loadSettings() {
    if (!s_phone) return;
    File f = SD.open(SETTINGS_FILE, FILE_READ);
    if (!f) return;

    JsonDocument doc;
    if (deserializeJson(doc, f)) { f.close(); return; }
    f.close();

    if (!doc["volume"].isNull())   s_phone->player().setVolume(doc["volume"].as<uint8_t>());
    if (!doc["bell_vol"].isNull()) s_phone->bell().setBellVolume(doc["bell_vol"].as<uint8_t>());
    if (!doc["ar_min"].isNull() && !doc["ar_max"].isNull()) {
        s_phone->setAutoRingInterval(
            doc["ar_min"].as<unsigned long>(),
            doc["ar_max"].as<unsigned long>());
    }
    if (!doc["ring_max"].isNull()) s_phone->setMaxRingCadences(doc["ring_max"].as<int>());
    Serial.println("[web] settings loaded");
}

static void saveSettings() {
    if (!s_phone) return;
    if (!SD.exists("/system")) SD.mkdir("/system");

    File f = SD.open(SETTINGS_FILE, FILE_WRITE);
    if (!f) return;

    JsonDocument doc;
    doc["volume"]   = s_phone->player().getVolume();
    doc["bell_vol"] = s_phone->bell().bellVolume();
    doc["ar_min"]   = s_phone->autoRingMinMs();
    doc["ar_max"]   = s_phone->autoRingMaxMs();
    doc["ring_max"] = s_phone->maxRingCadences();
    serializeJson(doc, f);
    f.close();
}

// --- Reboot handler ---------------------------------------------------------

static void handleReboot() {
    if (s_logger) s_logger->systemLog("Reboot requested via web");
    if (s_stats) s_stats->save();
    server.send(200, "application/json", "{\"ok\":true}");
    delay(500);
    ESP.restart();
}

// --- Alias API handlers -----------------------------------------------------

static void handleGetAliases() {
    File f = SD.open("/system/aliases.json", FILE_READ);
    if (!f) {
        server.send(200, "application/json", "[]");
        return;
    }
    String content = f.readString();
    f.close();
    server.send(200, "application/json", content);
}

static void handleSaveAliases() {
    if (!s_phone) { server.send(200, "application/json", "{\"ok\":false}"); return; }

    String body = server.arg("plain");
    if (!SD.exists("/system")) SD.mkdir("/system");

    File f = SD.open("/system/aliases.json", FILE_WRITE);
    if (!f) {
        server.send(200, "application/json", "{\"ok\":false,\"error\":\"Cannot write file\"}");
        return;
    }
    f.print(body);
    f.close();

    // Reload aliases in the audio player immediately.
    s_phone->player().loadAliases();
    if (s_logger) s_logger->systemLog("Aliases updated via web");
    server.send(200, "application/json", "{\"ok\":true}");
}

// --- PWA manifest and service worker ----------------------------------------

static const char MANIFEST_JSON[] PROGMEM = R"rawjson(
{
  "name": "K6 GPO Exhibit",
  "short_name": "K6 Exhibit",
  "description": "Control panel for K6 GPO telephone exhibit",
  "start_url": "/",
  "display": "standalone",
  "background_color": "#1a1a1a",
  "theme_color": "#1a1a1a",
  "icons": [{
    "src": "data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 100 100'><rect width='100' height='100' rx='20' fill='%23c41e1e'/><text x='50' y='68' text-anchor='middle' font-size='50' font-family='sans-serif' fill='white'>K6</text></svg>",
    "sizes": "any",
    "type": "image/svg+xml",
    "purpose": "any maskable"
  }]
}
)rawjson";

static void handleManifest() {
    server.send_P(200, "application/json", MANIFEST_JSON);
}

static const char SW_JS[] PROGMEM = R"rawjs(
const CACHE='k6-v1';
const URLS=['/'];
self.addEventListener('install',e=>{
  e.waitUntil(caches.open(CACHE).then(c=>c.addAll(URLS)));
  self.skipWaiting();
});
self.addEventListener('activate',e=>{
  e.waitUntil(caches.keys().then(keys=>
    Promise.all(keys.filter(k=>k!==CACHE).map(k=>caches.delete(k)))
  ));
  self.clients.claim();
});
self.addEventListener('fetch',e=>{
  if(e.request.url.includes('/api/'))return;
  e.respondWith(
    fetch(e.request).then(r=>{
      let c=r.clone();
      caches.open(CACHE).then(cache=>cache.put(e.request,c));
      return r;
    }).catch(()=>caches.match(e.request))
  );
});
)rawjs";

static void handleServiceWorker() {
    server.send_P(200, "application/javascript", SW_JS);
}

// --- Public interface -------------------------------------------------------

void WebManager::begin(Logger& logger, StatsTracker& stats, PhoneController& phone) {
    s_logger = &logger;
    s_stats  = &stats;
    s_phone  = &phone;

    WiFi.mode(WIFI_AP);
    WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASS);
    delay(100);

    IPAddress ip = WiFi.softAPIP();
    Serial.printf("[web] AP \"%s\" started — http://%s/\n",
                  WIFI_AP_SSID, ip.toString().c_str());

    if (MDNS.begin("k6-exhibit")) {
        MDNS.addService("http", "tcp", 80);
        Serial.println("[web] mDNS: http://k6-exhibit.local/");
    }

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
    server.on("/api/volume",      HTTP_POST, handleVolume);
    server.on("/api/bellvol",     HTTP_POST, handleBellVolume);
    server.on("/api/autoring",    HTTP_POST, handleAutoRing);
    server.on("/api/ring",        HTTP_POST, handleRingNow);
    server.on("/api/ringcount",  HTTP_POST, handleRingCount);
    server.on("/api/mode",        HTTP_POST, handleToggleMode);
    server.on("/api/stats",       HTTP_GET,  handleStats);
    server.on("/api/stats/reset", HTTP_POST, handleStatsReset);
    server.on("/api/aliases",     HTTP_GET,  handleGetAliases);
    server.on("/api/aliases",     HTTP_POST, handleSaveAliases);
    server.on("/api/preview",     HTTP_GET,  handlePreview);
    server.on("/api/reboot",      HTTP_POST, handleReboot);
    server.on("/manifest.json",   HTTP_GET,  handleManifest);
    server.on("/sw.js",           HTTP_GET,  handleServiceWorker);

    server.begin();
    active_ = true;
    loadSettings();
    logger.systemLog("Wi-Fi AP started SSID=%s", WIFI_AP_SSID);
    Serial.println("[web] server ready");
}

void WebManager::update() {
    if (active_) server.handleClient();
}
