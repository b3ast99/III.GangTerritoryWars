#include "TerritoryRadarRenderer.h"
#include "TerritorySystem.h"
#include "IniConfig.h"
#include "DebugLog.h"

#include "CRadar.h"
#include "CTimer.h"
#include "CWorld.h"
#include "CPlayerPed.h"
#include "CVector2D.h"
#include "CRGBA.h"

#include <rwcore.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace {

    static constexpr float kPi = 3.14159265358979323846f;

    // [AttackFlash] INI settings
    struct FlashConfig {
        unsigned int cycleMs = 1000;
        unsigned char maxAlpha = 180;
        unsigned char colorR = 160;
        unsigned char colorG = 15;
        unsigned char colorB = 15;
        unsigned int lastLoadTime = 0;
        bool liveReload = true;
        bool initialized = false;
    };

    static FlashConfig gFlashConfig;

    // Load config from INI
    static void LoadFlashConfig() {
        auto& ini = IniConfig::Instance();
        ini.Load("III.GangTerritoryWars.ini");

        auto ClampInt = [](int v, int lo, int hi) -> int {
            return std::clamp(v, lo, hi);
            };

        // Clamp to sane ranges
        gFlashConfig.cycleMs = (unsigned int)ClampInt(ini.GetInt("AttackFlash", "CycleMs", 1300), 100, 10000);
        gFlashConfig.maxAlpha = (unsigned char)ClampInt(ini.GetInt("AttackFlash", "MaxAlpha", 125), 0, 255);
        gFlashConfig.colorR = (unsigned char)ClampInt(ini.GetInt("AttackFlash", "ColorR", 210), 0, 255);
        gFlashConfig.colorG = (unsigned char)ClampInt(ini.GetInt("AttackFlash", "ColorG", 25), 0, 255);
        gFlashConfig.colorB = (unsigned char)ClampInt(ini.GetInt("AttackFlash", "ColorB", 25), 0, 255);
        gFlashConfig.liveReload = ini.GetInt("AttackFlash", "LiveReload", 1) != 0;

        gFlashConfig.lastLoadTime = CTimer::m_snTimeInMilliseconds;
        gFlashConfig.initialized = true;
    }


    // Reload config every ~500ms to allow live tuning (can be disabled via INI setting to boost performance)
    static void RefreshConfigIfNeeded() {
        if (!gFlashConfig.initialized) {
            LoadFlashConfig();
            return;
        }

        if (!gFlashConfig.liveReload) return;

        unsigned int now = CTimer::m_snTimeInMilliseconds;
        if (now - gFlashConfig.lastLoadTime > 500) {
            LoadFlashConfig();
        }
    }

    // -------------------------------
    // Small math helpers
    // -------------------------------
    static inline CVector2D Sub(const CVector2D& a, const CVector2D& b) { return CVector2D(a.x - b.x, a.y - b.y); }
    static inline CVector2D Add(const CVector2D& a, const CVector2D& b) { return CVector2D(a.x + b.x, a.y + b.y); }

    // Territory Radar Color
    static CRGBA RGBAForOwner(int ownerGang, bool underAttack, int defenseLevel)
    {
        if (underAttack) {
            // SA-style: simple, even pulse
            const unsigned int cycleMs = gFlashConfig.cycleMs;
            const unsigned int halfMs = cycleMs / 2;

            unsigned int t = CTimer::m_snTimeInMilliseconds % cycleMs;

            float amp;
            if (t < halfMs) {
                amp = t / float(halfMs);
            }
            else {
                amp = 1.0f - (t - halfMs) / float(halfMs);
            }

            if (amp < 0.0f) amp = 0.0f;
            if (amp > 1.0f) amp = 1.0f;

            unsigned char alpha = (unsigned char)(amp * gFlashConfig.maxAlpha);

            if (alpha < 4)
                return CRGBA(0, 0, 0, 0);

            return CRGBA(gFlashConfig.colorR, gFlashConfig.colorG, gFlashConfig.colorB, alpha);
        }

        // Normal territories
        if (ownerGang == -1)
            return CRGBA(0, 0, 0, 0);

        // ---- SA-ish tuning knobs ----
        // Stronger than your 55, still translucent.
        const unsigned char baseAlpha = 80;

        // Saturation boost: 1.0 = unchanged. 1.20â€“1.35 is usually the SA "pop" zone.
        const float satMul = 1.25f;

        // Defense level should NOT noticeably change color in SA. Keep extremely subtle.
        float lightnessFactor = 1.0f;
        switch (defenseLevel) {
        case 0: lightnessFactor = 1.10f; break; // barely brighter
        case 2: lightnessFactor = 0.90f; break; // barely darker
        default: lightnessFactor = 1.0f; break;
        }

        auto ClampU8 = [](float v) -> unsigned char {
            return (unsigned char)std::clamp((int)std::lround(v), 0, 255);
            };

        // Cheap saturation boost (no HSV):
        // Move color away from its luminance (gray) toward its original hue.
        auto ApplySaturation = [&](unsigned char r, unsigned char g, unsigned char b) -> CRGBA {
            // Luma (Rec.601-ish) gives stable results for "make it pop"
            const float fr = r / 255.0f;
            const float fg = g / 255.0f;
            const float fb = b / 255.0f;
            const float lum = fr * 0.299f + fg * 0.587f + fb * 0.114f;

            float rr = lum + (fr - lum) * satMul;
            float gg = lum + (fg - lum) * satMul;
            float bb = lum + (fb - lum) * satMul;

            rr = std::clamp(rr * lightnessFactor, 0.0f, 1.0f);
            gg = std::clamp(gg * lightnessFactor, 0.0f, 1.0f);
            bb = std::clamp(bb * lightnessFactor, 0.0f, 1.0f);

            return CRGBA(
                ClampU8(rr * 255.0f),
                ClampU8(gg * 255.0f),
                ClampU8(bb * 255.0f),
                baseAlpha
            );
            };

        // Base colors (slightly more vivid than your muted set)
        switch (ownerGang) {
        case PEDTYPE_GANG1: // Green
            return ApplySaturation(60, 220, 60);
        case PEDTYPE_GANG2: // Blue
            return ApplySaturation(60, 60, 235);
        case PEDTYPE_GANG3: // Red
            return ApplySaturation(245, 60, 60);
        default: // Yellow
            return ApplySaturation(255, 230, 70);
        }
    }


    // -------------------------------
    // RenderWare Im2D helpers
    // -------------------------------
    static inline void SetIm2DVertex(RwIm2DVertex& v, float x, float y, const CRGBA& c)
    {
        RwIm2DVertexSetScreenX(&v, x);
        RwIm2DVertexSetScreenY(&v, y);

        // Draw after map, before icons/blips.
        RwIm2DVertexSetScreenZ(&v, 0.95f);
        RwIm2DVertexSetRecipCameraZ(&v, 1.0f);

        RwIm2DVertexSetU(&v, 0.0f, 1.0f);
        RwIm2DVertexSetV(&v, 0.0f, 1.0f);

        RwIm2DVertexSetIntRGBA(&v, c.r, c.g, c.b, c.a);
    }

    static void SetRenderStateForOverlay()
    {
        RwRenderStateSet(rwRENDERSTATETEXTURERASTER, nullptr);
        RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, (void*)TRUE);
        RwRenderStateSet(rwRENDERSTATESRCBLEND, (void*)rwBLENDSRCALPHA);
        RwRenderStateSet(rwRENDERSTATEDESTBLEND, (void*)rwBLENDINVSRCALPHA);

        // Don't interfere with icons drawn later.
        RwRenderStateSet(rwRENDERSTATEZTESTENABLE, (void*)FALSE);
        RwRenderStateSet(rwRENDERSTATEZWRITEENABLE, (void*)FALSE);
    }

    struct RenderStateBackup {
        void* textureRaster = nullptr;
        void* vertexAlpha = nullptr;
        void* srcBlend = nullptr;
        void* dstBlend = nullptr;
        void* zTest = nullptr;
        void* zWrite = nullptr;
    };

    static inline RenderStateBackup CaptureRenderState()
    {
        RenderStateBackup s{};
        RwRenderStateGet(rwRENDERSTATETEXTURERASTER, &s.textureRaster);
        RwRenderStateGet(rwRENDERSTATEVERTEXALPHAENABLE, &s.vertexAlpha);
        RwRenderStateGet(rwRENDERSTATESRCBLEND, &s.srcBlend);
        RwRenderStateGet(rwRENDERSTATEDESTBLEND, &s.dstBlend);
        RwRenderStateGet(rwRENDERSTATEZTESTENABLE, &s.zTest);
        RwRenderStateGet(rwRENDERSTATEZWRITEENABLE, &s.zWrite);
        return s;
    }

    static inline void RestoreRenderState(const RenderStateBackup& s)
    {
        RwRenderStateSet(rwRENDERSTATETEXTURERASTER, s.textureRaster);
        RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, s.vertexAlpha);
        RwRenderStateSet(rwRENDERSTATESRCBLEND, s.srcBlend);
        RwRenderStateSet(rwRENDERSTATEDESTBLEND, s.dstBlend);
        RwRenderStateSet(rwRENDERSTATEZTESTENABLE, s.zTest);
        RwRenderStateSet(rwRENDERSTATEZWRITEENABLE, s.zWrite);
    }


    static void DrawPolyFilledFan(const std::vector<CVector2D>& poly, const CRGBA& fill)
    {
        if (poly.size() < 3) return;

        // Centroid fan triangulation (unchanged).
        CVector2D c(0.0f, 0.0f);
        for (const auto& p : poly) { c.x += p.x; c.y += p.y; }
        c.x /= (float)poly.size();
        c.y /= (float)poly.size();

        // Reuse buffer to avoid per-call heap churn.
        static std::vector<RwIm2DVertex> verts;
        verts.resize(poly.size() * 3);

        size_t o = 0;
        for (size_t i = 0; i < poly.size(); ++i) {
            const CVector2D& a = poly[i];
            const CVector2D& b = poly[(i + 1) % poly.size()];
            SetIm2DVertex(verts[o++], c.x, c.y, fill);
            SetIm2DVertex(verts[o++], a.x, a.y, fill);
            SetIm2DVertex(verts[o++], b.x, b.y, fill);
        }

        SetRenderStateForOverlay();
        RwIm2DRenderPrimitive(rwPRIMTYPETRILIST, verts.data(), (RwInt32)verts.size());
    }


    static void DrawPolyOutline(const std::vector<CVector2D>& poly, const CRGBA& border)
    {
        if (poly.size() < 2) return;

        static std::vector<RwIm2DVertex> verts;
        verts.resize(poly.size() * 2);

        size_t o = 0;
        for (size_t i = 0; i < poly.size(); ++i) {
            const CVector2D& a = poly[i];
            const CVector2D& b = poly[(i + 1) % poly.size()];
            SetIm2DVertex(verts[o++], a.x, a.y, border);
            SetIm2DVertex(verts[o++], b.x, b.y, border);
        }

        SetRenderStateForOverlay();
        RwIm2DRenderPrimitive(rwPRIMTYPELINELIST, verts.data(), (RwInt32)verts.size());
    }

    // -------------------------------
    // Radar transforms
    // -------------------------------
    static bool WorldToRadarScreen(float wx, float wy, CVector2D& outScreen)
    {
        CVector2D radar;
        CRadar::TransformRealWorldPointToRadarSpace(radar, CVector2D(wx, wy));
        CRadar::TransformRadarPointToScreenSpace(outScreen, radar);
        return true;
    }

    // Cached ellipse radii for the current draw (screen-space pixels).
    static float gRadarRxPx = 0.0f;
    static float gRadarRyPx = 0.0f;

    // Computes radar center + ellipse radii in screen space.
    // NOTE: outRadiusPx is kept only for compatibility; we use gRadarRxPx/gRadarRyPx.
    static void GetRadarCircleScreen(CVector2D& outCenter, float& outRadiusPx)
    {
        CRadar::TransformRadarPointToScreenSpace(outCenter, CVector2D(0.0f, 0.0f));

        CVector2D edgeX(100000.0f, 0.0f);
        CVector2D edgeY(0.0f, 100000.0f);

        CRadar::LimitRadarPoint(edgeX);
        CRadar::LimitRadarPoint(edgeY);

        CVector2D edgeXScreen, edgeYScreen;
        CRadar::TransformRadarPointToScreenSpace(edgeXScreen, edgeX);
        CRadar::TransformRadarPointToScreenSpace(edgeYScreen, edgeY);

        const float rx = std::fabs(edgeXScreen.x - outCenter.x);
        const float ry = std::fabs(edgeYScreen.y - outCenter.y);

        // Base inset to account for rounding + radar rim thickness.
        const float kBaseInsetPx = 3.25f;

        gRadarRxPx = std::max(0.0f, rx - kBaseInsetPx);
        gRadarRyPx = std::max(0.0f, ry - kBaseInsetPx);

        outRadiusPx = std::min(gRadarRxPx, gRadarRyPx);
    }

    // -------------------------------
// Per-frame radar cache (no visual change)
// -------------------------------
    struct RadarFrameCache {
        CVector2D center{};
        float rx = 0.0f;
        float ry = 0.0f;

        // Using EXACT same inset + seg count as current code.
        float fillRx = 0.0f;
        float fillRy = 0.0f;
        int segs = 96;

        std::vector<CVector2D> fillEllipseLocal; // CCW, centered at origin
    };

    static RadarFrameCache gRadarCache;
    #ifdef _DEBUG
    static unsigned int s_cacheUpdates = 0;
    static unsigned int s_ellipseRebuilds = 0;
    static unsigned int s_drawTerritoryCalls = 0;
    static unsigned int s_lastPerfLogMs = 0;
    #endif

    static std::vector<CVector2D> MakeEllipsePolyLocal(float rx, float ry, int segs)
    {
        segs = std::clamp(segs, 24, 160);

        std::vector<CVector2D> c;
        c.reserve(segs);

        // CCW ellipse polygon (center at origin).
        for (int i = 0; i < segs; ++i) {
            const float t = (2.0f * kPi) * ((float)i / (float)segs);
            c.push_back(CVector2D(rx * std::cos(t), ry * std::sin(t)));
        }

        return c;
    }

    static void UpdateRadarCache()
    {
        #ifdef _DEBUG
        ++s_cacheUpdates; // DEBUG
        #endif  
        float unusedRadius = 0.0f;
        GetRadarCircleScreen(gRadarCache.center, unusedRadius);

        gRadarCache.rx = gRadarRxPx;
        gRadarCache.ry = gRadarRyPx;

        const float kFillInsetPx = 5.0f;
        gRadarCache.fillRx = std::max(0.0f, gRadarCache.rx - kFillInsetPx);
        gRadarCache.fillRy = std::max(0.0f, gRadarCache.ry - kFillInsetPx);
        gRadarCache.segs = 96;

        // Keep the â€œchanged radiiâ€ test if you like
        static float s_lastFillRx = -1.0f;
        static float s_lastFillRy = -1.0f;
        static int   s_lastSegs = -1;

        const float eps = 0.01f;
        if (std::fabs(gRadarCache.fillRx - s_lastFillRx) > eps ||
            std::fabs(gRadarCache.fillRy - s_lastFillRy) > eps ||
            gRadarCache.segs != s_lastSegs)
        {
            s_lastFillRx = gRadarCache.fillRx;
            s_lastFillRy = gRadarCache.fillRy;
            s_lastSegs = gRadarCache.segs;

            gRadarCache.fillEllipseLocal =
                MakeEllipsePolyLocal(gRadarCache.fillRx, gRadarCache.fillRy, gRadarCache.segs);
            #ifdef _DEBUG
            ++s_ellipseRebuilds;
            #endif
        }
    }



    // -------------------------------
    // Convex polygon clipping (Sutherlandâ€“Hodgman)
    // Subject and clip polygons must be CCW.
    // -------------------------------
    static inline float Cross(const CVector2D& a, const CVector2D& b) { return a.x * b.y - a.y * b.x; }

    static bool InsideHalfPlaneCCW(const CVector2D& p, const CVector2D& a, const CVector2D& b)
    {
        // Inside is "left of" edge a->b for CCW clip polygon.
        return Cross(Sub(b, a), Sub(p, a)) >= 0.0f;
    }

    static CVector2D LineIntersection(
        const CVector2D& p1, const CVector2D& p2,
        const CVector2D& a, const CVector2D& b
    ) {
        // Intersect segment p1->p2 with infinite line a->b
        const CVector2D r = Sub(p2, p1);
        const CVector2D s = Sub(b, a);
        const float denom = Cross(r, s);
        if (std::fabs(denom) < 1e-6f) return p1; // nearly parallel; fallback

        const float t = Cross(Sub(a, p1), s) / denom;
        return Add(p1, CVector2D(r.x * t, r.y * t));
    }

    static std::vector<CVector2D> ClipConvexCCW(
        const std::vector<CVector2D>& subject,
        const std::vector<CVector2D>& clip
    ) {
        if (subject.size() < 3 || clip.size() < 3) return {};

        // Reused working buffers (capacity persists; contents do not leak)
        static std::vector<CVector2D> bufA;
        static std::vector<CVector2D> bufB;

        // Reserve to avoid growth reallocations.
        // For quad clipped by ~96-gon, typical upper bound stays well under a few hundred.
        if (bufA.capacity() < 256) bufA.reserve(256);
        if (bufB.capacity() < 256) bufB.reserve(256);

        // Copy subject into bufA (still a copy, but no reallocation once capacity is set)
        bufA.assign(subject.begin(), subject.end());
        bufB.clear();

        std::vector<CVector2D>* in = &bufA;
        std::vector<CVector2D>* out = &bufB;

        for (size_t i = 0; i < clip.size(); ++i) {
            const CVector2D A = clip[i];
            const CVector2D B = clip[(i + 1) % clip.size()];

            out->clear();
            if (in->empty()) break;

            CVector2D S = in->back();
            bool S_in = InsideHalfPlaneCCW(S, A, B);

            for (const auto& E : *in) {
                const bool E_in = InsideHalfPlaneCCW(E, A, B);

                if (S_in && E_in) {
                    out->push_back(E);
                }
                else if (S_in && !E_in) {
                    out->push_back(LineIntersection(S, E, A, B));
                }
                else if (!S_in && E_in) {
                    out->push_back(LineIntersection(S, E, A, B));
                    out->push_back(E);
                }

                S = E;
                S_in = E_in;
            }

            std::swap(in, out);
        }

        if (in->size() < 3) return {};

        // Dedup/clean into a normal local result so we return stable memory by value.
        std::vector<CVector2D> clean;
        clean.reserve(in->size());

        for (const auto& p : *in) {
            if (clean.empty()) { clean.push_back(p); continue; }
            const auto& q = clean.back();
            const float dx = p.x - q.x, dy = p.y - q.y;
            if (dx * dx + dy * dy > 0.25f) clean.push_back(p);
        }

        if (clean.size() >= 2) {
            const auto& f = clean.front();
            const auto& l = clean.back();
            const float dx = f.x - l.x, dy = f.y - l.y;
            if (dx * dx + dy * dy <= 0.25f) clean.pop_back();
        }

        if (clean.size() < 3) return {};
        return clean; // return-by-value (safe)
    }






    // -------------------------------
    // Per-territory render
    // -------------------------------
    static void DrawRadarTerritory(const Territory& t, const CRGBA& fill)
    {
        #ifdef _DEBUG
        ++s_drawTerritoryCalls; // DEBUG
        #endif
        const float x1 = t.minX;
        const float y1 = t.minY;
        const float x2 = t.maxX;
        const float y2 = t.maxY;


        CVector2D s0, s1, s2, s3;
        WorldToRadarScreen(x1, y1, s0);
        WorldToRadarScreen(x2, y1, s1);
        WorldToRadarScreen(x2, y2, s2);
        WorldToRadarScreen(x1, y2, s3);

        // Reuse buffers (no allocations per territory)
        static std::vector<CVector2D> quadLocal;
        static std::vector<CVector2D> clippedScreen;

        quadLocal.clear();
        quadLocal.reserve(4);

        const CVector2D& center = gRadarCache.center;

        // Quad in LOCAL space relative to radar center.
        quadLocal.push_back(Sub(s0, center));
        quadLocal.push_back(Sub(s1, center));
        quadLocal.push_back(Sub(s2, center));
        quadLocal.push_back(Sub(s3, center));

        // Ensure quad is CCW (ClipConvexCCW expects CCW). (unchanged)
        auto PolyArea2 = [](const std::vector<CVector2D>& p) {
            float a = 0.0f;
            for (size_t i = 0; i < p.size(); ++i) {
                const auto& A = p[i];
                const auto& B = p[(i + 1) % p.size()];
                a += (A.x * B.y - A.y * B.x);
            }
            return a;
            };
        if (PolyArea2(quadLocal) < 0.0f) std::reverse(quadLocal.begin(), quadLocal.end());

        // Check ellipse is not empty
        if (gRadarCache.fillEllipseLocal.size() < 3) return;

        // Fill clip (same ellipse poly, same segs, same clip code)
        const std::vector<CVector2D> clippedLocal = ClipConvexCCW(quadLocal, gRadarCache.fillEllipseLocal);
        if (clippedLocal.size() < 3) return;

        #ifdef _DEBUG
        if (clippedLocal.size() > 512) {
            DebugLog::Write("[RadarPerf] WARNING: clippedLocal too large (%zu)", clippedLocal.size());
            return;
        }
        #endif

        clippedScreen.clear();
        clippedScreen.reserve(clippedLocal.size());
        for (const auto& p : clippedLocal) clippedScreen.push_back(Add(p, center));

        DrawPolyFilledFan(clippedScreen, fill);
    }


} // namespace

void TerritoryRadarRenderer::DrawRadarOverlay(const std::vector<Territory>& territories)
{
    const auto rs = CaptureRenderState();

    RefreshConfigIfNeeded();

    static bool s_cacheInit = false;
    static unsigned int s_nextRadarCacheMs = 0;
    const unsigned int now = CTimer::m_snTimeInMilliseconds;

    if (!s_cacheInit || (int)(now - s_nextRadarCacheMs) >= 0) {
        s_cacheInit = true;
        s_nextRadarCacheMs = now + 80; // ~12.5 Hz
        UpdateRadarCache();
    }

    for (const auto& t : territories) {
        const CRGBA fill = RGBAForOwner(t.ownerGang, t.underAttack, t.defenseLevel);
        DrawRadarTerritory(t, fill);
    }

    RestoreRenderState(rs);

    #ifdef _DEBUG
    const unsigned int now2 = CTimer::m_snTimeInMilliseconds;
    if (now2 - s_lastPerfLogMs >= 2500) {
        s_lastPerfLogMs = now2;
        DebugLog::Write("[RadarPerf] territories=%zu drawCalls=%u cacheUpdates=%u ellipseRebuilds=%u",
            territories.size(), s_drawTerritoryCalls, s_cacheUpdates, s_ellipseRebuilds);
        s_drawTerritoryCalls = 0;
        s_cacheUpdates = 0;
        s_ellipseRebuilds = 0;
    }
    #endif
}
