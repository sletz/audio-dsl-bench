// exciter: dm.exciter (examples/psychoacoustic/harmonicExciter.dsp) without the compressor bypass and the gain
// meter. Wet path: fi.highpass(2, fc) (TPT SVF), co.compressor_mono (one-pole attack/release detector, in dB,
// gain computer, knee smoother, back to linear), pregain, asymmetric cubic harmonic creator, postgain; mixed
// with the dry input.
struct Exciter
{
    static constexpr int numInputs = 1, numOutputs = 1;

    float ratio = 5.0f, threshold = -30.0f, attack = 50.0f, release = 500.0f;   // parameters
    float cutoff = 5000.0f, harmonics = 20.0f, mix = 0.5f;
    float sampleRate, dryGain, wetGain, pre, g, gk, c1, c2, g2, slope;          // derived
    float poleRel, poleAtt, poleKnee;
    float s1, s2, env, gdbPrev, err;                                            // state

    void setRatio (float v) { ratio = v; update(); }
    void setThreshold (float v) { threshold = v; }
    void setAttack (float v) { attack = v; update(); }
    void setRelease (float v) { release = v; update(); }
    void setCutoff (float v) { cutoff = v; update(); }
    void setHarmonics (float v) { harmonics = v; update(); }
    void setMix (float v) { mix = v; update(); }

    void update()
    {
        const float t = 1.0f / sampleRate;
        dryGain = 1.0f - mix;
        wetGain = 100.0f * (mix / harmonics);                                // postgain 1/ph, times the mix
        pre = 0.01f * harmonics;                                             // pregain ph
        g = std::tan (std::max (0.0f, std::min (3.14159265358979323846f / sampleRate * cutoff, 1.5676547f)));
        gk = g + 1.4142135f;
        c1 = g * std::min (3.4028235e38f, 1.0f / (g * gk + 1.0f));
        c2 = 2.0f * c1;
        g2 = 2.0f * g;
        slope = 1.0f / std::max (0.0000001192092896f, ratio) + -1.0f;        // 1/max(ma.EPSILON, ratio) - 1
        poleRel = std::exp (-(t / std::max (t, 0.001f * release)));         // ba.tau2pole(rel)
        const float att = std::max (t, 0.001f * attack);                    // attack in s, at least one sample
        poleAtt = std::exp (-(t / att));                                     // ba.tau2pole(att)
        poleKnee = std::exp (-(t / (0.5f * att)));                           // ba.tau2pole(att/2)
    }

    void prepare (float sr) { sampleRate = sr; update(); }

    void process (float* const* in, float* const* out, int n)
    {
        const float* x0 = in[0];
        float* y0 = out[0];
        const float gv = g, gkv = gk, c1v = c1, c2v = c2, g2v = g2, sl = slope, th = threshold;
        const float pa = poleAtt, pr = poleRel, pk = poleKnee, pg = pre, dg = dryGain, wg = wetGain;
        float z1 = s1, z2 = s2, e = env, gp = gdbPrev, er = err;
        for (int i = 0; i < n; ++i)
        {
            const float x = x0[i];
            // fi.highpass(2, fc)
            const float v = x - (gkv * z1 + z2);
            const float s1n = z1 + c2v * v;
            const float bp = z1 + c1v * v;
            const float s2n = z2 + g2v * bp;
            const float hp = x - (z2 + gv * bp + 1.4142135f * bp);
            z1 = s1n;
            z2 = s2n;
            // co.compressor_mono
            const float a = std::fabs (hp);
            const float c = a > e ? pa : pr;
            e = a * (1.0f - c) + e * c;
            // ba.linear2db: 20*log10(max(ma.MIN, env)), log10 written log * 1/ln(10) like the Cmajor port
            const float gdb = sl * std::max (20.0f * (std::log (std::max (1.175494351e-38f, e)) * 0.4342944819f) - th,
                                             0.0f);
            er = pk * (er + std::min (3.4028235e38f, gp - gdb));
            gp = gdb;
            const float comp = hp * std::pow (10.0f, 0.05f * (gdb + er));
            // pregain and harmonic creator: x for x < 0, x - x^3/3 on [0, 1], 2/3 above 1
            const float p = pg * comp;
            // both branches computed, so that clang selects rather than branches on the noisy sign of p
            const float cubic = p - 0.33333334f * (p * p * p);
            const float pos = p <= 1.0f ? cubic : 0.33333334f * 2.0f;
            const float h = p < 0.0f ? p : pos;
            y0[i] = dg * x + wg * h;
        }
        s1 = z1;
        s2 = z2;
        env = e;
        gdbPrev = gp;
        err = er;
    }
};
using Dsp = Exciter;
