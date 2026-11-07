// Livery Decals for Forza Horizon 3 (FH3 Mod Platform mod).
//
// Every .png in fh3-mods\liverymod\ becomes a new decal in the livery editor: Apply a Manufacturer Decal. Nothing of the
// game's own decals is replaced; take a PNG out of the folder and its decal is gone from the menu on the next start.
//
// How: the decal menu is built from the game database. Livery_Decals lists the decals (a .modelbin each), and
// Livery_DecalsSortOrder gives every manufacturer its own order of them. For each PNG this mod
//  - makes a .swatchbin texture (the picture fitted into a 256, 512 or 1024 square, BC7 with mips, in the console's tiled layout) and a
//    .modelbin (the game's square decal quad, pointing at that texture) and hands both to the game with set_file, as
//    media\livery\Decals.zip\Textures\<name>.swatchbin and media\livery\Decals.zip\<name>.modelbin,
//  - adds a Livery_Decals row, and a Livery_DecalsSortOrder row for every manufacturer, after the game's own decals.
// A decal's id comes from its file name, so a decal keeps its id (and liveries that use it keep working) as long as the
// file keeps its name.
#include "fh3mod.h"

#include <windows.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include "../third_party/stb_image.h"

#include "swatch.hpp"

#include <algorithm>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace {

const FH3ModApi* g_api = nullptr;
FH3Mod* g_mod          = nullptr;

bool g_verbose       = false;  // LiveryMod.ini [LiveryMod] Log=1
bool g_premultiplied = false;  // LiveryMod.ini [LiveryMod] Premultiplied=1: store the colour multiplied by alpha
int g_max_size       = 1024;   // LiveryMod.ini [LiveryMod] Size: 256, 512 or 1024, the texture size of every decal

constexpr uint32_t kIdBase  = 10000;  // our ids are kIdBase .. kIdBase + kIdRange - 1 (the game's own end at 3023)
constexpr uint32_t kIdRange = 50000;
constexpr int kSeqBase      = 1000;   // sort order after the game's own decals (they stop at 54)
constexpr size_t kMaxFile   = 64u << 20;
constexpr int kMaxDecals    = 400;

void say(const char* fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    g_api->log(g_mod, buf);
}

std::string to_utf8(const std::wstring& w) {
    if (w.empty()) return {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), int(w.size()), nullptr, 0, nullptr, nullptr);
    std::string s(size_t(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), int(w.size()), s.data(), n, nullptr, nullptr);
    return s;
}

bool read_file(const std::wstring& path, std::vector<uint8_t>& out) {
    HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER sz{};
    bool ok = GetFileSizeEx(f, &sz) && sz.QuadPart > 0 && sz.QuadPart <= static_cast<LONGLONG>(kMaxFile);
    if (ok) {
        out.resize(size_t(sz.QuadPart));
        size_t got = 0;
        while (got < out.size()) {
            DWORD n = 0;
            if (!ReadFile(f, out.data() + got, DWORD(std::min<size_t>(out.size() - got, 1u << 24)), &n, nullptr) || n == 0) break;
            got += n;
        }
        ok = got == out.size();
    }
    CloseHandle(f);
    return ok;
}

std::wstring lower(std::wstring s) {
    for (wchar_t& c : s) c = static_cast<wchar_t>(towlower(c));
    return s;
}

uint32_t fnv1a(const std::wstring& s) {
    uint32_t h = 2166136261u;
    for (wchar_t c : s) {
        h = (h ^ (c & 0xFF)) * 16777619u;
        h = (h ^ ((c >> 8) & 0xFF)) * 16777619u;
    }
    return h;
}

std::string sql_text(const std::string& s) {
    std::string o = "'";
    for (char c : s) {
        if (c == '\'') o += "''";
        else if (c != '\0') o += c;
    }
    return o + "'";
}

std::wstring ini_path() { return std::wstring(g_api->azimuth_dir) + L"\\fh3-mods\\LiveryMod.ini"; }

void load_config() {
    const std::wstring p = ini_path();
    if (GetFileAttributesW(p.c_str()) == INVALID_FILE_ATTRIBUTES) {
        FILE* f = _wfopen(p.c_str(), L"wb");
        if (f) {
            std::fputs("[LiveryMod]\r\n"
                       "; 1 = write more to platform.log (which PNG became which decal)\r\n"
                       "Log=0\r\n"
                       "; 1 = store the colours multiplied by alpha (try it if the decals show dark or light edges in the game)\r\n"
                       "Premultiplied=0\r\n"
                       "; the texture size of every decal: 256, 512 or 1024 (every PNG is fitted into it; 256 is what the game's own decals use)\r\n"
                       "Size=1024\r\n",
                       f);
            std::fclose(f);
        }
    }
    g_verbose       = GetPrivateProfileIntW(L"LiveryMod", L"Log", 0, p.c_str()) != 0;
    g_premultiplied = GetPrivateProfileIntW(L"LiveryMod", L"Premultiplied", 0, p.c_str()) != 0;
    const int ms    = GetPrivateProfileIntW(L"LiveryMod", L"Size", 1024, p.c_str());
    g_max_size      = ms >= 1024 ? 1024 : ms >= 512 ? 512 : 256;
}

struct Decal {
    std::wstring file;   // file name with .png
    std::string stem;    // UTF-8, without .png
    uint32_t id = 0;
    std::string name8;   // "lm012345", what the game's files are called
};

void install() {
    const std::wstring dir = std::wstring(g_api->azimuth_dir) + L"\\fh3-mods\\liverymod";
    CreateDirectoryW((std::wstring(g_api->azimuth_dir) + L"\\fh3-mods").c_str(), nullptr);
    CreateDirectoryW(dir.c_str(), nullptr);

    std::vector<Decal> list;
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((dir + L"\\*.png").c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            std::wstring f = fd.cFileName;
            if (f.size() < 5 || lower(f.substr(f.size() - 4)) != L".png") continue;  // the *.png search also finds 8.3 names like x.png_
            Decal d;
            d.file = f;
            d.stem = to_utf8(f.substr(0, f.size() - 4));
            list.push_back(std::move(d));
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    std::sort(list.begin(), list.end(), [](const Decal& a, const Decal& b) { return lower(a.file) < lower(b.file); });
    if (list.size() > size_t(kMaxDecals)) {
        say("%zu PNGs found, using the first %d", list.size(), kMaxDecals);
        list.resize(kMaxDecals);
    }

    std::vector<bool> used(kIdRange, false);
    std::string sql;
    int ok = 0, seq = 0;
    for (Decal& d : list) {
        std::vector<uint8_t> bytes;
        if (!read_file(dir + L"\\" + d.file, bytes)) { say("%s: can't read it (empty, too big, or open in another program)", d.stem.c_str()); continue; }
        int w = 0, hgt = 0, comp = 0;
        stbi_uc* px = stbi_load_from_memory(bytes.data(), int(bytes.size()), &w, &hgt, &comp, 4);
        if (!px) { say("%s.png: not a PNG the mod can read (%s)", d.stem.c_str(), stbi_failure_reason()); continue; }
        if (w <= 0 || hgt <= 0 || w > 32768 || hgt > 32768) { stbi_image_free(px); say("%s.png: %dx%d is outside what the mod accepts", d.stem.c_str(), w, hgt); continue; }

        // the id comes from the name: stable across starts and across adding / removing other PNGs
        uint32_t slot = fnv1a(lower(d.file)) % kIdRange;
        while (used[slot]) slot = (slot + 1) % kIdRange;
        used[slot] = true;
        d.id = kIdBase + slot;
        char n8[16];
        std::snprintf(n8, sizeof n8, "lm%06u", d.id);
        d.name8 = n8;

        const int size = g_max_size;  // every decal gets the same texture size; a smaller picture is scaled up to it
        const std::vector<uint8_t> tex = swatch::make_swatchbin(px, w, hgt, g_premultiplied, size);
        stbi_image_free(px);
        const std::vector<uint8_t> mdl = swatch::make_modelbin(d.name8);
        const std::string tex_path = "livery/Decals/Textures/" + d.name8 + ".swatchbin";
        const std::string mdl_path = "livery/Decals/" + d.name8 + ".modelbin";
        const int a = g_api->set_file(g_mod, tex_path.c_str(), tex.data(), tex.size());
        const int b = g_api->set_file(g_mod, mdl_path.c_str(), mdl.data(), mdl.size());
        if (!a || !b) { say("%s.png: the game files couldn't be written (see the lines above)", d.stem.c_str()); continue; }

        ++seq;
        char row[640];
        std::snprintf(row, sizeof row, "INSERT INTO Livery_Decals (ID, Filename, DisplayName, AlphaSort) VALUES (%u, 'GAME:\\Media\\Livery\\Decals\\%s.modelbin', %s, %s);\n",
                      d.id, d.name8.c_str(), sql_text(d.stem).c_str(), sql_text(d.stem).c_str());
        sql += row;
        std::snprintf(row, sizeof row,
                      "INSERT INTO Livery_DecalsSortOrder (MakeID, Sequence, Livery_DecalID) SELECT DISTINCT MakeID, %d, %u FROM Livery_DecalsSortOrder WHERE Sequence < %d;\n",
                      kSeqBase + seq, d.id, kSeqBase);
        sql += row;
        ++ok;
        if (g_verbose) say("%s.png (%dx%d) -> decal %u (%s), texture %d", d.stem.c_str(), w, hgt, d.id, d.name8.c_str(), size);
    }
    if (!sql.empty()) g_api->add_sql(g_mod, sql.c_str());
    if (g_verbose) say("%d decal(s) from fh3-mods\\liverymod", ok);
}

}  // namespace

FH3MOD_EXPORT int fh3mod_init(const FH3ModApi* api) {
    if (api->api_version < 2) return 1;
    g_api = api;

    FH3ModInfo info{};
    info.size             = sizeof info;
    info.name             = "Livery Decals";
    info.version          = LIVERYMOD_VERSION;
    info.github_repo      = "Levilente/LiveryMod_FH3";
    info.platform_version = FH3MOD_PLATFORM_VERSION;
    info.check_updates    = 1;
    info.description      = "Every PNG in fh3-mods\\liverymod becomes a new decal in the livery editor (Apply a Manufacturer Decal).";
    g_mod                 = api->register_mod(&info);
    if (!g_mod) return 1;

    load_config();
    install();
    return 0;
}
