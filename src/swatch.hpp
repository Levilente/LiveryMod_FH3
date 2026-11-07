// PNG pixels -> FH3 decal .swatchbin (256x256 BC7, 9 mips, 1D tiled), and the matching .modelbin.
// No Windows code in here, so it can be built and tested on any machine.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "modelbin_template.h"
#include "swatch_header.h"

namespace swatch {

constexpr int kSize = 256;
constexpr int kMips = 9;

struct Rgba {
    float r, g, b, a;  // 0..255, straight alpha
};

// ---- picture to 256x256 ------------------------------------------------------------------------------------------

// Fits `src` (w*h RGBA8, straight alpha, any size) into 256x256 keeping its proportions, centred on a transparent
// canvas, the way the game's own wide logos sit in their square textures. Resampling is done on premultiplied colour
// (a box filter when shrinking, bilinear when growing), so edges don't pick up the colour of transparent pixels.
inline std::vector<Rgba> fit_to_square(const uint8_t* src, int w, int h) {
    std::vector<Rgba> out(kSize * kSize, Rgba{0, 0, 0, 0});
    if (w <= 0 || h <= 0) return out;
    const double s = std::min(double(kSize) / w, double(kSize) / h);
    const int dw = std::max(1, std::min(kSize, int(std::lround(w * s))));
    const int dh = std::max(1, std::min(kSize, int(std::lround(h * s))));
    const int ox = (kSize - dw) / 2, oy = (kSize - dh) / 2;
    auto px = [&](int x, int y, double& r, double& g, double& b, double& a) {
        x = std::clamp(x, 0, w - 1);
        y = std::clamp(y, 0, h - 1);
        const uint8_t* p = src + (size_t(y) * w + x) * 4;
        a = p[3] / 255.0;
        r = p[0] * a;
        g = p[1] * a;
        b = p[2] * a;
    };
    const double sx = double(w) / dw, sy = double(h) / dh;
    for (int y = 0; y < dh; ++y) {
        for (int x = 0; x < dw; ++x) {
            double R = 0, G = 0, B = 0, A = 0;
            if (sx > 1.0 || sy > 1.0) {
                // area average over the source box this pixel covers (fractional edges weighted)
                const double x0 = x * sx, x1 = (x + 1) * sx, y0 = y * sy, y1 = (y + 1) * sy;
                double wsum = 0;
                for (int yy = int(std::floor(y0)); yy < int(std::ceil(y1)) && yy < h; ++yy) {
                    const double wy = std::min<double>(yy + 1, y1) - std::max<double>(yy, y0);
                    for (int xx = int(std::floor(x0)); xx < int(std::ceil(x1)) && xx < w; ++xx) {
                        const double wx = std::min<double>(xx + 1, x1) - std::max<double>(xx, x0);
                        double r, g, b, a;
                        px(xx, yy, r, g, b, a);
                        const double k = wx * wy;
                        R += r * k; G += g * k; B += b * k; A += a * k;
                        wsum += k;
                    }
                }
                if (wsum > 0) { R /= wsum; G /= wsum; B /= wsum; A /= wsum; }
            } else {
                const double fx = (x + 0.5) * sx - 0.5, fy = (y + 0.5) * sy - 0.5;
                const int x0 = int(std::floor(fx)), y0 = int(std::floor(fy));
                const double tx = fx - x0, ty = fy - y0;
                for (int j = 0; j < 2; ++j)
                    for (int i = 0; i < 2; ++i) {
                        double r, g, b, a;
                        px(x0 + i, y0 + j, r, g, b, a);
                        const double k = (i ? tx : 1 - tx) * (j ? ty : 1 - ty);
                        R += r * k; G += g * k; B += b * k; A += a * k;
                    }
            }
            Rgba& o = out[size_t(oy + y) * kSize + ox + x];
            if (A > 1e-6) {
                o.r = float(std::clamp(R / A, 0.0, 255.0));
                o.g = float(std::clamp(G / A, 0.0, 255.0));
                o.b = float(std::clamp(B / A, 0.0, 255.0));
                o.a = float(std::clamp(A, 0.0, 1.0) * 255.0);
            }
        }
    }
    return out;
}

// Gives fully transparent pixels the colour of the nearest picture pixel, so bilinear filtering and the mips never blend
// in black or white from the transparent area (the visible colour fringe on a logo's edge otherwise).
inline void bleed_colour(std::vector<Rgba>& img, int size) {
    std::vector<Rgba> tmp;
    for (int pass = 0; pass < 8; ++pass) {
        bool any = false;
        tmp = img;
        for (int y = 0; y < size; ++y)
            for (int x = 0; x < size; ++x) {
                Rgba& o = tmp[size_t(y) * size + x];
                if (img[size_t(y) * size + x].a > 0) continue;
                float r = 0, g = 0, b = 0;
                int n = 0;
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx) {
                        const int xx = x + dx, yy = y + dy;
                        if (xx < 0 || yy < 0 || xx >= size || yy >= size) continue;
                        const Rgba& q = img[size_t(yy) * size + xx];
                        if (q.a > 0 || q.r + q.g + q.b > 0) { r += q.r; g += q.g; b += q.b; ++n; }
                    }
                if (n) { o.r = r / n; o.g = g / n; o.b = b / n; any = true; }
            }
        img.swap(tmp);
        if (!any) break;
    }
}

// Next mip: 2x2 average, alpha-weighted so colour doesn't bleed from transparent texels.
inline std::vector<Rgba> down(const std::vector<Rgba>& in, int size) {
    const int n = std::max(1, size / 2);
    std::vector<Rgba> out(size_t(n) * n);
    for (int y = 0; y < n; ++y)
        for (int x = 0; x < n; ++x) {
            double R = 0, G = 0, B = 0, A = 0, C0 = 0, C1 = 0, C2 = 0;
            for (int j = 0; j < 2; ++j)
                for (int i = 0; i < 2; ++i) {
                    const Rgba& p = in[size_t(std::min(size - 1, 2 * y + j)) * size + std::min(size - 1, 2 * x + i)];
                    R += p.r * p.a; G += p.g * p.a; B += p.b * p.a; A += p.a;
                    C0 += p.r; C1 += p.g; C2 += p.b;
                }
            Rgba& o = out[size_t(y) * n + x];
            if (A > 0) { o.r = float(R / A); o.g = float(G / A); o.b = float(B / A); }
            else { o.r = float(C0 / 4); o.g = float(C1 / 4); o.b = float(C2 / 4); }
            o.a = float(A / 4);
        }
    return out;
}

// ---- BC7, mode 6 (one subset, RGBA endpoints 7 bit + p-bit, 4-bit indices) --------------------------------------------

inline void put_bits(uint8_t* blk, int& pos, uint32_t v, int nb) {
    for (int i = 0; i < nb; ++i, ++pos)
        if ((v >> i) & 1) blk[pos >> 3] |= uint8_t(1u << (pos & 7));
}

inline void encode_block(const Rgba px[16], uint8_t out[16]) {
    static const int W4[16] = {0, 4, 9, 13, 17, 21, 26, 30, 34, 38, 43, 47, 51, 55, 60, 64};
    float mean[4] = {0, 0, 0, 0};
    for (int i = 0; i < 16; ++i) { mean[0] += px[i].r; mean[1] += px[i].g; mean[2] += px[i].b; mean[3] += px[i].a; }
    for (float& m : mean) m /= 16;
    auto get = [&](int i, int c) { return c == 0 ? px[i].r : c == 1 ? px[i].g : c == 2 ? px[i].b : px[i].a; };
    float cov[4][4] = {};
    for (int i = 0; i < 16; ++i)
        for (int a = 0; a < 4; ++a)
            for (int b = 0; b < 4; ++b) cov[a][b] += (get(i, a) - mean[a]) * (get(i, b) - mean[b]);
    float v[4] = {0.5f, 0.5f, 0.5f, 0.5f};
    for (int it = 0; it < 8; ++it) {
        float nv[4] = {};
        for (int a = 0; a < 4; ++a)
            for (int b = 0; b < 4; ++b) nv[a] += cov[a][b] * v[b];
        const float len = std::sqrt(nv[0] * nv[0] + nv[1] * nv[1] + nv[2] * nv[2] + nv[3] * nv[3]);
        if (len < 1e-6f) { for (float& x : v) x = 0.5f; break; }
        for (int a = 0; a < 4; ++a) v[a] = nv[a] / len;
    }
    float tmin = 1e30f, tmax = -1e30f;
    for (int i = 0; i < 16; ++i) {
        float t = 0;
        for (int c = 0; c < 4; ++c) t += (get(i, c) - mean[c]) * v[c];
        tmin = std::min(tmin, t);
        tmax = std::max(tmax, t);
    }
    float lo[4], hi[4];
    for (int c = 0; c < 4; ++c) {
        lo[c] = std::clamp(mean[c] + v[c] * tmin, 0.f, 255.f);
        hi[c] = std::clamp(mean[c] + v[c] * tmax, 0.f, 255.f);
    }

    int q0[4], q1[4], p0 = 0, p1 = 0, idx[16];
    float rec0[4], rec1[4];
    auto quant = [](const float* e, int* q, int& pbit, float* rec) {
        float best = 1e30f;
        for (int p = 0; p < 2; ++p) {
            int qq[4];
            float err = 0, rr[4];
            for (int c = 0; c < 4; ++c) {
                qq[c] = std::clamp(int(std::lround((e[c] - p) / 2.0f)), 0, 127);
                rr[c] = float(qq[c] * 2 + p);
                err += (rr[c] - e[c]) * (rr[c] - e[c]);
            }
            if (err < best) { best = err; pbit = p; for (int c = 0; c < 4; ++c) { q[c] = qq[c]; rec[c] = rr[c]; } }
        }
    };
    auto assign = [&](float& total) {
        total = 0;
        for (int i = 0; i < 16; ++i) {
            float best = 1e30f;
            int bi = 0;
            for (int k = 0; k < 16; ++k) {
                const float w = W4[k] / 64.0f;
                float d = 0;
                for (int c = 0; c < 4; ++c) {
                    const float e = rec0[c] * (1 - w) + rec1[c] * w - get(i, c);
                    d += e * e;
                }
                if (d < best) { best = d; bi = k; }
            }
            idx[i] = bi;
            total += best;
        }
    };
    quant(lo, q0, p0, rec0);
    quant(hi, q1, p1, rec1);
    float err;
    assign(err);
    // least-squares refinement of the endpoints for the chosen indices
    for (int round = 0; round < 6; ++round) {
        double a00 = 0, a01 = 0, a11 = 0, b0[4] = {}, b1[4] = {};
        for (int i = 0; i < 16; ++i) {
            const double w = W4[idx[i]] / 64.0;
            a00 += (1 - w) * (1 - w);
            a01 += (1 - w) * w;
            a11 += w * w;
            for (int c = 0; c < 4; ++c) { b0[c] += (1 - w) * get(i, c); b1[c] += w * get(i, c); }
        }
        const double det = a00 * a11 - a01 * a01;
        if (std::fabs(det) < 1e-6) break;
        float e0[4], e1[4];
        for (int c = 0; c < 4; ++c) {
            e0[c] = float(std::clamp((a11 * b0[c] - a01 * b1[c]) / det, 0.0, 255.0));
            e1[c] = float(std::clamp((a00 * b1[c] - a01 * b0[c]) / det, 0.0, 255.0));
        }
        int nq0[4], nq1[4], np0, np1, saved[16];
        float nr0[4], nr1[4], sr0[4], sr1[4];
        quant(e0, nq0, np0, nr0);
        quant(e1, nq1, np1, nr1);
        std::memcpy(saved, idx, sizeof idx);
        std::memcpy(sr0, rec0, sizeof rec0);
        std::memcpy(sr1, rec1, sizeof rec1);
        std::memcpy(rec0, nr0, sizeof rec0);
        std::memcpy(rec1, nr1, sizeof rec1);
        float nerr;
        assign(nerr);
        if (nerr < err) {
            err = nerr;
            std::memcpy(q0, nq0, sizeof q0); std::memcpy(q1, nq1, sizeof q1);
            p0 = np0; p1 = np1;
            std::memcpy(lo, e0, sizeof lo); std::memcpy(hi, e1, sizeof hi);
        } else {
            std::memcpy(idx, saved, sizeof idx);
            std::memcpy(rec0, sr0, sizeof rec0);
            std::memcpy(rec1, sr1, sizeof rec1);
            break;
        }
    }
    // the first pixel's index may only use the lower half: swap the endpoints otherwise
    if (idx[0] >= 8) {
        for (int c = 0; c < 4; ++c) std::swap(q0[c], q1[c]);
        std::swap(p0, p1);
        for (int& i : idx) i = 15 - i;
    }
    std::memset(out, 0, 16);
    int pos = 0;
    put_bits(out, pos, 0x40, 7);  // mode 6
    for (int c = 0; c < 4; ++c) { put_bits(out, pos, uint32_t(q0[c]), 7); put_bits(out, pos, uint32_t(q1[c]), 7); }
    put_bits(out, pos, uint32_t(p0), 1);
    put_bits(out, pos, uint32_t(p1), 1);
    put_bits(out, pos, uint32_t(idx[0]), 3);
    for (int i = 1; i < 16; ++i) put_bits(out, pos, uint32_t(idx[i]), 4);
}

// One mip level -> blocks in the console's 1D-tiled order (8x8-block micro tiles, Morton order inside, the surface padded
// to whole tiles; padding blocks stay zero).
inline void encode_level(const std::vector<Rgba>& img, int size, std::vector<uint8_t>& out) {
    const int bw = std::max(1, size / 4), bwp = (bw + 7) / 8 * 8;
    const size_t base = out.size();
    out.resize(base + size_t(bwp) * bwp * 16, 0);
    for (int by = 0; by < bw; ++by)
        for (int bx = 0; bx < bw; ++bx) {
            Rgba blk[16];
            for (int j = 0; j < 4; ++j)
                for (int i = 0; i < 4; ++i) {
                    const int x = std::min(size - 1, bx * 4 + i), y = std::min(size - 1, by * 4 + j);
                    blk[j * 4 + i] = img[size_t(y) * size + x];
                }
            const int lx = bx & 7, ly = by & 7;
            const int m = (lx & 1) | ((ly & 1) << 1) | (((lx >> 1) & 1) << 2) | (((ly >> 1) & 1) << 3) | (((lx >> 2) & 1) << 4) | (((ly >> 2) & 1) << 5);
            const size_t tile = size_t(by >> 3) * (bwp >> 3) + (bx >> 3);
            encode_block(blk, out.data() + base + (tile * 64 + m) * 16);
        }
}

// `rgba`: w*h pixels, straight alpha, any size. Returns the whole .swatchbin (87 + 92160 bytes).
inline std::vector<uint8_t> make_swatchbin(const uint8_t* rgba, int w, int h, bool premultiplied_output) {
    std::vector<Rgba> level = fit_to_square(rgba, w, h);
    bleed_colour(level, kSize);
    std::vector<uint8_t> out(k_swatch_hdr, k_swatch_hdr + sizeof k_swatch_hdr);
    int size = kSize;
    for (int m = 0; m < kMips; ++m) {
        std::vector<Rgba> enc = level;
        if (premultiplied_output)
            for (Rgba& p : enc) { const float a = p.a / 255.0f; p.r *= a; p.g *= a; p.b *= a; }
        encode_level(enc, size, out);
        if (m + 1 < kMips) { level = down(level, size); size = std::max(1, size / 2); }
    }
    return out;
}

// ---- .modelbin ---------------------------------------------------------------------------------------------------

inline uint32_t crc32(const std::string& s) {
    uint32_t c = 0xFFFFFFFFu;
    for (unsigned char ch : s) {
        c ^= ch;
        for (int k = 0; k < 8; ++k) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
    }
    return ~c;
}

// `name` is exactly 8 lowercase characters (the template's "decal002"); the texture it points to is
// game:\media\livery\decals\textures\<name>.swatchbin.
inline std::vector<uint8_t> make_modelbin(const std::string& name) {
    std::vector<uint8_t> m(k_modelbin, k_modelbin + sizeof k_modelbin);
    std::memcpy(m.data() + 723, name.data(), 8);
    const uint32_t h = crc32("game:\\media\\livery\\decals\\textures\\" + name + ".swatchbin");
    std::memcpy(m.data() + 377, &h, 4);
    std::memcpy(m.data() + 741, &h, 4);
    return m;
}

}  // namespace swatch
