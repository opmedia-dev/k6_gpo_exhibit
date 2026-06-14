#include "logger.h"
#include "config.h"
#include <SD.h>
#include <stdarg.h>

static const char* SYS_LOG  = "/logs/system.log";
static const char* CALL_LOG = "/logs/calls.log";
static const size_t MAX_LOG_SIZE = 65536;  // 64 KB per log file

void Logger::begin() {
    if (!SD.exists("/logs")) {
        SD.mkdir("/logs");
    }
    systemLog("BOOT firmware=%s heap=%u", FIRMWARE_VERSION, ESP.getFreeHeap());
}

void Logger::systemLog(const char* fmt, ...) {
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    appendLog(SYS_LOG, buf);
    Serial.printf("[syslog] %s\n", buf);
}

void Logger::callLog(const char* fmt, ...) {
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    appendLog(CALL_LOG, buf);
    Serial.printf("[calllog] %s\n", buf);
}

char* Logger::readSystemLog(size_t* len) {
    File f = SD.open(SYS_LOG, FILE_READ);
    if (!f) { *len = 0; return nullptr; }
    *len = f.size();
    char* buf = (char*)malloc(*len + 1);
    if (buf) {
        f.read((uint8_t*)buf, *len);
        buf[*len] = '\0';
    }
    f.close();
    return buf;
}

char* Logger::readCallLog(size_t* len) {
    File f = SD.open(CALL_LOG, FILE_READ);
    if (!f) { *len = 0; return nullptr; }
    *len = f.size();
    char* buf = (char*)malloc(*len + 1);
    if (buf) {
        f.read((uint8_t*)buf, *len);
        buf[*len] = '\0';
    }
    f.close();
    return buf;
}

void Logger::clearSystemLog() {
    SD.remove(SYS_LOG);
}

void Logger::clearCallLog() {
    SD.remove(CALL_LOG);
}

void Logger::appendLog(const char* path, const char* line) {
    rotateIfNeeded(path);
    File f = SD.open(path, FILE_APPEND);
    if (!f) return;
    String ts = timestamp();
    f.printf("[%s] %s\n", ts.c_str(), line);
    f.close();
}

void Logger::rotateIfNeeded(const char* path) {
    File f = SD.open(path, FILE_READ);
    if (!f) return;
    size_t sz = f.size();
    f.close();

    if (sz < MAX_LOG_SIZE) return;

    // Keep only the second half of the file.
    f = SD.open(path, FILE_READ);
    size_t skip = sz / 2;
    f.seek(skip);

    // Find the next newline to avoid splitting a line.
    while (f.available()) {
        char c = f.read();
        skip++;
        if (c == '\n') break;
    }

    size_t keep = sz - skip;
    char* buf = (char*)malloc(keep);
    if (!buf) { f.close(); return; }

    f.read((uint8_t*)buf, keep);
    f.close();

    // Rewrite the file with just the kept portion.
    f = SD.open(path, FILE_WRITE);
    if (f) {
        f.write((uint8_t*)buf, keep);
        f.close();
    }
    free(buf);
}

String Logger::timestamp() {
    unsigned long s = millis() / 1000;
    unsigned long h = s / 3600;
    unsigned long m = (s % 3600) / 60;
    unsigned long sec = s % 60;
    char buf[16];
    snprintf(buf, sizeof(buf), "%03lu:%02lu:%02lu", h, m, sec);
    return String(buf);
}
