// YOLO pitch-correction engine. Header-only, no JUCE dependency.
// Monophonic YIN pitch tracking -> scale snapping -> two-tap delay-line pitch shifter,
// plus doubler and soft-knee output limiter. Direct port of the browser prototype.
#pragma once
#include <algorithm>
#include <atomic>
#include <cmath>
#include <vector>

namespace yolo
{
struct Params
{
    float flex = 0;       // 0..1  dead zone around the note centre
    float speed = 50;     // ms retune time, -3..200 (<=0 snaps instantly)
    float amount = 0.5f;  // 0..1  how much correction is applied
    float sustain = 0;    // 0..1  smooths the tracked pitch (keeps vibrato)
    int   stab = 0;       // 0 none, 1 short, 2 mid, 3 long
    bool  bypass = false, inst = false, lowlat = true;
    float refCents = 0;   // pitch reference offset (cents)
    float inG = 1, outG = 1, dbl = 0, hum = 0;
    bool  allowed[12] = {true, true, true, true, true, true, true, true, true, true, true, true};
};

class Engine
{
public:
    // Live readouts for the UI (written from the audio thread).
    std::atomic<float> detM {60.f}, outM {60.f}, peakIn {0.f}, peakOut {0.f};
    std::atomic<bool>  voiced {false};

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        D = std::max (1, (int) std::lround (sr / 48000.0));
        srD = sr / D;
        N = 4096;
        buf.assign ((size_t) N, 0.f);
        x.assign (2300, 0.f);
        cm.assign (1500, 0.f);
        Wlow  = std::max (64.f, (float) std::lround (576.0 * sr / 48000.0));
        Whigh = std::max (64.f, (float) std::lround (1024.0 * sr / 48000.0));
        e2 = (int) std::lround (0.017 * sr);
        e3 = (int) std::lround (0.024 * sr);
        int need = (int) Whigh + e3 + 64, sz = 1;
        while (sz < need) sz <<= 1;
        dl.assign ((size_t) sz, 0.f);
        mask = sz - 1;
        hopLow  = std::max (16, (int) std::lround (0.005 * sr));
        hopHigh = std::max (16, (int) std::lround (0.010 * sr));
        reset();
    }

    void reset()
    {
        std::fill (buf.begin(), buf.end(), 0.f);
        std::fill (dl.begin(), dl.end(), 0.f);
        w = 0; dw = 0; dcount = 0; acc = 0; cnt = 0;
        ph = ph2 = ph3 = 0;
        smValid = false; sm = 60; tgt = 60; cc = 0; o = 0; ratio = rt = 1; rb = 0;
        voicedNow = false; m = 60; pi = po = 0;
        gIn = p.inG; gOut = p.outG; gDbl = p.dbl;
    }

    void setParams (const Params& np) { p = np; }
    int  latencySamples() const { return (int) ((p.lowlat ? Wlow : Whigh) * 0.5f); }

    // inR/outR may be null (mono). In-place processing (in == out) is fine.
    void process (const float* inL, const float* inR, float* outL, float* outR, int n)
    {
        const float W = p.lowlat ? Wlow : Whigh;
        const int hop = p.lowlat ? hopLow : hopHigh;
        const bool byp = p.bypass;
        const int bypDelay = (int) (W * 0.5f);
        const float smooth = 1.f - std::exp (-1.f / (0.005f * (float) sr));
        float peakI = 0, peakO = 0;

        for (int i = 0; i < n; ++i)
        {
            float in = inR ? 0.5f * (inL[i] + inR[i]) : inL[i];
            gIn  += (p.inG  - gIn)  * smooth;
            gOut += (p.outG - gOut) * smooth;
            gDbl += (p.dbl  - gDbl) * smooth;
            const float s = in * gIn;
            peakI = std::max (peakI, std::fabs (s));

            acc += s;
            if (++dcount >= D) { buf[(size_t) w] = acc / (float) D; w = (w + 1) % N; acc = 0; dcount = 0; }
            dl[(size_t) dw] = s;
            dw = (dw + 1) & mask;
            if (++cnt >= hop) { cnt = 0; analyze (hop); }

            ratio += (rt - ratio) * (0.01f + rb * 0.06f);
            ph += (1.f - ratio) / W; wrap (ph);

            float y;
            if (byp) y = dl[(size_t) ((dw - 1 - bypDelay) & mask)];
            else     y = rd (ph, 0.f, W);
            float l = y, r = y;
            if (gDbl > 0.0005f && ! byp)
            {
                ph2 += (1.f - ratio * 0.9931f) / W;  wrap (ph2);
                ph3 += (1.f - ratio * 1.00696f) / W; wrap (ph3);
                l += gDbl * 0.6f * rd (ph2, (float) e2, W);
                r += gDbl * 0.6f * rd (ph3, (float) e3, W);
            }
            l = soft (l * gOut); r = soft (r * gOut);
            peakO = std::max (peakO, std::max (std::fabs (l), std::fabs (r)));
            outL[i] = l;
            if (outR) outR[i] = r;
        }
        pi = std::max (pi, peakI); po = std::max (po, peakO);
        peakIn.store (peakI);  peakOut.store (peakO);
    }

private:
    static void wrap (float& v) { if (v < 0) v += 1.f; if (v >= 1.f) v -= 1.f; }

    static float soft (float v)
    {
        const float a = std::fabs (v);
        return a < 0.7f ? v : (v < 0 ? -1.f : 1.f) * (0.7f + 0.3f * std::tanh ((a - 0.7f) / 0.3f));
    }

    float rd (float phase, float extra, float W) const
    {
        float y = 0;
        for (int k = 0; k < 2; ++k)
        {
            float t = phase + (float) k * 0.5f; if (t >= 1.f) t -= 1.f;
            const float d = t * W + 2.f + extra, pos = (float) (dw - 1) - d;
            const int i0 = (int) std::floor (pos);
            const float fr = pos - (float) i0;
            const float a0 = dl[(size_t) (i0 & mask)], a1 = dl[(size_t) ((i0 + 1) & mask)];
            y += (a0 + (a1 - a0) * fr) * (1.f - std::fabs (2.f * t - 1.f));
        }
        return y;
    }

    // YIN pitch detector on the (decimated) input history. Returns Hz or -1.
    float detect()
    {
        const int Wd = 768;
        const int tmax = std::min (p.inst ? 1400 : 700, (int) std::floor (srD / (p.inst ? 58.0 : 70.0)));
        const int tmin = std::max (2, (int) std::floor (srD / (p.inst ? 1500.0 : 900.0)));
        const int total = Wd + tmax;
        for (int i = 0; i < total; ++i) x[(size_t) i] = buf[(size_t) (((w - total + i) % N + N) % N)];
        float e = 0;
        for (int i = tmax; i < total; ++i) e += x[(size_t) i] * x[(size_t) i];
        if (e / (float) Wd < 2e-5f) return -1.f;
        float run = 0; cm[0] = 1.f;
        for (int t = 1; t < tmax; ++t)
        {
            float d = 0;
            for (int j = tmax; j < total; ++j) { const float v = x[(size_t) j] - x[(size_t) (j - t)]; d += v * v; }
            run += d;
            cm[(size_t) t] = run > 0 ? d * (float) t / run : 1.f;
        }
        int best = -1;
        for (int t = tmin; t < tmax - 1; ++t)
            if (cm[(size_t) t] < 0.15f)
            {
                while (t + 1 < tmax - 1 && cm[(size_t) (t + 1)] < cm[(size_t) t]) ++t;
                best = t; break;
            }
        if (best < 0)
        {
            float mn = 1.f; int mi = -1;
            for (int t = tmin; t < tmax - 1; ++t) if (cm[(size_t) t] < mn) { mn = cm[(size_t) t]; mi = t; }
            if (mn < 0.3f) best = mi;
        }
        if (best < 1) return -1.f;
        const float a = cm[(size_t) (best - 1)], b = cm[(size_t) best], c = cm[(size_t) (best + 1)];
        const float den = 2.f * (2.f * b - a - c);
        float tau = (float) best;
        if (std::fabs (den) > 1e-9f) tau += (c - a) / den;
        return (float) srD / tau;
    }

    void analyze (int hopSamples)
    {
        const float hop = (float) hopSamples / (float) sr, f = detect();
        const float amt = p.amount;
        if (f > 0)
        {
            const float mm = 69.f + 12.f * std::log2 (f / (440.f * std::pow (2.f, p.refCents / 1200.f)));
            voicedNow = true; m = mm;
            rb = std::max (0.f, std::min (1.f, (25.f - p.speed) / 28.f));
            sm = (! smValid || std::fabs (sm - mm) > 3.f) ? mm : sm + (mm - sm) * (1.f - 0.92f * p.sustain * (1.f - rb));
            smValid = true;
            int cand = (int) std::lround (sm); float bd = 99.f;
            const int base = (int) std::lround (sm);
            for (int i = -6; i <= 6; ++i)
            {
                const int r = base + i;
                if (p.allowed[((r % 12) + 12) % 12] && std::fabs ((float) r - sm) < bd) { bd = std::fabs ((float) r - sm); cand = r; }
            }
            if (cand == tgt) cc = 0;
            else
            {
                ++cc;
                static const int needTab[4] = {0, 2, 6, 14};
                if (cc > needTab[std::max (0, std::min (3, p.stab))]) { tgt = cand; cc = 0; }
            }
            float oT = (float) tgt - sm;
            const float tol = p.flex * 1.2f, ae = std::fabs (oT);
            if (tol > 0 && ae < tol) { const float k = ae / tol; oT *= k * k; }
            const float ms = p.speed;
            const float a = (ms <= 0 ? 1.f : 1.f - std::exp (-hop / std::max (0.0005f, ms / 1000.f / 2.5f))) * (1.f - 0.7f * p.hum);
            o += (oT - o) * a;
            rt = p.bypass ? 1.f : std::pow (2.f, o * amt / 12.f);
        }
        else
        {
            voicedNow = false; o *= 0.8f;
            rt = p.bypass ? 1.f : std::pow (2.f, o * amt / 12.f);
            cc = 0;
        }
        detM.store (m);
        outM.store (m + o * (p.bypass ? 0.f : amt));
        voiced.store (voicedNow);
    }

    Params p;
    double sr = 48000, srD = 48000;
    int D = 1, N = 4096, w = 0, dw = 0, dcount = 0, mask = 8191, cnt = 0, hopLow = 240, hopHigh = 480, e2 = 0, e3 = 0, tgt = 60, cc = 0;
    float acc = 0, Wlow = 576, Whigh = 1024, ph = 0, ph2 = 0, ph3 = 0, sm = 60, o = 0, ratio = 1, rt = 1, rb = 0, m = 60, pi = 0, po = 0;
    float gIn = 1, gOut = 1, gDbl = 0;
    bool smValid = false, voicedNow = false;
    std::vector<float> buf, dl, x, cm;
};
} // namespace yolo
