// noise_gate: dm.gate_demo (ef.gate_stereo of Faust's misceffects.lib) without the bypass and the gate-gain
// meter. The level |x| + |y| goes through a fast one-pole follower (pole of min(attack, release)); the raw
// gate is that level above the threshold; a counter holds the gate open for `hold` after the raw gate falls;
// the gate signal goes through the attack/release one-pole (an.amp_follower_ar) and multiplies both channels.
struct NoiseGate
{
    static constexpr int numInputs = 2, numOutputs = 2;

    float threshold = -30.0f, attack = 10.0f, hold = 200.0f, release = 100.0f;   // parameters (dB, us, ms, ms)
    float sampleRate, poleMin, oneMinusPoleMin, thr, poleRel, poleAtt;           // derived
    int holdSamps;
    float lev;     // inlevel follower state
    int rawPrev;   // raw gate, previous sample
    int cnt;       // hold counter
    float env;     // gate gain follower state

    void setThreshold (float v) { threshold = v; update(); }
    void setAttack (float v) { attack = v; update(); }
    void setHold (float v) { hold = v; update(); }
    void setRelease (float v) { release = v; update(); }

    void update()
    {
        const float t = 1.0f / sampleRate;
        const float att = std::max (t, 0.000001f * attack);   // attack in s, at least one sample
        const float rel = std::max (t, 0.001f * release);
        poleMin = std::exp (-(t / std::min (att, rel)));      // ba.tau2pole(min(att, rel))
        oneMinusPoleMin = 1.0f - poleMin;
        thr = std::pow (10.0f, 0.05f * threshold);            // ba.db2linear
        holdSamps = int (sampleRate * std::max (t, 0.001f * hold));
        poleRel = std::exp (-(t / rel));
        poleAtt = std::exp (-(t / att));
    }

    void prepare (float sr) { sampleRate = sr; update(); }

    void process (float* const* in, float* const* out, int n)
    {
        const float* in1 = in[0];
        const float* in2 = in[1];
        float* out1 = out[0];
        float* out2 = out[1];
        const float pm = poleMin, opm = oneMinusPoleMin, th = thr, pa = poleAtt, pr = poleRel;
        const int hs = holdSamps;
        float l = lev, e = env;
        int rp = rawPrev, ct = cnt;
        for (int i = 0; i < n; ++i)
        {
            const float x1 = in1[i];
            const float x2 = in2[i];
            l = opm * std::fabs (std::fabs (x1) + std::fabs (x2)) + pm * l;
            const int raw = l > th ? 1 : 0;
            const int reset = raw < rp ? 1 : 0;   // the raw gate falls: restart the hold counter
            rp = raw;
            ct = std::max (hs * reset, ct - 1);
            const float gsig = std::fabs (std::max (float (raw), ct > 0 ? 1.0f : 0.0f));
            const float c = gsig > e ? pa : pr;
            e = gsig * (1.0f - c) + e * c;
            out1[i] = x1 * e;
            out2[i] = x2 * e;
        }
        lev = l;
        env = e;
        rawPrev = rp;
        cnt = ct;
    }
};
using Dsp = NoiseGate;
