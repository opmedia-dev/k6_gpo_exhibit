#include "web_manager.h"
#include "phone_controller.h"
#include "config.h"

#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <SD.h>
#include <Update.h>
#include <esp_ota_ops.h>
#include <esp_task_wdt.h>
#include <ArduinoJson.h>

static WebServer server(80);
static Logger* s_logger = nullptr;
static StatsTracker* s_stats = nullptr;
static PhoneController* s_phone = nullptr;

static void saveSettings();

// --- Wi-Fi configuration ----------------------------------------------------
// s_wifi_sta = requested mode from settings (false = host own AP, true = join
// an existing network). s_ap_active reflects the mode actually running after
// boot (a failed STA join falls back to AP, so these can differ).
static bool   s_wifi_sta   = false;
static String s_sta_ssid;
static String s_sta_pass;
static bool   s_ap_active  = true;

// Current portal IP for captive-portal redirects (AP or STA address).
static IPAddress currentIP() { return s_ap_active ? WiFi.softAPIP() : WiFi.localIP(); }

// Minimal JSON string escaping (quotes / backslashes) for user-entered values
// such as Wi-Fi SSIDs.
static String jsonEscape(const String& in) {
    String out;
    for (size_t i = 0; i < in.length(); i++) {
        unsigned char c = (unsigned char)in[i];
        if (c == '"' || c == '\\') { out += '\\'; out += (char)c; }
        else if (c >= 0x20)        { out += (char)c; }
    }
    return out;
}

// Audio-probe instrumentation (defined in audio_player.cpp).
extern volatile bool g_audio_probe;
void audio_probe_reset();
void audio_probe_result(int32_t* peak, float* rms, uint32_t* count);

// Rotary self-confirm mode (defined in main.cpp): echoes each decoded digit
// on the panel lamp so dialling can be verified with no laptop attached.
extern volatile bool g_dial_confirm;

// Captured on-hook level between the two calibration steps (web flow).
static int s_cal_onhook = -1;

// Run the one-shot bring-up self-test and return a plain-text checklist.
static String buildSelfTest(PhoneController& p) {
    String o = "=== K6 bring-up self-test ===\n";

    o += String("[") + (p.player().sdReady() ? "PASS" : "FAIL") + "] SD card mounted\n";

    int raw = p.line().readAveraged(64);
    bool lineOk = raw < p.line().thresholdOn();  // on-hook should read low
    o += String("[") + (lineOk ? "PASS" : "WARN") + "] line-sense raw=" + raw;
    o += "  (on>=" + String(p.line().thresholdOn()) + " off<" + String(p.line().thresholdOff()) + ")";
    if (!lineOk) o += "  <- high: handset off-hook, line shorted, or needs calibrate";
    o += "\n";

    p.bell().strike(150);
    o += "[ -- ] bell strike fired (listen/feel for a tick; needs 48 V for sound)\n";

    bool tone = p.player().sdReady() && p.player().playTestTone(1000, 2);
    o += String("[") + (tone ? "PASS" : "FAIL") + "] 1 kHz test tone -> earpiece (2 s)\n";

    // Buttons are active-low; report their instantaneous state to catch a
    // stuck/shorted button (reads DOWN when nothing is pressed).
    o += "buttons now: ";
    o += "RING=" + String(digitalRead(PIN_BTN_RING)   ? "up" : "DOWN") + "  ";
    o += "CANCEL=" + String(digitalRead(PIN_BTN_CANCEL) ? "up" : "DOWN") + "  ";
    o += "RESET=" + String(digitalRead(PIN_BTN_RESET)  ? "up" : "DOWN") + "  ";
    o += "MODE=" + String(digitalRead(PIN_BTN_MODE)    ? "up" : "DOWN") + "\n";

    o += "coinbox: " + String(p.coinBox().isInstalled() ? "detected" : "none") + "\n";
    o += "heap free: " + String(ESP.getFreeHeap()) + " bytes\n";
    o += "=== end ===";

    // Persist the result so it survives a reboot and drift can be reviewed.
    if (s_stats) s_stats->recordSelfTest(o.c_str(), millis() / 1000);
    return o;
}

// Play a fixed 1 kHz tone and measure the peak/RMS of the samples fed to I2S.
static String buildProbe(PhoneController& p) {
    if (!p.player().sdReady()) return "PROBE: SD not available";

    audio_probe_reset();
    g_audio_probe = true;
    if (!p.player().playTestTone(1000, 3)) {
        g_audio_probe = false;
        return "PROBE: could not start test tone";
    }

    // Feed I2S for ~2 s while the probe accumulates.
    unsigned long start = millis();
    while (millis() - start < 2000) {
        p.player().update();
        esp_task_wdt_reset();
        delay(2);
    }
    g_audio_probe = false;
    p.player().stop();

    int32_t peak = 0; float rms = 0; uint32_t cnt = 0;
    audio_probe_result(&peak, &rms, &cnt);

    float peakPct = peak * 100.0f / 32767.0f;
    float rmsPct  = rms  * 100.0f / 32767.0f;
    // For a clean sine, peak/RMS = sqrt(2) ~ 1.414. Big deviations hint at
    // clipping (ratio -> 1.0) or a mostly-silent/distorted feed.
    float crest = (rms > 1.0f) ? (peak / rms) : 0.0f;

    char buf[256];
    snprintf(buf, sizeof(buf),
             "=== audio probe (1 kHz, digital side) ===\n"
             "samples=%lu\n"
             "peak=%ld/32767 (%.1f%%)\n"
             "rms=%.0f/32767 (%.2f%%)\n"
             "crest(peak/rms)=%.2f (sine~1.41)\n"
             "note: measures the DIGITAL feed. If this is clean but the\n"
             "earpiece is distorted, the fault is analog (amp/transformer).",
             (unsigned long)cnt, (long)peak, peakPct, rms, rmsPct, crest);
    return String(buf);
}

// --- HTML UI (served from flash, not SD) ------------------------------------

static const char INDEX_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="apple-mobile-web-app-capable" content="yes">
<meta name="apple-mobile-web-app-status-bar-style" content="black-translucent">
<meta name="theme-color" content="#1a1a1a" id="themecolor">
<link rel="manifest" href="/manifest.json">
<title>K6 GPO Exhibit</title>
<style>
:root{
--bg:#1a1a1a;--bg2:#252525;--bg3:#333;--bg4:#111;--bg5:#1e1e1e;
--fg:#e0e0e0;--fg2:#ccc;--fg3:#aaa;--fg4:#999;--fg5:#888;
--border:#333;--border2:#444;--border3:#555;
--card-border:#333;
--input-bg:#333;--input-fg:#e0e0e0;--input-border:#555;
--btn2:#444;--btn2h:#555;--btn-dis:#555;--btn-dis-fg:#999;
--dir:#fc6;--file:#e0e0e0;--link:#6af;--linkh:#8cf;
--play:#6f6;--playh:#8f8;--del:#f55;--delh:#f88;
--ok:#6f6;--warn:#fc6;--err:#f55;--stat-b:#fc6;
--badge-green-bg:#1a3a1a;--badge-green-fg:#6f6;--badge-green-bd:#3a5a3a;
--badge-amber-bg:#3a2a0a;--badge-amber-fg:#fc6;--badge-amber-bd:#5a4a1a;
--badge-red-bg:#3a1a1a;--badge-red-fg:#f55;--badge-red-bd:#5a2a2a;
--log-bg:#111;--log-fg:#bfb;
--nav-bg:#111;--nav-active:#c41e1e;
}
.light{
--bg:#f5f5f5;--bg2:#fff;--bg3:#e8e8e8;--bg4:#f0f0f0;--bg5:#f8f8f8;
--fg:#222;--fg2:#333;--fg3:#555;--fg4:#666;--fg5:#777;
--border:#ddd;--border2:#ccc;--border3:#bbb;
--card-border:#ddd;
--input-bg:#fff;--input-fg:#222;--input-border:#ccc;
--btn2:#e0e0e0;--btn2h:#d0d0d0;--btn-dis:#ccc;--btn-dis-fg:#999;
--dir:#b8860b;--file:#222;--link:#0066cc;--linkh:#0044aa;
--play:#228b22;--playh:#196619;--del:#cc0000;--delh:#990000;
--ok:#228b22;--warn:#cc8800;--err:#cc0000;--stat-b:#b8860b;
--badge-green-bg:#e6f4e6;--badge-green-fg:#228b22;--badge-green-bd:#b3d9b3;
--badge-amber-bg:#fff3cd;--badge-amber-fg:#856404;--badge-amber-bd:#ffc107;
--badge-red-bg:#f8d7da;--badge-red-fg:#721c24;--badge-red-bd:#f5c6cb;
--log-bg:#f8f8f0;--log-fg:#333;
--nav-bg:#f0f0f0;--nav-active:#c41e1e;
}
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:system-ui,-apple-system,sans-serif;background:var(--bg);color:var(--fg);padding:0;max-width:640px;margin:0 auto;transition:background .3s,color .3s}
h1{color:#c41e1e;margin-bottom:2px;font-size:1.5em}
h2{font-size:1.1em;margin:0 0 4px;color:var(--fg2)}
.sub{color:var(--fg4);font-size:.85em;margin-bottom:0}
.card{background:var(--bg2);border-radius:10px;padding:16px;margin-bottom:14px;border:1px solid var(--card-border);transition:background .3s}
.hint{color:var(--fg4);font-size:.8em;margin:2px 0 8px;line-height:1.3}
.path{font-family:monospace;color:var(--fg3);font-size:.9em;margin-bottom:8px}
.crumb{color:var(--link);cursor:pointer;text-decoration:underline}
.crumb:hover{color:var(--linkh)}
table{width:100%;border-collapse:collapse}
td{padding:8px;border-bottom:1px solid var(--border);font-size:.9em}
td:first-child{font-family:monospace}
.dir{color:var(--dir);cursor:pointer}
.dir:hover{text-decoration:underline}
.file{color:var(--file)}
.del{color:var(--del);cursor:pointer;font-size:.85em;text-decoration:underline}
.del:hover{color:var(--delh)}
.play{color:var(--play);cursor:pointer;font-size:.85em;text-decoration:underline;margin-right:10px}
.play:hover{color:var(--playh)}
.sz{color:var(--fg5);text-align:right;font-size:.8em}
button,input[type=submit]{background:#c41e1e;color:#fff;border:none;padding:10px 18px;border-radius:6px;cursor:pointer;font-size:.9em;margin-top:8px;font-weight:500}
button:hover,input[type=submit]:hover{background:#d63030}
button:disabled{background:var(--btn-dis);cursor:wait;color:var(--btn-dis-fg)}
.btn-secondary{background:var(--btn2);color:var(--fg)}
.btn-secondary:hover{background:var(--btn2h)}
.btn-danger{background:#8b0000}
.btn-danger:hover{background:#a00}
.btn-theme{background:var(--bg3);color:var(--fg);border:1px solid var(--border2);padding:6px 14px;border-radius:20px;font-size:.8em;margin:0;cursor:pointer}
.btn-theme:hover{background:var(--btn2h)}
input[type=file]{margin:8px 0;font-size:.9em;color:var(--fg)}
input[type=range]{width:100%;margin:8px 0;accent-color:#c41e1e}
input[type=number],input[type=text],select{background:var(--input-bg);color:var(--input-fg);border:1px solid var(--input-border);border-radius:6px;padding:6px 10px;font-size:.9em}
input[type=number]{width:70px}
input[type=text]{width:140px}
.status{color:var(--fg5);font-size:.85em;margin-top:8px}
.warn{color:var(--warn)}
.ok{color:var(--ok)}
.err{color:var(--err)}
.field{margin:10px 0}
.field-label{color:var(--fg2);font-size:.9em;font-weight:500;margin-bottom:4px}
.field-row{display:flex;align-items:center;gap:8px;flex-wrap:wrap}
.field-hint{color:var(--fg5);font-size:.75em;margin-top:2px}
.stat{display:inline-block;background:var(--bg3);border-radius:6px;padding:6px 12px;margin:3px;font-size:.85em}
.stat b{color:var(--stat-b)}
.stat-label{color:var(--fg3);font-size:.75em;display:block;margin-bottom:1px}
#prog{width:100%;height:8px;background:var(--bg3);border-radius:4px;margin-top:8px;display:none}
#progbar{height:100%;background:#c41e1e;border-radius:4px;width:0%;transition:width .2s}
.topnum{font-family:monospace;color:var(--link)}
.live-status{display:flex;gap:16px;flex-wrap:wrap;margin:8px 0}
.live-item{font-size:.9em}
.live-item .label{color:var(--fg5);font-size:.8em}
.live-item .value{font-weight:500}
.section-icon{font-size:1.2em;margin-right:6px;vertical-align:middle}
.badge{display:inline-block;padding:3px 10px;border-radius:12px;font-size:.8em;font-weight:600}
.badge-green{background:var(--badge-green-bg);color:var(--badge-green-fg);border:1px solid var(--badge-green-bd)}
.badge-amber{background:var(--badge-amber-bg);color:var(--badge-amber-fg);border:1px solid var(--badge-amber-bd)}
.badge-red{background:var(--badge-red-bg);color:var(--badge-red-fg);border:1px solid var(--badge-red-bd)}
.divider{border:none;border-top:1px solid var(--border);margin:12px 0}
.header-row{display:flex;justify-content:space-between;align-items:center;padding:16px 16px 12px}
/* --- Tab navigation --- */
.tab-nav{display:flex;background:var(--nav-bg);border-bottom:2px solid var(--border);position:sticky;top:0;z-index:100}
.tab-nav button{flex:1;background:none;color:var(--fg4);border:none;padding:12px 8px;margin:0;border-radius:0;font-size:.85em;font-weight:500;cursor:pointer;border-bottom:3px solid transparent;transition:color .2s,border-color .2s}
.tab-nav button:hover{color:var(--fg);background:none}
.tab-nav button.active{color:var(--nav-active);border-bottom-color:var(--nav-active);font-weight:700}
.tab-content{display:none;padding:16px}
.tab-content.active{display:block}
/* --- Burger menu (mobile) --- */
.burger{display:none;background:none;border:none;color:var(--fg);font-size:1.6em;padding:4px 8px;margin:0;cursor:pointer;line-height:1}
.burger:hover{color:#c41e1e;background:none}
.nav-backdrop{display:none}
@media(max-width:520px){
  .tab-nav{display:none}
  .tab-nav.open{display:flex;flex-direction:column;position:absolute;top:60px;right:10px;min-width:180px;background:var(--bg2);border:1px solid var(--border2);border-radius:10px;box-shadow:0 8px 24px rgba(0,0,0,.45);z-index:1000;padding:6px;gap:2px}
  .tab-nav.open button{font-size:1em;padding:11px 16px;width:100%;border-radius:6px;border-bottom:none;text-align:left}
  .tab-nav.open button.active{background:var(--nav-active);color:#fff}
  .burger{display:block}
  .nav-backdrop.open{display:block;position:fixed;inset:0;z-index:999;background:transparent}
}
</style>
</head>
<body>
<div class="header-row">
<div><h1>K6 GPO Exhibit</h1><p class="sub">Telephone Management System</p></div>
<div style="display:flex;gap:8px;align-items:center">
<button class="btn-theme" id="themebtn" onclick="toggleTheme()">Light Mode</button>
<button class="burger" id="burgerbtn" onclick="toggleMenu()">&#9776;</button>
</div>
</div>

<nav class="tab-nav" id="tabnav">
<button class="active" onclick="switchTab('overview',this)">Overview</button>
<button onclick="switchTab('stats',this)">Stats</button>
<button onclick="switchTab('diagnostics',this)">Diagnostics</button>
<button onclick="switchTab('terminal',this)">Terminal</button>
<button onclick="switchTab('settings',this)">Settings</button>
</nav>
<div class="nav-backdrop" id="navbackdrop" onclick="closeMenu()"></div>

<!-- ===== OVERVIEW TAB ===== -->
<div class="tab-content active" id="tab-overview">

<div class="card" style="border-color:var(--border2)">
<h2><span class="section-icon">&#128222;</span> Phone Status</h2>
<p class="hint">Live information about the telephone — updates every 5 seconds.</p>
<div class="live-status">
<div class="live-item"><div class="label">Current Mode</div><div class="value"><span id="modelbl" class="badge badge-green">AUTOMATIC</span></div></div>
<div class="live-item"><div class="label">Phone State</div><div class="value" id="statelbl">Waiting for visitors</div></div>
<div class="live-item"><div class="label">Now Playing</div><div class="value" id="playlbl" style="font-family:monospace;color:var(--link)">Nothing</div></div>
<div class="live-item"><div class="label">Call Duration</div><div class="value" id="calltimer" style="font-family:monospace;color:var(--warn)">&mdash;</div></div>
</div>
<div id="alertbanner" style="display:none;background:#c41e1e;color:#fff;padding:10px 14px;border-radius:6px;margin-top:10px;font-weight:600;font-size:.9em">&#9888; No visitor activity detected for a while &mdash; please check the exhibit is working.</div>
<div style="margin-top:10px;display:flex;gap:8px;flex-wrap:wrap">
<button onclick="ringNow()">Make Phone Ring</button>
<button onclick="toggleMode()" id="modebtn" class="btn-secondary">Switch to Manual Mode</button>
</div>
</div>

<div class="card">
<h2><span class="section-icon">&#128200;</span> Session Summary</h2>
<p class="hint">Activity since the exhibit was powered on.</p>
<div id="sessionbox" style="display:flex;flex-wrap:wrap;gap:4px"></div>
</div>

<div class="card">
<h2><span class="section-icon">&#128994;</span> Quick Health</h2>
<p class="hint">At-a-glance system status.</p>
<div id="healthbadge" style="margin-bottom:8px"><span class="badge badge-amber">Checking…</span></div>
<div id="healthbox" style="font-size:.85em;line-height:1.8">Loading...</div>
</div>

</div>

<!-- ===== STATS TAB ===== -->
<div class="tab-content" id="tab-stats">

<div class="card">
<h2><span class="section-icon">&#128202;</span> Visitor Activity</h2>
<p class="hint">How visitors have been interacting with the telephone.</p>
<div id="statsbox">Loading...</div>
<div style="margin-top:8px"><button onclick="resetStats()" class="btn-danger">Clear All Statistics</button></div>
</div>

<div class="card">
<h2><span class="section-icon">&#128270;</span> Numbers Tried</h2>
<p class="hint">Numbers visitors tried to dial that aren't in the directory. Use this to decide what content to add next.</p>
<div id="discbox">Loading...</div>
<div style="margin-top:8px"><button onclick="clearDiscovery()" class="btn-danger">Clear All</button></div>
</div>

<div class="card">
<h2><span class="section-icon">&#128214;</span> Number Directory</h2>
<p class="hint">Link dialled numbers to audio files. Dialling a number listed here plays the matching file from <b>/numbers/</b>.</p>
<table id="aliastbl"><thead><tr><td style="color:var(--fg3)">Dial Number</td><td style="color:var(--fg3)">Plays File</td><td></td></tr></thead><tbody></tbody></table>
<div style="margin-top:10px">
<div style="color:var(--fg2);font-size:.85em;margin-bottom:6px">Add a new number:</div>
<div style="display:flex;gap:6px;align-items:center;flex-wrap:wrap">
<input type="text" id="anew_num" placeholder="e.g. 999" style="width:80px">
<input type="text" id="anew_name" placeholder="e.g. emergency">
<button onclick="addAlias()" style="margin:0">Add</button>
</div>
</div>
<div class="status" id="aliasstatus"></div>
</div>

</div>

<!-- ===== DIAGNOSTICS TAB ===== -->
<div class="tab-content" id="tab-diagnostics">

<div class="card">
<h2><span class="section-icon">&#9888;</span> Error Log</h2>
<p class="hint">Hardware and system errors since last power-on. Bell faults, line anomalies, and SD card failures appear here.</p>
<div id="errorbox" style="font-size:.85em;color:var(--fg3)">Loading...</div>
</div>

<div class="card">
<h2><span class="section-icon">&#128196;</span> Activity Log</h2>
<p class="hint">View a record of what the telephone has been doing.</p>
<div style="margin-bottom:8px;display:flex;gap:6px;flex-wrap:wrap">
<button onclick="loadLog('system')">System Events</button>
<button onclick="loadLog('calls')" class="btn-secondary">Call History</button>
<button onclick="clearLog()" class="btn-danger">Clear Log</button>
</div>
<pre id="logview" style="background:var(--log-bg);color:var(--log-fg);padding:12px;border-radius:6px;font-size:.8em;max-height:400px;overflow:auto;white-space:pre-wrap;word-break:break-all">Select a log to view.</pre>
</div>

<div class="card">
<h2><span class="section-icon">&#128200;</span> Line-Sense Drift</h2>
<p class="hint">On-hook line reading captured at each power-on, tagged by boot number. Steady numbers mean the analogue front-end is stable; a creeping value hints at a developing fault.</p>
<div id="driftbox" style="font-size:.85em;color:var(--fg3)">Loading...</div>
</div>

<div class="card">
<h2><span class="section-icon">&#9989;</span> Last Self-Test</h2>
<p class="hint">Most recent bring-up self-test result (persists across reboots). Run a fresh one from the Terminal tab.</p>
<pre id="selftestbox" style="background:var(--log-bg);color:var(--log-fg);padding:12px;border-radius:6px;font-size:.8em;max-height:320px;overflow:auto;white-space:pre-wrap;word-break:break-word">Loading...</pre>
</div>

<div class="card">
<h2><span class="section-icon">&#128295;</span> System Information</h2>
<p class="hint">Technical details about the device.</p>
<div id="sysinfo" style="font-size:.85em;color:var(--fg3);line-height:1.6">Loading...</div>
<div style="margin-top:10px"><button onclick="rebootDevice()" class="btn-danger">Restart Telephone</button></div>
</div>

</div>

<!-- ===== TERMINAL TAB ===== -->
<div class="tab-content" id="tab-terminal">
<div class="card">
<h2><span class="section-icon">&#128421;</span> Command Terminal</h2>
<p class="hint">Send commands directly to the telephone for manual control and troubleshooting. Type <b>help</b> for a list of commands.</p>
<pre id="termout" style="background:var(--log-bg);color:var(--log-fg);padding:12px;border-radius:6px;font-size:.8em;height:320px;overflow:auto;white-space:pre-wrap;word-break:break-word;margin-bottom:10px">K6 GPO Exhibit terminal ready. Type 'help' for commands.</pre>
<div class="field-row" style="gap:6px">
<input type="text" id="termin" placeholder="Enter command…" autocomplete="off" autocapitalize="off" spellcheck="false" onkeydown="termKey(event)" style="flex:1;min-width:120px;font-family:monospace">
<button onclick="runCmd()" style="margin:0">Send</button>
</div>
<div style="margin-top:8px;display:flex;gap:6px;flex-wrap:wrap">
<button class="btn-secondary" style="margin:0;padding:5px 12px;font-size:.8em" onclick="quickCmd('status')">status</button>
<button class="btn-secondary" style="margin:0;padding:5px 12px;font-size:.8em" onclick="quickCmd('ring')">ring</button>
<button class="btn-secondary" style="margin:0;padding:5px 12px;font-size:.8em" onclick="quickCmd('hangup')">hangup</button>
<button class="btn-secondary" style="margin:0;padding:5px 12px;font-size:.8em" onclick="quickCmd('ls /')">ls /</button>
<button class="btn-secondary" style="margin:0;padding:5px 12px;font-size:.8em" onclick="quickCmd('sd')">sd</button>
<button class="btn-secondary" style="margin:0;padding:5px 12px;font-size:.8em" onclick="quickCmd('help')">help</button>
<button class="btn-secondary" style="margin:0;padding:5px 12px;font-size:.8em" onclick="termClear()">clear</button>
</div>
<div style="margin-top:8px;display:flex;gap:6px;flex-wrap:wrap">
<span style="font-size:.8em;opacity:.7;align-self:center">Commissioning:</span>
<button class="btn-secondary" style="margin:0;padding:5px 12px;font-size:.8em" onclick="quickCmd('selftest')">self-test</button>
<button class="btn-secondary" style="margin:0;padding:5px 12px;font-size:.8em" onclick="quickCmd('probe')">audio probe</button>
<button class="btn-secondary" style="margin:0;padding:5px 12px;font-size:.8em" onclick="quickCmd('calibrate on')">calibrate on (handset down)</button>
<button class="btn-secondary" style="margin:0;padding:5px 12px;font-size:.8em" onclick="quickCmd('calibrate off')">calibrate off (handset up)</button>
<button class="btn-secondary" style="margin:0;padding:5px 12px;font-size:.8em" onclick="quickCmd('dialecho')">dial echo toggle</button>
<button class="btn-secondary" style="margin:0;padding:5px 12px;font-size:.8em" onclick="quickCmd('ticks')">dial ticks toggle</button>
</div>
</div>
</div>

<!-- ===== SETTINGS TAB ===== -->
<div class="tab-content" id="tab-settings">

<div class="card">
<h2><span class="section-icon">&#128266;</span> Sound Settings</h2>
<p class="hint">Adjust how loud the telephone sounds through the handset and bell.</p>
<div class="field">
<div class="field-label">Handset Volume</div>
<div class="field-hint">How loud audio plays through the telephone earpiece.</div>
<div class="field-row"><input type="range" id="vol" min="0" max="21" value="15" oninput="setVol(this.value)" style="flex:1"><span id="vollbl" style="min-width:30px;text-align:right">15</span></div>
</div>
<div class="field">
<div class="field-label">Bell Frequency</div>
<div class="field-hint">Ringing frequency in Hz. UK exchanges used ~17 Hz (16&#8532;) to 25 Hz. Lower can give an older bell a fuller ring &mdash; try a few and listen. Press "Test Ring" after changing to hear it.</div>
<div class="field-row">
<input type="number" id="bellfreq" value="25" min="10" max="50" style="width:70px">
<span> Hz</span><button onclick="setBellFreq()" style="margin:0">Save</button>
<button onclick="testRing()" class="btn-secondary" style="margin:0">Test Ring (3s)</button>
</div>
</div>
<div class="field">
<div class="field-label">Line Level (earpiece trim)</div>
<div class="field-hint">Master attenuation of all audio sent to the phone line. Lower this if audio distorts in the earpiece &mdash; it goes far quieter than the Handset Volume alone can. Use "Test Tone" to set the level, then fine-tune with Handset Volume. 100% = no attenuation.</div>
<div class="field-row"><input type="range" id="linelevel" min="0" max="100" value="100" oninput="setLineLevel(this.value)" style="flex:1"><span id="linelevellbl" style="min-width:38px;text-align:right">100%</span></div>
<div class="field-row"><button onclick="testTone()" class="btn-secondary" style="margin:0">Test Tone (5s)</button></div>
</div>
</div>

<div class="card">
<h2><span class="section-icon">&#128276;</span> Automatic Ringing</h2>
<p class="hint">When in Automatic mode, the phone rings by itself at random intervals to attract visitors.</p>
<div class="field">
<div class="field-label">Time Between Rings</div>
<div class="field-hint">The phone will ring randomly between these two times.</div>
<div class="field-row">
<span>Every </span><input type="number" id="armin" value="5" min="1" max="120" style="width:60px">
<span> to </span><input type="number" id="armax" value="30" min="1" max="120" style="width:60px">
<span> minutes</span><button onclick="setAutoRing()" style="margin:0">Save</button>
</div>
</div>
<div class="field">
<div class="field-label">How Long to Ring</div>
<div class="field-hint">How many seconds the phone rings each time before giving up.</div>
<div class="field-row">
<span>Ring for </span><input type="number" id="rtmin" value="4" min="2" max="15" style="width:55px">
<span> to </span><input type="number" id="rtmax" value="8" min="2" max="15" style="width:55px">
<span> seconds</span><button onclick="setRingTone()" style="margin:0">Save</button>
</div>
</div>
<div class="field">
<div class="field-label">Maximum Ring Cycles</div>
<div class="field-hint">How many times the bell rings before it stops trying. Set to 0 for unlimited.</div>
<div class="field-row">
<input type="number" id="ringmax" value="10" min="0" max="60" style="width:70px">
<span> cycles</span><button onclick="setRingCount()" style="margin:0">Save</button>
</div>
</div>
<hr class="divider">
<div class="field">
<div class="field-label">Inactivity Warning</div>
<div class="field-hint">Flash the panel lamp if no visitors for this many minutes. Set to 0 to disable.</div>
<div class="field-row">
<span>Warn after </span><input type="number" id="alertidle" value="120" min="0" max="1440" style="width:70px">
<span> minutes</span><button onclick="setAlertIdle()" style="margin:0">Save</button>
</div>
</div>
</div>

<div class="card">
<h2><span class="section-icon">&#9742;</span> Dialling</h2>
<p class="hint">Tune how the rotary dial is decoded.</p>
<div class="field">
<div class="field-label">Time Allowed Between Digits</div>
<div class="field-hint">How long the phone waits after a digit before it decides the number is finished. Increase this if visitors (through age or unfamiliarity with a rotary dial) can't dial the next digit quickly enough and the number is cut short.</div>
<div class="field-row">
<span>Wait </span><input type="number" id="digitgap" value="3" min="2" max="30" step="1" style="width:65px">
<span> seconds</span><button onclick="setDigitGap()" style="margin:0">Save</button>
</div>
</div>
</div>

<div class="card">
<h2><span class="section-icon">&#128246;</span> Wi-Fi Network</h2>
<p class="hint">The telephone can host its own Wi-Fi hotspot, or join an existing network. Only one at a time. Changing this restarts the telephone.</p>
<div class="field">
<div class="field-label">Connection Mode</div>
<div class="field-row">
<select id="wifimode" onchange="wifiModeChanged()" style="padding:6px 10px;border-radius:6px;border:1px solid var(--border2);background:var(--bg4);color:var(--fg1)">
<option value="ap">Host its own hotspot (K6-Exhibit)</option>
<option value="sta">Join an existing Wi-Fi network</option>
</select>
</div>
</div>
<div class="field" id="wifi-sta-fields" style="display:none">
<div class="field-label">Network Name (SSID)</div>
<div class="field-row"><input type="text" id="wifissid" placeholder="Your Wi-Fi name" oninput="touchUI()" style="flex:1"></div>
<div class="field-label" style="margin-top:8px">Password</div>
<div class="field-row"><input type="password" id="wifipass" placeholder="Leave blank for an open network" oninput="touchUI()" style="flex:1"></div>
<div class="field-hint" style="margin-top:6px">If the telephone can't join this network it automatically falls back to hosting its own <b>K6-Exhibit</b> hotspot, so you can always reconnect and fix the details.</div>
</div>
<div class="field-row" style="margin-top:6px"><button onclick="saveWifi()" class="btn-danger" style="margin:0">Save &amp; Restart</button></div>
<div class="status" id="wifistatus"></div>
</div>

<div class="card">
<h2><span class="section-icon">&#128176;</span> A+B Coin Box</h2>
<p class="hint">Control whether the A+B coin box daughter board is active. In Auto mode, the system detects the hardware at boot. Use the override to force it on or off.</p>
<div class="field">
<div class="field-label">Coin Box Mode</div>
<div class="field-hint">Auto = detect hardware at boot. Force Off = disable coin logic. Force On = always require coins.</div>
<div class="field-row">
<select id="coinmode" onchange="setCoinMode(this.value)" style="padding:6px 10px;border-radius:6px;border:1px solid var(--border2);background:var(--bg4);color:var(--fg1)">
<option value="-1">Auto (detect at boot)</option>
<option value="0">Force Off</option>
<option value="1">Force On</option>
</select>
<span id="coinstatus" style="margin-left:10px;font-size:.85em;color:var(--fg3)"></span>
</div>
</div>
</div>

<div class="card">
<h2><span class="section-icon">&#128193;</span> Audio Files</h2>
<p class="hint">Browse, upload, and manage the audio files stored on the SD card.</p>
<div class="path" id="pathbar">/</div>
<table id="filetbl"><tbody></tbody></table>
<div style="margin-top:12px;display:flex;gap:8px;flex-wrap:wrap;align-items:center">
<input type="file" id="upfile" multiple accept=".mp3,.MP3,.wav,.WAV">
<button onclick="upload()" id="upbtn">Upload Files</button>
<button onclick="mkdirPrompt()" class="btn-secondary">Create Folder</button>
</div>
<div class="status" id="upstatus"></div>
</div>

<div class="card">
<h2><span class="section-icon">&#127908;</span> Record New Audio</h2>
<p class="hint">Record audio using your device's microphone. Listen back before saving.</p>
<div style="margin-top:8px;display:flex;gap:8px;align-items:center">
<button onclick="startRec()" id="recbtn">Start Recording</button>
<button onclick="stopRec()" id="stopbtn" disabled class="btn-secondary">Stop Recording</button>
<span id="rectimer" style="margin-left:4px;font-family:monospace;color:var(--warn);font-size:.9em"></span>
</div>
<div id="recpreview" style="display:none;margin-top:10px;padding:12px;background:var(--bg5);border-radius:8px;border:1px solid var(--border2)">
<div style="color:var(--fg2);font-size:.85em;font-weight:500;margin-bottom:6px">Preview your recording:</div>
<audio id="recaudio" controls style="width:100%;margin-bottom:10px"></audio>
<div style="color:var(--fg2);font-size:.85em;font-weight:500;margin-bottom:4px">Save as:</div>
<div class="field-row">
<input type="text" id="recname" placeholder="filename">.mp3
<span style="margin-left:8px">in <select id="recdir"><option value="/history/">/history/</option><option value="/numbers/">/numbers/</option><option value="/system/">/system/</option></select></span>
</div>
<div style="margin-top:10px;display:flex;gap:8px">
<button onclick="saveRec()">Save Recording</button>
<button onclick="discardRec()" class="btn-secondary">Discard</button>
</div>
</div>
<div class="status" id="recstatus"></div>
</div>

<div class="card">
<h2><span class="section-icon">&#9881;</span> Update Firmware</h2>
<p class="hint">Upload a new firmware file (.bin) to update the telephone software.</p>
<input type="file" id="otafile" accept=".bin">
<button onclick="otaUpload()" id="otabtn">Install Update</button>
<div id="prog"><div id="progbar"></div></div>
<div class="status" id="otastatus"></div>
<hr class="divider">
<div class="field-hint" style="margin-bottom:4px">If a firmware update causes problems:</div>
<button onclick="rollbackFW()" class="btn-danger">Restore Previous Version</button>
</div>

</div>

<script>
// --- Tab navigation ---
function switchTab(id,btn){
  document.querySelectorAll('.tab-content').forEach(t=>t.classList.remove('active'));
  document.querySelectorAll('.tab-nav button').forEach(b=>b.classList.remove('active'));
  document.getElementById('tab-'+id).classList.add('active');
  if(btn)btn.classList.add('active');
  closeMenu();
  try{localStorage.setItem('k6tab',id)}catch(e){}
  if(id==='stats'){loadStats();loadDiscovery();loadAliases();}
  if(id==='diagnostics'){loadErrors();loadDiag();loadLog('system');}
  if(id==='terminal'){setTimeout(function(){document.getElementById('termin').focus()},50);}
  if(id==='settings'){loadFiles();}
}
function toggleMenu(){
  var open=document.getElementById('tabnav').classList.toggle('open');
  document.getElementById('navbackdrop').classList.toggle('open',open);
}
function closeMenu(){
  document.getElementById('tabnav').classList.remove('open');
  document.getElementById('navbackdrop').classList.remove('open');
}
// --- Theme ---
function toggleTheme(){
  var b=document.body;b.classList.toggle('light');
  var isLight=b.classList.contains('light');
  document.getElementById('themebtn').textContent=isLight?'Dark Mode':'Light Mode';
  document.getElementById('themecolor').content=isLight?'#f5f5f5':'#1a1a1a';
  try{localStorage.setItem('k6theme',isLight?'light':'dark')}catch(e){}
}
(function(){try{if(localStorage.getItem('k6theme')==='light'){document.body.classList.add('light');document.getElementById('themebtn').textContent='Dark Mode';document.getElementById('themecolor').content='#f5f5f5';}}catch(e){}})();
(function(){try{var t=localStorage.getItem('k6tab');if(t){var idx={overview:1,stats:2,diagnostics:3,terminal:4,settings:5}[t]||1;var btn=document.querySelector('.tab-nav button:nth-child('+idx+')');switchTab(t,btn);}}catch(e){}})();
// --- Terminal ---
let termHist=[],termHi=0;
function termPrint(t){var o=document.getElementById('termout');o.textContent+='\n'+t;o.scrollTop=o.scrollHeight;}
function termClear(){document.getElementById('termout').textContent='K6 GPO Exhibit terminal ready. Type \'help\' for commands.';}
function termKey(e){
  if(e.key==='Enter'){runCmd();return;}
  if(e.key==='ArrowUp'){if(termHi>0){termHi--;e.target.value=termHist[termHi];}e.preventDefault();}
  if(e.key==='ArrowDown'){if(termHi<termHist.length-1){termHi++;e.target.value=termHist[termHi];}else{termHi=termHist.length;e.target.value='';}e.preventDefault();}
}
function runCmd(){
  var i=document.getElementById('termin');var c=i.value.trim();if(!c)return;
  termPrint('> '+c);termHist.push(c);termHi=termHist.length;i.value='';
  fetch('/api/terminal?cmd='+encodeURIComponent(c),{method:'POST'})
    .then(r=>r.text()).then(t=>{if(t)termPrint(t);loadStatus();})
    .catch(e=>termPrint('error: '+e));
}
function quickCmd(c){document.getElementById('termin').value=c;runCmd();}
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
        let nm=f.name.toLowerCase();
        let playable=nm.endsWith('.mp3')||nm.endsWith('.wav');
        let acts=playable?'<span class="play" onclick="preview(\''+cwd+f.name+'\')">play</span> ':'';
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
var uiEdit=0;var _dbt={};
function touchUI(){uiEdit=Date.now();}
function debPost(key,url){touchUI();clearTimeout(_dbt[key]);_dbt[key]=setTimeout(function(){fetch(url,{method:'POST'})},150);}
function setVol(v){
  document.getElementById('vollbl').textContent=v;
  debPost('vol','/api/volume?v='+v);
}
function setDigitGap(){
  touchUI();
  let v=document.getElementById('digitgap').value;
  fetch('/api/digitgap?v='+v,{method:'POST'});
}
function wifiModeChanged(){
  touchUI();
  let sta=document.getElementById('wifimode').value==='sta';
  document.getElementById('wifi-sta-fields').style.display=sta?'block':'none';
}
function saveWifi(){
  let mode=document.getElementById('wifimode').value;
  let ssid=document.getElementById('wifissid').value;
  if(mode==='sta'&&!ssid){alert('Enter the name of the Wi-Fi network to join.');return;}
  let msg=mode==='sta'
    ?'The telephone will restart and try to join "'+ssid+'". If it can\'t, it falls back to its own K6-Exhibit hotspot.'
    :'The telephone will restart and host its own K6-Exhibit hotspot.';
  if(!confirm(msg))return;
  let pass=encodeURIComponent(document.getElementById('wifipass').value);
  document.getElementById('wifistatus').innerHTML='<span class="warn">Saving and restarting\u2026</span>';
  fetch('/api/wifi?mode='+mode+'&ssid='+encodeURIComponent(ssid)+'&pass='+pass,{method:'POST'})
    .then(()=>{setTimeout(()=>{location.reload()},9000);})
    .catch(()=>{document.getElementById('wifistatus').innerHTML='<span class="warn">Restarting\u2026 reconnect to the telephone\'s network.</span>';});
}
function setBellFreq(){
  touchUI();
  let hz=document.getElementById('bellfreq').value;
  fetch('/api/bellfreq?hz='+hz,{method:'POST'});
}
function setLineLevel(v){
  document.getElementById('linelevellbl').textContent=v+'%';
  debPost('linelevel','/api/linelevel?v='+v);
}
function testTone(){fetch('/api/tone?hz=1000&secs=5',{method:'POST'})}
function setRingCount(){
  touchUI();
  let n=document.getElementById('ringmax').value;
  fetch('/api/ringcount?n='+n,{method:'POST'});
}
function setRingTone(){
  touchUI();
  let mn=document.getElementById('rtmin').value;
  let mx=document.getElementById('rtmax').value;
  fetch('/api/ringtone?min='+mn+'&max='+mx,{method:'POST'});
}
function setAlertIdle(){
  touchUI();
  let v=document.getElementById('alertidle').value;
  fetch('/api/alertidle?v='+v,{method:'POST'});
}
function setCoinMode(v){
  touchUI();
  fetch('/api/coinmode?v='+v,{method:'POST'}).then(r=>r.json()).then(d=>{
    document.getElementById('coinstatus').textContent=d.active?'Coin logic active':'Coin logic disabled';
  });
}
function setAutoRing(){
  touchUI();
  let mn=document.getElementById('armin').value;
  let mx=document.getElementById('armax').value;
  fetch('/api/autoring?min='+mn+'&max='+mx,{method:'POST'});
}
function ringNow(){fetch('/api/ring',{method:'POST'})}
function testRing(){fetch('/api/testring?secs=3',{method:'POST'})}
function toggleMode(){fetch('/api/mode',{method:'POST'}).then(()=>loadStatus())}
function rebootDevice(){
  if(!confirm('This will restart the telephone. Any active calls will be disconnected.'))return;
  fetch('/api/reboot',{method:'POST'}).then(()=>{
    document.getElementById('sysinfo').innerHTML='<span class="warn">Restarting...</span>';
    setTimeout(()=>{location.reload()},8000);
  });
}
function resetStats(){
  if(!confirm('This will erase all visitor statistics. Are you sure?'))return;
  fetch('/api/stats/reset',{method:'POST'}).then(()=>loadStats());
}
function loadStatus(){
  fetch('/api/status').then(r=>r.json()).then(d=>{
    let uH=Math.floor(d.uptime/3600),uM=Math.floor(d.uptime%3600/60);
    document.getElementById('sysinfo').innerHTML=
      'Available memory: '+(d.heap/1024).toFixed(0)+'KB<br>'+
      'SD card: '+(d.sd?'<span class="ok">Working</span>':'<span class="err">Not detected</span>')+
      (d.sd_total?' ('+d.sd_used+'MB used of '+d.sd_total+'MB)':'')+
      '<br>Running for: '+uH+' hours '+uM+' minutes<br>'+
      'Firmware version: '+d.firmware+
      (d.wifi_mode?('<br>Wi-Fi: '+(d.wifi_mode==='ap'?'hosting <b>'+d.wifi_ssid+'</b>':'joined <b>'+d.wifi_ssid+'</b>')+' at '+d.wifi_ip):'');
    // Don't clobber controls the visitor is actively adjusting: skip syncing
    // input values for a few seconds after any edit (otherwise this poll snaps
    // a slider back to a stale server value mid-drag).
    if(Date.now()-uiEdit>4000){
    document.getElementById('vol').value=d.volume;
    document.getElementById('vollbl').textContent=d.volume;
    if(d.bell_freq!==undefined) document.getElementById('bellfreq').value=d.bell_freq;
    if(d.digit_gap!==undefined) document.getElementById('digitgap').value=Math.round(d.digit_gap/1000);
    if(d.wifi_cfg_mode!==undefined){
      document.getElementById('wifimode').value=d.wifi_cfg_mode;
      if(d.wifi_cfg_ssid) document.getElementById('wifissid').value=d.wifi_cfg_ssid;
      wifiModeChanged();
    }
    if(d.line_level!==undefined){document.getElementById('linelevel').value=d.line_level;document.getElementById('linelevellbl').textContent=d.line_level+'%';}
    if(d.ring_max!==undefined) document.getElementById('ringmax').value=d.ring_max;
    if(d.rt_min!==undefined){document.getElementById('rtmin').value=d.rt_min;document.getElementById('rtmax').value=d.rt_max;}
    if(d.alert_idle!==undefined) document.getElementById('alertidle').value=d.alert_idle;
    if(d.coin_override!==undefined){
      document.getElementById('coinmode').value=d.coin_override;
      let cs=document.getElementById('coinstatus');
      cs.textContent=d.coin_active?'Coin logic active':'Coin logic disabled';
    }
    document.getElementById('armin').value=Math.round(d.ar_min/60000);
    document.getElementById('armax').value=Math.round(d.ar_max/60000);
    }
    document.getElementById('alertbanner').style.display=d.alert_on?'block':'none';
    let ml=document.getElementById('modelbl');
    let mb=document.getElementById('modebtn');
    if(d.mode=='AUTO'){ml.textContent='AUTOMATIC';ml.className='badge badge-green';mb.textContent='Switch to Manual Mode';}
    else{ml.textContent='MANUAL';ml.className='badge badge-amber';mb.textContent='Switch to Automatic Mode';}
    let states={'IDLE':'Waiting for visitors','RINGING':'Phone is ringing','PLAYING':'Playing audio','DIALLING':'Visitor is dialling'};
    document.getElementById('statelbl').textContent=states[d.state]||d.state;
    document.getElementById('playlbl').textContent=d.playing||'Nothing';
    let ct=document.getElementById('calltimer');
    if(d.state!=='IDLE'&&d.call_secs>=0){
      let m=Math.floor(d.call_secs/60),s=d.call_secs%60;
      ct.textContent=m+':'+(s<10?'0':'')+s;
    } else { ct.textContent='\u2014'; }
    // Quick Health — one-line badge + detail lines.
    let heapKB=Math.round(d.heap/1024);
    let sdOk=!!d.sd, calOk=!!d.line_cal, lowHeap=heapKB<40;
    let cls='badge-green',word='Healthy';
    if(!sdOk){cls='badge-red';word='SD fault';}
    else if(!calOk||lowHeap||(d.errors&&d.errors>0)){cls='badge-amber';word='Check';}
    let badge='<span class="badge '+cls+'">'+word+'</span> ';
    badge+='<span style="font-size:.85em;color:var(--fg3)">SD '+(sdOk?'OK':'FAULT')
      +' \u00b7 Line '+(calOk?'calibrated':'not calibrated')
      +' \u00b7 '+heapKB+' KB free</span>';
    document.getElementById('healthbadge').innerHTML=badge;
    let hb=document.getElementById('healthbox');
    let hh='SD Card: '+(sdOk?'<span class="ok">OK</span>':'<span class="err">Not detected</span>');
    hh+='<br>Line sensing: '+(calOk?'<span class="ok">Calibrated</span>':'<span class="warn">Not calibrated (using defaults)</span>');
    hh+='<br>Memory: '+(lowHeap?'<span class="warn">':'<span class="ok">')+heapKB+' KB free</span>';
    hh+='<br>Uptime: '+uH+'h '+uM+'m';
    if(d.errors&&d.errors>0) hh+='<br><span class="err">&#9888; '+d.errors+' error'+(d.errors>1?'s':'')+' recorded</span> <span style="font-size:.8em;color:var(--link);cursor:pointer" onclick="switchTab(\'diagnostics\',document.querySelector(\'.tab-nav button:nth-child(3)\'))">(view)</span>';
    else hh+='<br>Errors: <span class="ok">None</span>';
    hb.innerHTML=hh;
  });
}
function loadSession(){
  fetch('/api/stats').then(r=>r.json()).then(d=>{
    let s=d.session||{};
    let sb=document.getElementById('sessionbox');
    let h='';
    h+='<span class="stat"><span class="stat-label">Pickups Today</span><b>'+(s.pickups||0)+'</b></span>';
    h+='<span class="stat"><span class="stat-label">Numbers Dialled</span><b>'+(s.outgoing||0)+'</b></span>';
    h+='<span class="stat"><span class="stat-label">Times Rung</span><b>'+(s.incoming||0)+'</b></span>';
    let pk=s.pickups||0;
    if(pk>0){
      let rate=Math.round((s.completions||0)/pk*100);
      h+='<span class="stat"><span class="stat-label">Completion Rate</span><b>'+rate+'%</b></span>';
    }
    if(s.top_numbers&&s.top_numbers.length){
      h+='<span class="stat"><span class="stat-label">Most Dialled</span><b>'+s.top_numbers[0].number+'</b> &times;'+s.top_numbers[0].count+'</span>';
    }
    let up=s.uptime||0,uH=Math.floor(up/3600),uM=Math.floor(up%3600/60);
    h+='<span class="stat"><span class="stat-label">Powered On</span><b>'+uH+'h '+uM+'m</b></span>';
    sb.innerHTML=h;
  });
}
function loadStats(){
  fetch('/api/stats').then(r=>r.json()).then(d=>{
    let h='<div style="display:flex;flex-wrap:wrap;gap:4px;margin-bottom:8px">';
    h+='<span class="stat"><span class="stat-label">Sessions (Pickups)</span><b>'+d.pickups+'</b></span>';
    h+='<span class="stat"><span class="stat-label">Times Rung</span><b>'+d.incoming+'</b></span>';
    h+='<span class="stat"><span class="stat-label">Calls Answered</span><b>'+d.answered+'</b></span>';
    h+='<span class="stat"><span class="stat-label">Numbers Dialled</span><b>'+d.outgoing+'</b></span>';
    h+='<span class="stat"><span class="stat-label">Unknown Numbers</span><b>'+d.not_recognised+'</b></span>';
    if(d.coin_collected>0) h+='<span class="stat"><span class="stat-label">Coins Collected</span><b>'+d.coin_collected+'</b></span>';
    if(d.coin_refunded>0) h+='<span class="stat"><span class="stat-label">Coins Refunded</span><b>'+d.coin_refunded+'</b></span>';
    h+='</div>';
    if(d.call_count>0){
      let avgM=Math.floor(d.avg_call/60), avgS=d.avg_call%60;
      let lonM=Math.floor(d.longest_call/60), lonS=d.longest_call%60;
      h+='<div style="display:flex;flex-wrap:wrap;gap:4px;margin-bottom:8px">';
      h+='<span class="stat"><span class="stat-label">Average Call</span><b>'+avgM+'m '+avgS+'s</b></span>';
      h+='<span class="stat"><span class="stat-label">Longest Call</span><b>'+lonM+'m '+lonS+'s</b></span>';
      let totM=Math.floor(d.call_seconds/60);
      h+='<span class="stat"><span class="stat-label">Total Talk Time</span><b>'+totM+' min</b></span>';
      h+='</div>';
    }
    // Engagement metrics
    h+='<div style="display:flex;flex-wrap:wrap;gap:4px;margin-bottom:8px">';
    if(d.pickups>0){
      let compRate=d.completions>0?Math.min(100,Math.round(d.completions/d.pickups*100)):0;
      h+='<span class="stat"><span class="stat-label">Completion Rate</span><b>'+compRate+'%</b></span>';
    }
    if(d.first_digit_n>0){
      let avgFd=Math.round(d.first_digit_ms/d.first_digit_n/1000*10)/10;
      h+='<span class="stat"><span class="stat-label">Avg. Time to First Digit</span><b>'+avgFd+'s</b></span>';
    }
    let uH=Math.floor(d.total_uptime/3600), uM=Math.floor(d.total_uptime%3600/60);
    h+='<span class="stat"><span class="stat-label">Total Running Time</span><b>'+uH+'h '+uM+'m</b></span>';
    h+='</div>';
    if(d.top_numbers&&d.top_numbers.length){
      h+='<div style="margin-top:12px"><div style="color:var(--fg2);font-weight:500;font-size:.9em;margin-bottom:4px">Most Popular Numbers:</div>';
      d.top_numbers.forEach(n=>{
        h+='<span class="stat"><span class="topnum">'+n.number+'</span> &times;'+n.count+'</span> ';
      });
      h+='</div>';
    }
    document.getElementById('statsbox').innerHTML=h;
  });
}
function loadDiscovery(){
  fetch('/api/discovery').then(r=>r.json()).then(d=>{
    if(!d||!d.length){document.getElementById('discbox').innerHTML='<span style="color:var(--fg4)">No unrecognised numbers yet. When visitors dial numbers that aren\'t in the directory, they\'ll appear here.</span>';return;}
    let h='<table style="width:100%;border-collapse:collapse"><thead><tr><td style="color:var(--fg3);padding:4px 8px">Number Dialled</td><td style="color:var(--fg3);padding:4px 8px">Times Tried</td><td></td></tr></thead><tbody>';
    d.forEach(e=>{
      h+='<tr><td style="padding:4px 8px"><span class="topnum">'+e.number+'</span></td><td style="padding:4px 8px">'+e.count+'</td><td style="padding:4px 8px"><span class="del" onclick="removeDiscovery(\''+e.number+'\')">remove</span></td></tr>';
    });
    h+='</tbody></table>';
    document.getElementById('discbox').innerHTML=h;
  });
}
function clearDiscovery(){
  if(!confirm('Clear the numbers tried log?'))return;
  fetch('/api/discovery/clear',{method:'POST'}).then(()=>loadDiscovery());
}
function removeDiscovery(num){
  fetch('/api/discovery/remove?number='+encodeURIComponent(num),{method:'POST'}).then(()=>loadDiscovery());
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
      tr.innerHTML='<td class="topnum">'+a.number+'</td><td>'+a.name+'</td><td><span class="del" onclick="delAlias('+i+')">remove</span></td>';
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
let mediaRec=null,recChunks=[],recInt=null,recStart=0,recBlob=null;
function startRec(){
  document.getElementById('recpreview').style.display='none';
  recBlob=null;
  navigator.mediaDevices.getUserMedia({audio:true}).then(stream=>{
    recChunks=[];
    mediaRec=new MediaRecorder(stream,{mimeType:'audio/webm;codecs=opus'});
    mediaRec.ondataavailable=e=>{if(e.data.size>0)recChunks.push(e.data)};
    mediaRec.onstop=()=>{
      stream.getTracks().forEach(t=>t.stop());
      recBlob=new Blob(recChunks,{type:'audio/webm'});
      let url=URL.createObjectURL(recBlob);
      document.getElementById('recaudio').src=url;
      document.getElementById('recpreview').style.display='block';
      document.getElementById('recstatus').innerHTML='<span class="ok">Recording complete. Listen back above, then save or discard.</span>';
    };
    mediaRec.start(100);
    recStart=Date.now();
    document.getElementById('recbtn').disabled=true;
    document.getElementById('stopbtn').disabled=false;
    document.getElementById('recstatus').innerHTML='<span style="color:var(--err);font-weight:500">&#9679; Recording in progress...</span>';
    recInt=setInterval(()=>{let s=Math.floor((Date.now()-recStart)/1000);document.getElementById('rectimer').textContent=Math.floor(s/60)+':'+(s%60<10?'0':'')+(s%60)},500);
  }).catch(e=>{document.getElementById('recstatus').innerHTML='<span class="err">Microphone access denied. Please allow microphone access and try again.</span>'});
}
function stopRec(){
  if(mediaRec&&mediaRec.state!=='inactive')mediaRec.stop();
  clearInterval(recInt);
  document.getElementById('recbtn').disabled=false;
  document.getElementById('stopbtn').disabled=true;
}
function saveRec(){
  if(!recBlob){return;}
  let name=document.getElementById('recname').value.trim();
  if(!name){document.getElementById('recstatus').innerHTML='<span class="err">Please enter a filename</span>';return;}
  let dir=document.getElementById('recdir').value;
  let fd=new FormData();
  fd.append('file',recBlob,name+'.mp3');
  document.getElementById('recstatus').innerHTML='<span class="ok">Saving...</span>';
  fetch('/api/upload?path='+encodeURIComponent(dir),{method:'POST',body:fd})
  .then(r=>r.json()).then(d=>{
    document.getElementById('recstatus').innerHTML=d.ok?'<span class="ok">Saved to '+dir+name+'.mp3</span>':'<span class="err">'+d.error+'</span>';
    document.getElementById('rectimer').textContent='';
    document.getElementById('recpreview').style.display='none';
    recBlob=null;
    loadFiles();
  });
}
function discardRec(){
  recBlob=null;
  document.getElementById('recpreview').style.display='none';
  document.getElementById('recstatus').innerHTML='Recording discarded.';
  document.getElementById('rectimer').textContent='';
}
function rollbackFW(){
  if(!confirm('Restore the previous firmware version? The telephone will restart.'))return;
  fetch('/api/rollback',{method:'POST'}).then(r=>r.json()).then(d=>{
    if(d.ok){document.getElementById('otastatus').innerHTML='<span class="ok">Restoring previous version... restarting</span>';setTimeout(()=>{location.reload()},8000);}
    else document.getElementById('otastatus').innerHTML='<span class="err">'+(d.error||'No previous firmware available')+'</span>';
  });
}
function loadErrors(){
  fetch('/api/diagnostics').then(r=>r.json()).then(d=>{
    let box=document.getElementById('errorbox');
    if(!d||!d.length){box.innerHTML='<span style="color:var(--ok)">No errors recorded since last power-on.</span>';return;}
    let h='<table style="width:100%;border-collapse:collapse"><thead><tr><td style="color:var(--fg3);padding:4px 8px">Time</td><td style="color:var(--fg3);padding:4px 8px">Type</td><td style="color:var(--fg3);padding:4px 8px">Detail</td></tr></thead><tbody>';
    d.forEach(e=>{
      let mins=Math.floor(e.time/60), secs=e.time%60;
      let ts=mins+'m '+secs+'s';
      let cls=e.type==='SD_FAILURE'?'err':e.type==='BELL_FAULT'?'warn':'warn';
      h+='<tr><td style="padding:4px 8px;font-family:monospace;font-size:.8em">'+ts+'</td>';
      h+='<td style="padding:4px 8px"><span class="'+cls+'">'+e.type+'</span></td>';
      h+='<td style="padding:4px 8px;font-size:.85em">'+(e.detail||'—')+'</td></tr>';
    });
    h+='</tbody></table>';
    box.innerHTML=h;
  }).catch(()=>{document.getElementById('errorbox').innerHTML='<span class="err">Failed to load error log</span>';});
}
function loadDiag(){
  fetch('/api/diag').then(r=>r.json()).then(d=>{
    let db=document.getElementById('driftbox');
    if(!d.boot_lines||!d.boot_lines.length){db.innerHTML='<span style="color:var(--fg4)">No boot readings recorded yet.</span>';}
    else{
      let bl=d.boot_lines;
      let h='<table style="width:100%;border-collapse:collapse"><thead><tr><td style="color:var(--fg3);padding:4px 8px">Boot #</td><td style="color:var(--fg3);padding:4px 8px">On-Hook Reading</td></tr></thead><tbody>';
      for(let i=bl.length-1;i>=0;i--){
        h+='<tr><td style="padding:4px 8px;font-family:monospace">'+bl[i].boot+'</td><td style="padding:4px 8px;font-family:monospace">'+bl[i].raw+'</td></tr>';
      }
      h+='</tbody></table>';
      db.innerHTML=h;
    }
    let stb=document.getElementById('selftestbox');
    if(d.selftest&&d.selftest.length){
      let up=d.selftest_uptime||0,uH=Math.floor(up/3600),uM=Math.floor(up%3600/60);
      stb.textContent='(ran at uptime '+uH+'h '+uM+'m)\n\n'+d.selftest;
    } else { stb.textContent='No self-test has been run yet. Run one from the Terminal tab.'; }
  }).catch(()=>{document.getElementById('driftbox').innerHTML='<span class="err">Failed to load diagnostics</span>';});
}
if('serviceWorker' in navigator){navigator.serviceWorker.register('/sw.js').catch(()=>{})}
loadStatus();loadSession();loadStats();loadAliases();loadDiscovery();
setInterval(loadStatus,5000);
setInterval(loadSession,30000);
setInterval(loadStats,30000);
setInterval(loadDiscovery,30000);
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

    // A large file blocks server.handleClient() for the whole transfer, so
    // loop()'s watchdog reset never runs. Feed it here (this callback fires
    // per chunk) or a slow upload trips the 15s watchdog and reboots.
    esp_task_wdt_reset();

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
            // Validate first chunk of .mp3 files.  WAV files are accepted
            // as-is (the player supports MP3 and WAV).
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

static void handleCoinMode() {
    if (!s_phone) { server.send(200, "application/json", "{\"ok\":false}"); return; }
    int v = server.arg("v").toInt();
    if (v < -1) v = -1;
    if (v > 1)  v = 1;
    s_phone->coinBox().setOverride(v);
    saveSettings();
    if (s_logger) s_logger->systemLog("Coin box override set to %d via web", v);
    String json = "{\"ok\":true,\"active\":";
    json += s_phone->coinBox().isInstalled() ? "true" : "false";
    json += "}";
    server.send(200, "application/json", json);
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
    json += ",\"bell_freq\":"; json += String(s_phone->bell().ringFreq());
    json += ",\"digit_gap\":"; json += String(s_phone->numberCompleteMs());
    json += ",\"line_level\":"; json += String(s_phone->player().lineLevel());
    json += ",\"line_cal\":"; json += s_phone->line().calibrated() ? "true" : "false";
    json += ",\"errors\":"; json += String(s_stats ? s_stats->errorCount() : 0);
    json += ",\"ar_min\":";  json += String(s_phone->autoRingMinMs());
    json += ",\"ar_max\":";  json += String(s_phone->autoRingMaxMs());
    json += ",\"ring_max\":"; json += String(s_phone->maxRingCadences());
    json += ",\"rt_min\":"; json += String(s_phone->ringToneMinSecs());
    json += ",\"rt_max\":"; json += String(s_phone->ringToneMaxSecs());
    json += ",\"alert_idle\":"; json += String(s_phone->alertIdleMinutes());
    json += ",\"alert_on\":"; json += s_phone->isAlertActive() ? "true" : "false";
    json += ",\"coin_override\":"; json += String(s_phone->coinBox().overrideMode());
    json += ",\"coin_active\":"; json += s_phone->coinBox().isInstalled() ? "true" : "false";
    json += ",\"wifi_mode\":\""; json += s_ap_active ? "ap" : "sta"; json += "\"";
    json += ",\"wifi_ssid\":\""; json += jsonEscape(s_ap_active ? WiFi.softAPSSID() : WiFi.SSID()); json += "\"";
    json += ",\"wifi_ip\":\"";   json += currentIP().toString(); json += "\"";
    json += ",\"wifi_cfg_mode\":\""; json += s_wifi_sta ? "sta" : "ap"; json += "\"";
    json += ",\"wifi_cfg_ssid\":\""; json += jsonEscape(s_sta_ssid); json += "\"";
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

static void handleRollback() {
    const esp_partition_t* prev = esp_ota_get_last_invalid_partition();
    if (!prev) {
        // Try the non-running OTA partition as fallback.
        const esp_partition_t* running = esp_ota_get_running_partition();
        const esp_partition_t* other = esp_ota_get_next_update_partition(running);
        if (other && other != running) prev = other;
    }
    if (!prev) {
        server.send(200, "application/json", "{\"ok\":false,\"error\":\"No previous firmware available\"}");
        return;
    }
    esp_err_t err = esp_ota_set_boot_partition(prev);
    if (err != ESP_OK) {
        server.send(200, "application/json", "{\"ok\":false,\"error\":\"Rollback failed\"}");
        return;
    }
    if (s_logger) s_logger->systemLog("Firmware rollback to %s via web", prev->label);
    server.send(200, "application/json", "{\"ok\":true}");
    delay(500);
    ESP.restart();
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

    esp_task_wdt_reset();

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

// Stream a heap text buffer to the client without forcing a single large
// String allocation. Passing a long char* to server.send() makes the core
// build a String copy that fails ("String cast failed") under heap pressure
// and leaves a slow/closed client mid-write (the fd EAGAIN spam). Writing the
// response directly avoids that, and avoids the core's "content length is
// zero" warning that an empty send() would emit before sendContent().
static void sendTextBuffer(const char* buf, size_t len) {
    WiFiClient client = server.client();
    client.print(F("HTTP/1.1 200 OK\r\n"));
    client.print(F("Content-Type: text/plain\r\n"));
    client.printf("Content-Length: %u\r\n", (unsigned)len);
    client.print(F("Connection: close\r\n\r\n"));
    client.write((const uint8_t*)buf, len);
}

static void handleLogSystem() {
    if (!s_logger) { server.send(200, "text/plain", "(empty)"); return; }
    size_t len;
    char* buf = s_logger->readSystemLog(&len);
    if (buf) {
        sendTextBuffer(buf, len);
        free(buf);
    } else {
        server.send(200, "text/plain", "(empty)");
    }
}

static void handleLogCalls() {
    if (!s_logger) { server.send(200, "text/plain", "(empty)"); return; }
    size_t len;
    char* buf = s_logger->readCallLog(&len);
    if (buf) {
        sendTextBuffer(buf, len);
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

static void handleDigitGap() {
    if (!s_phone) { server.send(200, "application/json", "{\"ok\":false}"); return; }
    int secs = server.arg("v").toInt();
    if (secs < 2)  secs = 2;
    if (secs > 30) secs = 30;
    s_phone->setNumberCompleteMs((unsigned long)secs * 1000UL);
    saveSettings();
    if (s_logger) s_logger->systemLog("Inter-digit gap set to %ds via web", secs);
    server.send(200, "application/json", "{\"ok\":true}");
}

// Configure Wi-Fi mode (host own AP vs join existing network). Persists the
// choice and reboots so the new mode takes effect from a clean boot.
static void handleWifi() {
    String mode = server.arg("mode");
    if (mode == "sta") {
        s_wifi_sta  = true;
        s_sta_ssid  = server.arg("ssid");
        s_sta_pass  = server.arg("pass");
    } else {
        s_wifi_sta = false;
    }
    saveSettings();
    if (s_logger) s_logger->systemLog("Wi-Fi mode set to %s via web (ssid=%s), rebooting",
                                       s_wifi_sta ? "STA" : "AP", s_sta_ssid.c_str());
    server.send(200, "application/json", "{\"ok\":true}");
    delay(400);
    ESP.restart();
}

static void handleLineLevel() {
    if (!s_phone) { server.send(200, "application/json", "{\"ok\":false}"); return; }
    int v = server.arg("v").toInt();
    if (v < 0) v = 0;
    if (v > 100) v = 100;
    s_phone->player().setLineLevel(v);
    saveSettings();
    if (s_logger) s_logger->systemLog("Line level set to %d%% via web", v);
    server.send(200, "application/json", "{\"ok\":true}");
}

static void handleBellFreq() {
    if (!s_phone) { server.send(200, "application/json", "{\"ok\":false}"); return; }
    int hz = server.arg("hz").toInt();
    if (hz < 10) hz = 10;
    if (hz > 50) hz = 50;
    s_phone->bell().setRingFreq(hz);
    saveSettings();
    if (s_logger) s_logger->systemLog("Bell frequency set to %d Hz via web", hz);
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

static void handleTone() {
    if (!s_phone) { server.send(200, "application/json", "{\"ok\":false}"); return; }
    int hz = server.arg("hz").toInt();
    int secs = server.arg("secs").toInt();
    if (hz < 50) hz = 1000;
    if (secs < 1) secs = 5;
    bool ok = s_phone->player().playTestTone(hz, secs);
    if (s_logger) s_logger->systemLog("Test tone %d Hz for %ds via web", hz, secs);
    server.send(200, "application/json", ok ? "{\"ok\":true}" : "{\"ok\":false}");
}

static void handleTestRing() {
    if (!s_phone) { server.send(200, "application/json", "{\"ok\":false}"); return; }
    int secs = server.arg("secs").toInt();
    if (secs < 1) secs = 3;
    if (secs > 30) secs = 30;
    s_phone->testRing(secs);
    if (s_logger) s_logger->systemLog("Test ring (%ds) triggered via web", secs);
    server.send(200, "application/json", "{\"ok\":true}");
}

static void handleAlertIdle() {
    if (!s_phone) { server.send(200, "application/json", "{\"ok\":false}"); return; }
    int v = server.arg("v").toInt();
    if (v < 0) v = 0;
    if (v > 1440) v = 1440;
    s_phone->setAlertIdleMinutes(v);
    saveSettings();
    if (s_logger) s_logger->systemLog("Alert idle set to %d min via web", v);
    server.send(200, "application/json", "{\"ok\":true}");
}

static void handleRingTone() {
    if (!s_phone) { server.send(200, "application/json", "{\"ok\":false}"); return; }
    int mn = server.arg("min").toInt();
    int mx = server.arg("max").toInt();
    s_phone->setRingToneRange(mn, mx);
    saveSettings();
    if (s_logger) s_logger->systemLog("Ring tone set to %d-%ds via web", s_phone->ringToneMinSecs(), s_phone->ringToneMaxSecs());
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

// --- Terminal command handler -----------------------------------------------
// Text-based command interface for manual control / troubleshooting from the
// web dashboard.  Returns plain text.
static void handleTerminal() {
    if (!s_phone) { server.send(200, "text/plain", "error: phone not ready"); return; }
    String cmd = server.arg("cmd");
    cmd.trim();
    if (cmd.length() == 0) { server.send(200, "text/plain", "(no command)"); return; }

    String verb = cmd, arg = "";
    int sp = cmd.indexOf(' ');
    if (sp >= 0) { verb = cmd.substring(0, sp); arg = cmd.substring(sp + 1); arg.trim(); }
    verb.toLowerCase();
    String larg = arg; larg.toLowerCase();

    PhoneController& p = *s_phone;
    String out;

    if (verb == "help" || verb == "?") {
        out  = "Available commands:\n";
        out += "  status              show phone state\n";
        out += "  ring                trigger the bell\n";
        out += "  testring [secs]     ring for a fixed time (default 3s)\n";
        out += "  hangup              hang up / stop playback\n";
        out += "  cancel              cancel ringing\n";
        out += "  mode [auto|manual]  get/set ring mode\n";
        out += "  vol [0-21]          get/set handset volume\n";
        out += "  bellfreq [10-50]    get/set ring frequency (Hz)\n";
        out += "  digitgap [2-30]     get/set seconds allowed between dialled digits\n";
        out += "  linelevel [0-100]   get/set master line level (%)\n";
        out += "  coin [auto|on|off]  coin box override\n";
        out += "  play <path>         play an SD file\n";
        out += "  tone [hz] [secs]    play a steady sine tone (default 1000Hz 5s)\n";
        out += "  stop                stop playback\n";
        out += "  ls [path]           list SD directory\n";
        out += "  cat <path>          show a text file\n";
        out += "  rm <path>           delete a file\n";
        out += "  sd                  SD card info\n";
        out += "  mem                 free heap\n";
        out += "  uptime              time since boot\n";
        out += "  wifi                Wi-Fi connection info\n";
        out += "  selftest            run bring-up self-test checklist\n";
        out += "  probe               play 1kHz tone, report peak/RMS\n";
        out += "  calibrate on        capture ON-HOOK line level (handset down)\n";
        out += "  calibrate off       capture OFF-HOOK + set thresholds (handset up)\n";
        out += "  dialecho [on|off]   echo dialled digits on the panel lamp\n";
        out += "  ticks [on|off]      earpiece click on each rotary dial pulse\n";
        out += "  reboot              restart the device";
    } else if (verb == "status" || verb == "s") {
        int m = p.coinBox().overrideMode();
        out  = "state=" + String(p.stateName());
        out += "  hook=" + String(p.line().hookState() == HookState::OFF_HOOK ? "OFF_HOOK" : "ON_HOOK");
        out += "  line=" + String(p.line().lastRawReading());
        out += "  mode=" + String(p.autoRingEnabled() ? "AUTO" : "MANUAL");
        out += "  sd=" + String(p.player().sdReady() ? "OK" : "FAIL");
        out += "  vol=" + String(p.player().getVolume()) + "/21";
        out += "  coinbox=" + String(p.coinBox().isInstalled() ? "ACTIVE" : "OFF");
        out += "(" + String(m == 1 ? "forced-on" : m == 0 ? "forced-off" : "auto") + ")";
    } else if (verb == "ring") {
        p.ring(); out = "ringing";
    } else if (verb == "testring") {
        int secs = arg.length() ? arg.toInt() : 3;
        if (secs < 1) secs = 1; if (secs > 30) secs = 30;
        p.testRing(secs);
        out = "test ring for " + String(secs) + "s";
    } else if (verb == "hangup" || verb == "h") {
        p.hangUp(); out = "hung up";
    } else if (verb == "cancel" || verb == "c") {
        p.cancelRing(); out = "ring cancelled";
    } else if (verb == "mode") {
        if (larg == "auto")        p.setAutoRing(true);
        else if (larg == "manual") p.setAutoRing(false);
        else if (larg.length())    { server.send(200, "text/plain", "usage: mode [auto|manual]"); return; }
        else { server.send(200, "text/plain", String("mode=") + (p.autoRingEnabled() ? "AUTO" : "MANUAL")); return; }
        saveSettings();
        out = String("mode=") + (p.autoRingEnabled() ? "AUTO" : "MANUAL");
    } else if (verb == "vol") {
        if (arg.length()) {
            int v = arg.toInt(); if (v < 0) v = 0; if (v > 21) v = 21;
            p.player().setVolume(v); saveSettings();
        }
        out = "volume=" + String(p.player().getVolume()) + "/21";
    } else if (verb == "digitgap") {
        if (arg.length()) {
            int secs = arg.toInt(); if (secs < 2) secs = 2; if (secs > 30) secs = 30;
            p.setNumberCompleteMs((unsigned long)secs * 1000UL); saveSettings();
        }
        out = "inter-digit gap=" + String(p.numberCompleteMs() / 1000) + "s";
    } else if (verb == "bellfreq") {
        if (arg.length()) {
            int hz = arg.toInt(); if (hz < 10) hz = 10; if (hz > 50) hz = 50;
            p.bell().setRingFreq(hz); saveSettings();
        }
        out = "bell frequency=" + String(p.bell().ringFreq()) + " Hz";
    } else if (verb == "linelevel") {
        if (arg.length()) {
            int v = arg.toInt(); if (v < 0) v = 0; if (v > 100) v = 100;
            p.player().setLineLevel(v); saveSettings();
        }
        out = "line level=" + String(p.player().lineLevel()) + "%";
    } else if (verb == "coin") {
        if (larg == "auto")      p.coinBox().setOverride(-1);
        else if (larg == "on")   p.coinBox().setOverride(1);
        else if (larg == "off")  p.coinBox().setOverride(0);
        else if (larg.length())  { server.send(200, "text/plain", "usage: coin [auto|on|off]"); return; }
        if (larg.length()) saveSettings();
        int m = p.coinBox().overrideMode();
        out  = String("coin override=") + (m == 1 ? "on" : m == 0 ? "off" : "auto");
        out += "  active=" + String(p.coinBox().isInstalled() ? "yes" : "no");
    } else if (verb == "play") {
        if (!arg.length())           { server.send(200, "text/plain", "usage: play <path>"); return; }
        if (!p.player().sdReady())   { server.send(200, "text/plain", "error: SD not available"); return; }
        bool ok = p.player().playFile(arg.c_str(), false);
        out = ok ? ("playing " + arg) : ("error: could not play " + arg);
    } else if (verb == "tone") {
        if (!p.player().sdReady()) { server.send(200, "text/plain", "error: SD not available"); return; }
        // tone [hz] [secs] — default 1000 Hz for 5s
        int hz = 1000, secs = 5;
        if (larg.length()) {
            int sp2 = larg.indexOf(' ');
            if (sp2 >= 0) { hz = larg.substring(0, sp2).toInt(); secs = larg.substring(sp2 + 1).toInt(); }
            else hz = larg.toInt();
        }
        if (secs < 1) secs = 5;
        bool ok = p.player().playTestTone(hz, secs);
        out = ok ? ("playing " + String(hz) + " Hz tone for " + String(secs) + "s")
                 : "error: could not start tone";
    } else if (verb == "stop") {
        p.player().stop(); out = "playback stopped";
    } else if (verb == "ls") {
        if (!p.player().sdReady()) { server.send(200, "text/plain", "error: SD not available"); return; }
        String path = arg.length() ? arg : "/";
        File dir = SD.open(path);
        if (!dir || !dir.isDirectory()) { server.send(200, "text/plain", "error: not a directory: " + path); return; }
        out = path + ":\n";
        File f = dir.openNextFile();
        int n = 0;
        while (f) {
            out += f.isDirectory() ? "  [DIR] " : "        ";
            out += String(f.name());
            if (!f.isDirectory()) out += "  (" + String((unsigned long)f.size()) + " bytes)";
            out += "\n";
            f = dir.openNextFile();
            if (++n > 100) { out += "  ... (more)\n"; break; }
        }
        if (n == 0) out += "  (empty)";
    } else if (verb == "cat") {
        if (!arg.length())         { server.send(200, "text/plain", "usage: cat <path>"); return; }
        if (!p.player().sdReady()) { server.send(200, "text/plain", "error: SD not available"); return; }
        File f = SD.open(arg);
        if (!f || f.isDirectory()) { server.send(200, "text/plain", "error: cannot open " + arg); return; }
        const size_t MAXB = 2048;
        while (f.available() && out.length() < MAXB) out += (char)f.read();
        if (f.available()) out += "\n... (truncated)";
        f.close();
        if (out.length() == 0) out = "(empty file)";
    } else if (verb == "rm") {
        if (!arg.length())         { server.send(200, "text/plain", "usage: rm <path>"); return; }
        if (!p.player().sdReady()) { server.send(200, "text/plain", "error: SD not available"); return; }
        bool ok = SD.remove(arg);
        if (ok && s_logger) s_logger->systemLog("File deleted via terminal: %s", arg.c_str());
        out = ok ? ("deleted " + arg) : ("error: could not delete " + arg);
    } else if (verb == "sd") {
        if (!p.player().sdReady()) out = "SD card: NOT MOUNTED";
        else {
            out  = "SD card: OK  type=" + String(SD.cardType());
            out += "  size=" + String((unsigned long)(SD.cardSize() / (1024 * 1024))) + "MB";
            out += "  used=" + String((unsigned long)(SD.usedBytes() / (1024 * 1024))) + "MB";
        }
    } else if (verb == "mem" || verb == "heap") {
        out  = "free heap: " + String(ESP.getFreeHeap()) + " bytes";
        out += "  min free: " + String(ESP.getMinFreeHeap()) + " bytes";
    } else if (verb == "uptime") {
        unsigned long s = millis() / 1000;
        out = "uptime: " + String(s / 3600) + "h " + String((s % 3600) / 60) + "m " + String(s % 60) + "s";
    } else if (verb == "wifi") {
        if (s_ap_active) {
            out  = "mode: hosting AP\n";
            out += "AP SSID: " + WiFi.softAPSSID() + "\n";
            out += "AP IP: " + WiFi.softAPIP().toString() + "\n";
            out += "connected clients: " + String(WiFi.softAPgetStationNum());
        } else {
            out  = "mode: joined network\n";
            out += "SSID: " + WiFi.SSID() + "\n";
            out += "IP: " + WiFi.localIP().toString() + "\n";
            out += "RSSI: " + String(WiFi.RSSI()) + " dBm";
        }
    } else if (verb == "selftest" || verb == "test") {
        out = buildSelfTest(p);
    } else if (verb == "probe") {
        out = buildProbe(p);
    } else if (verb == "calibrate" || verb == "cal") {
        if (larg == "on" || larg.length() == 0) {
            s_cal_onhook = p.line().readAveraged(128);
            out  = "on-hook level=" + String(s_cal_onhook) + "\n";
            out += "now LIFT the handset and run: calibrate off";
        } else if (larg == "off") {
            if (s_cal_onhook < 0) {
                out = "error: run 'calibrate on' first (handset down)";
            } else {
                int off = p.line().readAveraged(128);
                if (p.line().applyCalibration(s_cal_onhook, off)) {
                    saveSettings();
                    out  = "calibrated: on-hook=" + String(s_cal_onhook);
                    out += " off-hook=" + String(off) + "\n";
                    out += "thresholds -> on>=" + String(p.line().thresholdOn());
                    out += " off<" + String(p.line().thresholdOff()) + " (saved)";
                } else {
                    out  = "FAILED: on-hook=" + String(s_cal_onhook) + " off-hook=" + String(off);
                    out += " — too close (need off-hook much higher). Check wiring.";
                }
                s_cal_onhook = -1;
            }
        } else {
            out = "usage: calibrate on | calibrate off";
        }
    } else if (verb == "dialecho") {
        if (larg == "on")       g_dial_confirm = true;
        else if (larg == "off") g_dial_confirm = false;
        else if (larg.length()) { server.send(200, "text/plain", "usage: dialecho [on|off]"); return; }
        else g_dial_confirm = !g_dial_confirm;
        out = String("dial echo ") + (g_dial_confirm ? "ON" : "OFF");
    } else if (verb == "ticks") {
        if (larg == "on")       s_phone->setDialTicks(true);
        else if (larg == "off") s_phone->setDialTicks(false);
        else if (larg.length()) { server.send(200, "text/plain", "usage: ticks [on|off]"); return; }
        else s_phone->setDialTicks(!s_phone->dialTicks());
        saveSettings();
        out = String("dial ticks ") + (s_phone->dialTicks() ? "ON" : "OFF");
    } else if (verb == "reboot") {
        if (s_logger) s_logger->systemLog("Reboot via terminal");
        server.send(200, "text/plain", "rebooting…");
        delay(300);
        ESP.restart();
        return;
    } else {
        out = "unknown command: " + verb + "  (type 'help')";
    }

    server.send(200, "text/plain", out);
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
    json += ",\"pickups\":";       json += String(st.total_pickups);
    json += ",\"completions\":";   json += String(st.total_completions);
    json += ",\"first_digit_ms\":"; json += String(st.total_first_digit_ms);
    json += ",\"first_digit_n\":";  json += String(st.first_digit_count);

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
    json += "]";

    // Since-boot ("today") summary — the exhibit is powered down out of hours.
    const SessionStats& se = s_stats->session();
    json += ",\"session\":{";
    json += "\"incoming\":";       json += String(se.incoming);
    json += ",\"outgoing\":";      json += String(se.outgoing);
    json += ",\"answered\":";      json += String(se.answered);
    json += ",\"not_recognised\":"; json += String(se.not_recognised);
    json += ",\"pickups\":";       json += String(se.pickups);
    json += ",\"completions\":";   json += String(se.completions);
    json += ",\"uptime\":";        json += String(session_secs);
    StatsTracker::NumberEntry stop[5];
    int sn = s_stats->topSessionNumbers(stop, 5);
    json += ",\"top_numbers\":[";
    for (int i = 0; i < sn; i++) {
        if (i > 0) json += ",";
        json += "{\"number\":\"";
        json += stop[i].number;
        json += "\",\"count\":";
        json += String(stop[i].count);
        json += "}";
    }
    json += "]}";
    json += "}";

    server.send(200, "application/json", json);
}

static void handleStatsReset() {
    if (s_stats) {
        s_stats->resetStats();
        if (s_logger) s_logger->systemLog("Stats reset via web");
    }
    server.send(200, "application/json", "{\"ok\":true}");
}

// --- Diagnostics API handler ------------------------------------------------

static void handleDiagnostics() {
    if (!s_stats) { server.send(200, "application/json", "[]"); return; }

    int count = s_stats->errorCount();
    const StatsTracker::ErrorEntry* entries = s_stats->errorEntries();

    String json = "[";
    for (int i = 0; i < count; i++) {
        if (i > 0) json += ",";
        json += "{\"time\":";
        json += String(entries[i].timestamp);
        json += ",\"type\":\"";
        switch (entries[i].type) {
            case StatsTracker::ErrorType::BELL_FAULT:  json += "BELL_FAULT"; break;
            case StatsTracker::ErrorType::LINE_ANOMALY: json += "LINE_ANOMALY"; break;
            case StatsTracker::ErrorType::SD_FAILURE:   json += "SD_FAILURE"; break;
        }
        json += "\"";
        if (entries[i].detail[0]) {
            json += ",\"detail\":\"";
            // Escape any quotes in detail string.
            for (const char* p = entries[i].detail; *p; p++) {
                if (*p == '"') json += "\\\"";
                else if (*p == '\\') json += "\\\\";
                else json += *p;
            }
            json += "\"";
        }
        json += "}";
    }
    json += "]";

    server.send(200, "application/json", json);
}

// Persisted diagnostics: boot-time line-sense readings (drift) + last self-test.
static void handleDiag() {
    if (!s_stats) { server.send(200, "application/json", "{}"); return; }

    String json = "{\"boot_lines\":[";
    int n = s_stats->bootLineCount();
    const StatsTracker::BootLineEntry* bl = s_stats->bootLineEntries();
    for (int i = 0; i < n; i++) {
        if (i > 0) json += ",";
        json += "{\"boot\":"; json += String(bl[i].boot);
        json += ",\"raw\":";  json += String(bl[i].raw);
        json += "}";
    }
    json += "],\"selftest_uptime\":";
    json += String(s_stats->lastSelfTestUptime());
    json += ",\"selftest\":\"";
    for (const char* p = s_stats->lastSelfTest(); *p; p++) {
        if (*p == '"') json += "\\\"";
        else if (*p == '\\') json += "\\\\";
        else if (*p == '\n') json += "\\n";
        else if (*p == '\r') { /* skip */ }
        else json += *p;
    }
    json += "\"}";
    server.send(200, "application/json", json);
}

// --- Discovery log API handlers ---------------------------------------------

static void handleDiscovery() {
    if (!s_stats) { server.send(200, "application/json", "[]"); return; }

    int count = s_stats->discoveryCount();
    const StatsTracker::NumberEntry* entries = s_stats->discoveryEntries();

    String json = "[";
    for (int i = 0; i < count; i++) {
        if (i > 0) json += ",";
        json += "{\"number\":\"";
        json += entries[i].number;
        json += "\",\"count\":";
        json += String(entries[i].count);
        json += "}";
    }
    json += "]";
    server.send(200, "application/json", json);
}

static void handleDiscoveryClear() {
    if (s_stats) {
        s_stats->clearDiscovery();
        if (s_logger) s_logger->systemLog("Discovery log cleared via web");
    }
    server.send(200, "application/json", "{\"ok\":true}");
}

static void handleDiscoveryRemove() {
    if (s_stats) {
        String number = server.arg("number");
        if (number.length() > 0) {
            s_stats->removeDiscovery(number.c_str());
            if (s_logger) s_logger->systemLog("Discovery entry removed: %s", number.c_str());
        }
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

    String lower = path;
    lower.toLowerCase();
    const char* mime = lower.endsWith(".wav") ? "audio/wav" : "audio/mpeg";
    server.streamFile(f, mime);
    f.close();
}

// --- Settings persistence ---------------------------------------------------

static const char* SETTINGS_FILE = "/system/settings.json";
static const char* SETTINGS_TMP  = "/system/settings.tmp";

static void loadSettings() {
    if (!s_phone) return;
    // Recover from interrupted save.
    if (!SD.exists(SETTINGS_FILE) && SD.exists(SETTINGS_TMP)) {
        SD.rename(SETTINGS_TMP, SETTINGS_FILE);
    }
    File f = SD.open(SETTINGS_FILE, FILE_READ);
    if (!f) return;

    JsonDocument doc;
    if (deserializeJson(doc, f)) { f.close(); return; }
    f.close();

    if (!doc["volume"].isNull())   s_phone->player().setVolume(doc["volume"].as<uint8_t>());
    if (!doc["digit_gap"].isNull()) s_phone->setNumberCompleteMs(doc["digit_gap"].as<unsigned long>());
    s_wifi_sta = doc["wifi_sta"].as<bool>();
    if (!doc["wifi_ssid"].isNull()) s_sta_ssid = doc["wifi_ssid"].as<const char*>();
    if (!doc["wifi_pass"].isNull()) s_sta_pass = doc["wifi_pass"].as<const char*>();
    if (!doc["ar_min"].isNull() && !doc["ar_max"].isNull()) {
        s_phone->setAutoRingInterval(
            doc["ar_min"].as<unsigned long>(),
            doc["ar_max"].as<unsigned long>());
    }
    if (!doc["ring_max"].isNull()) s_phone->setMaxRingCadences(doc["ring_max"].as<int>());
    if (!doc["rt_min"].isNull() && !doc["rt_max"].isNull())
        s_phone->setRingToneRange(doc["rt_min"].as<int>(), doc["rt_max"].as<int>());
    if (!doc["alert_idle"].isNull()) s_phone->setAlertIdleMinutes(doc["alert_idle"].as<int>());
    if (!doc["coin_override"].isNull()) s_phone->coinBox().setOverride(doc["coin_override"].as<int>());
    if (!doc["bell_freq"].isNull()) s_phone->bell().setRingFreq(doc["bell_freq"].as<int>());
    if (!doc["line_level"].isNull()) s_phone->player().setLineLevel(doc["line_level"].as<uint8_t>());
    if (!doc["line_on"].isNull() && !doc["line_off"].isNull())
        s_phone->line().setThresholds(doc["line_on"].as<int>(), doc["line_off"].as<int>());
    if (!doc["line_cal"].isNull()) s_phone->line().setCalibrated(doc["line_cal"].as<bool>());
    if (!doc["dial_ticks"].isNull()) s_phone->setDialTicks(doc["dial_ticks"].as<bool>());
    Serial.println("[web] settings loaded");
}

static void saveSettings() {
    if (!s_phone) return;
    if (!SD.exists("/system")) SD.mkdir("/system");

    // Write to temp file first, then rename for crash-safe update.
    File f = SD.open(SETTINGS_TMP, FILE_WRITE);
    if (!f) return;

    JsonDocument doc;
    doc["volume"]   = s_phone->player().getVolume();
    doc["digit_gap"] = s_phone->numberCompleteMs();
    doc["wifi_sta"]  = s_wifi_sta;
    doc["wifi_ssid"] = s_sta_ssid;
    doc["wifi_pass"] = s_sta_pass;
    doc["ar_min"]   = s_phone->autoRingMinMs();
    doc["ar_max"]   = s_phone->autoRingMaxMs();
    doc["ring_max"] = s_phone->maxRingCadences();
    doc["rt_min"] = s_phone->ringToneMinSecs();
    doc["rt_max"] = s_phone->ringToneMaxSecs();
    doc["alert_idle"] = s_phone->alertIdleMinutes();
    doc["coin_override"] = s_phone->coinBox().overrideMode();
    doc["bell_freq"] = s_phone->bell().ringFreq();
    doc["line_level"] = s_phone->player().lineLevel();
    doc["line_on"]  = s_phone->line().thresholdOn();
    doc["line_off"] = s_phone->line().thresholdOff();
    doc["line_cal"] = s_phone->line().calibrated();
    doc["dial_ticks"] = s_phone->dialTicks();
    serializeJson(doc, f);
    f.flush();
    f.close();

    SD.remove(SETTINGS_FILE);
    SD.rename(SETTINGS_TMP, SETTINGS_FILE);
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

    // Load persisted settings first so Wi-Fi comes up in the configured mode.
    loadSettings();

    // Station mode: try to join the configured network. If it doesn't connect
    // within the timeout, fall back to hosting our own AP so the operator can
    // always reach the portal and correct the credentials.
    bool staOk = false;
    if (s_wifi_sta && s_sta_ssid.length()) {
        Serial.printf("[web] joining Wi-Fi \"%s\"...\n", s_sta_ssid.c_str());
        WiFi.mode(WIFI_STA);
        WiFi.begin(s_sta_ssid.c_str(), s_sta_pass.c_str());
        unsigned long t0 = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) delay(250);
        staOk = (WiFi.status() == WL_CONNECTED);
    }

    if (staOk) {
        s_ap_active = false;
        Serial.printf("[web] joined \"%s\" — http://%s/\n",
                      s_sta_ssid.c_str(), WiFi.localIP().toString().c_str());
    } else {
        if (s_wifi_sta) Serial.println("[web] Wi-Fi join failed — hosting own AP instead");
        s_ap_active = true;
        WiFi.mode(WIFI_AP);
        WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASS);
        delay(100);
        Serial.printf("[web] AP \"%s\" started — http://%s/\n",
                      WIFI_AP_SSID, WiFi.softAPIP().toString().c_str());
    }

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
    server.on("/api/rollback",    HTTP_POST, handleRollback);
    server.on("/api/logs/system", HTTP_GET,  handleLogSystem);
    server.on("/api/logs/calls",  HTTP_GET,  handleLogCalls);
    server.on("/api/logs/clear",  HTTP_POST, handleLogClear);
    server.on("/api/volume",      HTTP_POST, handleVolume);
    server.on("/api/digitgap",    HTTP_POST, handleDigitGap);
    server.on("/api/wifi",        HTTP_POST, handleWifi);
    server.on("/api/bellfreq",    HTTP_POST, handleBellFreq);
    server.on("/api/linelevel",   HTTP_POST, handleLineLevel);
    server.on("/api/autoring",    HTTP_POST, handleAutoRing);
    server.on("/api/ring",        HTTP_POST, handleRingNow);
    server.on("/api/testring",    HTTP_POST, handleTestRing);
    server.on("/api/tone",        HTTP_POST, handleTone);
    server.on("/api/ringcount",  HTTP_POST, handleRingCount);
    server.on("/api/ringtone",   HTTP_POST, handleRingTone);
    server.on("/api/alertidle",  HTTP_POST, handleAlertIdle);
    server.on("/api/coinmode",   HTTP_POST, handleCoinMode);
    server.on("/api/mode",        HTTP_POST, handleToggleMode);
    server.on("/api/terminal",    HTTP_POST, handleTerminal);
    server.on("/api/stats",       HTTP_GET,  handleStats);
    server.on("/api/stats/reset", HTTP_POST, handleStatsReset);
    server.on("/api/diagnostics", HTTP_GET,  handleDiagnostics);
    server.on("/api/diag",        HTTP_GET,  handleDiag);
    server.on("/api/discovery",       HTTP_GET,  handleDiscovery);
    server.on("/api/discovery/clear",  HTTP_POST, handleDiscoveryClear);
    server.on("/api/discovery/remove", HTTP_POST, handleDiscoveryRemove);
    server.on("/api/aliases",     HTTP_GET,  handleGetAliases);
    server.on("/api/aliases",     HTTP_POST, handleSaveAliases);
    server.on("/api/preview",     HTTP_GET,  handlePreview);
    server.on("/api/reboot",      HTTP_POST, handleReboot);
    server.on("/manifest.json",   HTTP_GET,  handleManifest);
    server.on("/sw.js",           HTTP_GET,  handleServiceWorker);

    // Silence the "request handler not found" spam from favicon/OS captive-
    // portal probes: redirect stray GETs to the portal, 404 everything else.
    server.onNotFound([]() {
        if (server.method() == HTTP_GET && !server.uri().startsWith("/api/")) {
            server.sendHeader("Location",
                              String("http://") + currentIP().toString() + "/");
            server.send(302, "text/plain", "redirecting");
        } else {
            server.send(404, "text/plain", "not found");
        }
    });

    server.begin();
    active_ = true;
    if (s_ap_active) logger.systemLog("Wi-Fi AP started SSID=%s", WIFI_AP_SSID);
    else             logger.systemLog("Wi-Fi joined SSID=%s ip=%s",
                                      s_sta_ssid.c_str(), WiFi.localIP().toString().c_str());
    Serial.println("[web] server ready");
}

void WebManager::update() {
    if (active_) server.handleClient();
}

void WebManager::persistSettings() {
    saveSettings();
}

String WebManager::selfTest() {
    return s_phone ? buildSelfTest(*s_phone) : String("phone not ready");
}

String WebManager::audioProbe() {
    return s_phone ? buildProbe(*s_phone) : String("phone not ready");
}
