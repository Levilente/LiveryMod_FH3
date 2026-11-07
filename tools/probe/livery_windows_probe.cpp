// Throwaway diagnostic for the livery-window-decals research. Logs how the livery editor builds its tabs and
// section records to fh3-mods\LiveryWindowsProbe.log. Changes nothing in the game.
#include "fh3mod.h"
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <mutex>
#include <set>
#include <thread>

namespace {
const FH3ModApi* g_api; FH3Mod* g_mod; uint8_t* g_base;
std::mutex g_mx; FILE* g_f; std::set<int> g_seen_rec, g_seen_tab; bool g_dumped; int g_valid_calls, g_rec_calls, g_tab_calls;

void say(const char* fmt, ...) {
    std::lock_guard<std::mutex> l(g_mx);
    if (!g_f) return;
    va_list ap; va_start(ap, fmt); std::vfprintf(g_f, fmt, ap); va_end(ap); std::fputc('\n', g_f); std::fflush(g_f);
}
bool rd(const void* p, void* out, size_t n) { SIZE_T got = 0; return ReadProcessMemory(GetCurrentProcess(), p, out, n, &got) && got == n; }
void hex(const char* tag, const void* p, size_t n) {
    uint8_t b[256]; if (n > sizeof b) n = sizeof b;
    if (!rd(p, b, n)) { say("%s @%p: unreadable", tag, p); return; }
    char line[600]; int k = 0;
    for (size_t i = 0; i < n; ++i) k += std::snprintf(line + k, sizeof line - k, "%02x%s", b[i], (i % 8 == 7) ? " " : "");
    say("%s @%p: %s", tag, p, line);
}
void dump_table() {
    if (g_dumped) return; g_dumped = true;
    uint64_t count = 0; rd(g_base + 0x4571DE0, &count, 8);
    say("tab table @base+0x4E2A460, count var @base+0x4571DE0 = %llu", (unsigned long long)count);
    for (int i = 0; i < 8; ++i) {
        uint8_t* e = g_base + 0x4E2A460 + i * 0x18; hex("  entry", e, 0x18);
        uint64_t obj = 0; if (rd(e + 8, &obj, 8) && obj) hex("    text obj", (void*)obj, 0x40);
    }
}

typedef bool (*ValidFn)(void*, int); ValidFn o_valid;
typedef void* (*RecFn)(void*, int);  RecFn o_rec;
typedef void* (*TabFn)(void*, void*, int, void*, void*); TabFn o_tab;

bool d_valid(void* ed, int id) {
    bool r = o_valid(ed, id);
    if (g_valid_calls++ < 200) say("IsSectionValid(editor=%p, id=%d) -> %d", ed, id, (int)r);
    dump_table();
    return r;
}
void* d_rec(void* ctx, int id) {
    void* r = o_rec(ctx, id);
    if (g_rec_calls++ < 400 && g_seen_rec.insert(id).second) { say("SectionRecord(ctx=%p, id=%d) -> %p", ctx, id, r); if (r) hex("  record", r, 0x90); }
    return r;
}
void* d_tab(void* page, void* out, int id, void* text, void* extra) {
    if (g_tab_calls++ < 200) say("MakeTab(page=%p, out=%p, id=%d, text=%p, extra=%p)", page, out, id, text, extra);
    return o_tab(page, out, id, text, extra);
}

void worker() {
    void* base = g_api->wait_for_code(120000);
    if (!base) { say("wait_for_code timed out"); return; }
    g_base = (uint8_t*)base;
    say("game base %p", base);
    FH3Hook* h1 = g_api->hook_function(g_mod, "IsSectionValid", g_base + 0x3823c0, (void*)d_valid, (void**)&o_valid);
    FH3Hook* h2 = g_api->hook_function(g_mod, "SectionRecord",  g_base + 0x1c8cc70, (void*)d_rec,   (void**)&o_rec);
    FH3Hook* h3 = g_api->hook_function(g_mod, "MakeTab",        g_base + 0x14c7cc0, (void*)d_tab,   (void**)&o_tab);
    say("hooks: %p %p %p", (void*)h1, (void*)h2, (void*)h3);
    say("enable -> %d", g_api->enable_hooks(g_mod, nullptr));
}
}  // namespace

FH3MOD_EXPORT int fh3mod_init(const FH3ModApi* api) {
    if (api->api_version < 3) return 1;
    g_api = api;
    FH3ModInfo info{}; info.size = sizeof info; info.name = "Livery Windows Probe"; info.version = "0.0.1";
    info.platform_version = FH3MOD_PLATFORM_VERSION; info.description = "Diagnostic for the livery window decals research. Logs only.";
    g_mod = api->register_mod(&info);
    if (!g_mod) return 1;
    std::wstring p = std::wstring(api->azimuth_dir) + L"\\fh3-mods\\LiveryWindowsProbe.log";
    g_f = _wfopen(p.c_str(), L"wb");
    std::thread(worker).detach();
    return 0;
}
