// compressor: dm.compressor_demo (co.compressor_stereo with makeup gain). Detector |x| + |y| through an
// attack/release one-pole, in dB, gain computer, knee smoother (si.smooth, pole tau2pole(att/2)), linear.
struct Compressor
{
    static constexpr int numInputs = 2, numOutputs = 2;

    float ratio = 5.0f, threshold = -30.0f, attack = 50.0f, release = 500.0f, makeupgain = 12.0f;
    float sampleRate, mgain, slope, poleAtt, poleRel, poleKnee;   // derived
    float env, gdbPrev, err;                                      // state

    void setRatio (float v) { ratio = v; update(); }
    void setThreshold (float v) { threshold = v; }
    void setAttack (float v) { attack = v; update(); }
    void setRelease (float v) { release = v; update(); }
    void setMakeupgain (float v) { makeupgain = v; update(); }

    void update()
    {
        const float t = 1.0f / sampleRate;
        mgain = std::pow (10.0f, 0.05f * makeupgain);                  // ba.db2linear
        slope = 1.0f / std::max (0.0000001192092896f, ratio) + -1.0f;  // 1/max(ma.EPSILON, ratio) - 1
        const float att = std::max (t, 0.001f * attack);
        const float rel = std::max (t, 0.001f * release);
        poleAtt = std::exp (-(t / att));                               // ba.tau2pole(att)
        poleRel = std::exp (-(t / rel));
        poleKnee = std::exp (-(t / (0.5f * att)));                     // ba.tau2pole(att/2)
    }

    void prepare (float sr) { sampleRate = sr; update(); }

    void process (float* const* in, float* const* out, int n)
    {
        const float* xa = in[0];
        const float* xb = in[1];
        float* ya = out[0];
        float* yb = out[1];
        float e = env, gp = gdbPrev, er = err;
        const float pa = poleAtt, pr = poleRel, pk = poleKnee, sl = slope, th = threshold, mg = mgain;
        for (int i = 0; i < n; ++i)
        {
            const float x1 = xa[i];
            const float x2 = xb[i];
            const float x = std::fabs (std::fabs (x1) + std::fabs (x2));
            const float c = x > e ? pa : pr;
            e = x * (1.0f - c) + e * c;
            // ba.linear2db: 20*log10(max(ma.MIN, env)), log10 written log * 1/ln(10)
            const float gdb = sl * std::max (20.0f * (std::log (std::max (1.175494351e-38f, e)) * 0.4342944819f) - th, 0.0f);
            er = pk * (er + std::min (3.4028235e38f, gp - gdb));
            gp = gdb;
            const float g = std::pow (10.0f, 0.05f * (gdb + er));
            ya[i] = mg * x1 * g;
            yb[i] = mg * x2 * g;
        }
        env = e;
        gdbPrev = gp;
        err = er;
    }
};
using Dsp = Compressor;
