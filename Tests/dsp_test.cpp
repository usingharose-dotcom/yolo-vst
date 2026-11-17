// Offline sanity tests for the engine (no JUCE needed): g++ -O2 -std=c++17 -I../Source dsp_test.cpp
#include "YoloTune.h"
#include <cstdio>
#include <numeric>

static const double PI2 = 6.283185307179586;

static std::vector<float> tone (double sr, double f0, double vib, double secs)
{
    std::vector<float> d ((size_t) (sr * secs));
    double ph = 0;
    for (size_t i = 0; i < d.size(); ++i)
    {
        const double t = (double) i / sr, f = f0 * (1 + vib * std::sin (PI2 * 5.5 * t));
        ph += PI2 * f / sr;
        double v = 0; for (int h = 1; h <= 10; ++h) v += std::sin (h * ph) / h;
        d[i] = (float) (v * 0.25);
    }
    return d;
}

// zero-crossing based pitch estimate over a window
static double pitchOf (const std::vector<float>& y, double sr, size_t a, size_t len)
{
    const size_t lo = (size_t) (sr / 400), hi = (size_t) (sr / 100);
    std::vector<float> z (y.begin() + (long) a, y.begin() + (long) (a + len));
    double mean = std::accumulate (z.begin(), z.end(), 0.0) / (double) z.size();
    for (auto& v : z) v -= (float) mean;
    double best = -1e30; size_t bk = lo;
    for (size_t k = lo; k < hi; ++k)
    {
        double s = 0; for (size_t i = 0; i + k < z.size(); ++i) s += (double) z[i] * z[i + k];
        if (s > best) { best = s; bk = k; }
    }
    return sr / (double) bk;
}

static double cents (double f, double ref) { return 1200.0 * std::log2 (f / ref); }

static std::vector<float> run (double sr, const std::vector<float>& in, yolo::Params p, yolo::Engine& e)
{
    e.prepare (sr); e.setParams (p);
    std::vector<float> out (in.size()), outR (in.size());
    for (size_t i = 0; i < in.size(); i += 512)
    {
        const int n = (int) std::min<size_t> (512, in.size() - i);
        e.process (in.data() + i, nullptr, out.data() + i, outR.data() + i, n);
    }
    return out;
}

int main()
{
    int fails = 0;
    auto check = [&] (const char* what, bool ok, double v) { std::printf ("%-52s %s (%.2f)\n", what, ok ? "PASS" : "FAIL", v); if (! ok) ++fails; };

    for (double sr : {44100.0, 48000.0, 96000.0})
    {
        yolo::Engine e; yolo::Params p;
        for (int i = 0; i < 12; ++i) p.allowed[i] = (i == 11);          // only B allowed
        p.speed = 20; p.amount = 1;
        auto in = tone (sr, 237.0, 0.0, 3.0);                           // 29 cents above A#3
        auto out = run (sr, in, p, e);
        const double f = pitchOf (out, sr, (size_t) (sr * 2), 9600);
        char name[96]; std::snprintf (name, sizeof name, "sr %.0f: 237 Hz tuned to B3 (246.94), error in cents", sr);
        check (name, std::fabs (cents (f, 246.94)) < 12.0, cents (f, 246.94));
        float pk = 0; for (float v : out) pk = std::max (pk, std::fabs (v));
        check ("   output finite and under 1.0", std::isfinite (pk) && pk <= 1.0f, pk);
    }

    { // natural vs hard on vibrato: natural keeps more vibrato
        const double sr = 48000;
        yolo::Params p; for (int i = 0; i < 12; ++i) p.allowed[i] = (i == 11);
        auto in = tone (sr, 240.0, 0.012, 3.0);
        auto spread = [&] (yolo::Params q)
        {
            yolo::Engine e; auto out = run (sr, in, q, e);
            std::vector<double> c;
            for (size_t a = (size_t) sr; a + 4800 < out.size(); a += 960) c.push_back (cents (pitchOf (out, sr, a, 4800), 246.94));
            double m = std::accumulate (c.begin(), c.end(), 0.0) / (double) c.size(), s = 0;
            for (double v : c) s += (v - m) * (v - m);
            return std::sqrt (s / (double) c.size());
        };
        yolo::Params nat = p; nat.speed = 34.5f; nat.sustain = 0.4f; nat.amount = 1;
        yolo::Params hard = p; hard.speed = -3; hard.sustain = 0; hard.amount = 1;
        const double sn = spread (nat), sh = spread (hard);
        check ("hard setting flattens vibrato more than natural", sh < sn, sh);
    }

    { // bypass keeps pitch and delays by latency
        const double sr = 48000; yolo::Engine e; yolo::Params p; p.bypass = true;
        auto in = tone (sr, 237.0, 0.0, 2.0);
        auto out = run (sr, in, p, e);
        check ("bypass leaves pitch alone", std::fabs (cents (pitchOf (out, sr, 48000, 9600), 237.0)) < 8.0, cents (pitchOf (out, sr, 48000, 9600), 237.0));
        check ("reported latency is 288 samples at 48k (low latency)", e.latencySamples() == 288, e.latencySamples());
    }

    { // low notes only in instrument mode
        const double sr = 48000; yolo::Engine e; yolo::Params p; p.amount = 1; p.speed = 0;
        auto in = tone (sr, 65.4, 0.0, 2.0);
        auto out = run (sr, in, p, e);
        check ("voice mode ignores 65 Hz", ! e.voiced.load(), 0);
        p.inst = true; run (sr, in, p, e);
        check ("instrument mode tracks 65 Hz as C2 (midi 36)", e.voiced.load() && std::fabs (e.detM.load() - 36.f) < 0.5f, e.detM.load());
    }

    { // silence stays silent, doubler widens
        const double sr = 48000; yolo::Engine e; yolo::Params p; p.dbl = 1;
        std::vector<float> z ((size_t) sr, 0.f);
        auto out = run (sr, z, p, e);
        float pk = 0; for (float v : out) pk = std::max (pk, std::fabs (v));
        check ("silence in -> silence out", pk == 0.f, pk);
    }
    std::printf (fails ? "\n%d FAILED\n" : "\nall passed\n", fails);
    return fails != 0;
}
