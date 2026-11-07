// Host-side test tool: swatch_tool in.png out.swatchbin out.modelbin name8   (also checks the modelbin against the game's)
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "../third_party/stb_image.h"
#include "../src/swatch.hpp"
#include <cstdio>
int main(int c, char** v) {
    if (c < 5) return 2;
    int w, h, n;
    unsigned char* d = stbi_load(v[1], &w, &h, &n, 4);
    if (!d) { std::puts("load failed"); return 1; }
    auto s = swatch::make_swatchbin(d, w, h, false);
    auto m = swatch::make_modelbin(c > 4 ? v[4] : "lm000001");
    FILE* f = fopen(v[2], "wb"); fwrite(s.data(), 1, s.size(), f); fclose(f);
    f = fopen(v[3], "wb"); fwrite(m.data(), 1, m.size(), f); fclose(f);
    std::printf("%dx%d -> %zu bytes, model %zu\n", w, h, s.size(), m.size());
}
