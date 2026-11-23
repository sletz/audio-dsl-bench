// svf_mod: TPT state variable lowpass whose cutoff is swept by a sine LFO: coefficients per sample.
struct SvfMod
{
    static constexpr int numInputs = 1, numOutputs = 1;

    float base = 800.0f, depth = 2.0f, rate = 0.5f, q = 2.0f;   // parameters
    float sampleRate, inc, k;                                   // derived
    float ph, ic1, ic2;                                         // state

    void setBase (float v) { base = v; }
    void setDepth (float v) { depth = v; }
    void setRate (float v) { rate = v; update(); }
    void setQ (float v) { q = v; update(); }

    void update()
    {
        inc = rate / sampleRate;
        k = 1.0f / q;
    }

    void prepare (float sr) { sampleRate = sr; update(); }

    void process (float* const* in, float* const* out, int n)
    {
        const float* x = in[0];
        float* o = out[0];
        const float sr = sampleRate, b = base, dep = depth, kk = k, dp = inc;
        float p = ph, s1 = ic1, s2 = ic2;
        for (int i = 0; i < n; ++i)
        {
            p += dp;
            p -= std::floor (p);
            const float lfo = std::sin (6.2831853071795862f * p);
            const float fc = b * std::pow (2.0f, dep * lfo);
            const float g = std::tan (3.14159265358979323f * fc / sr);
            const float a1 = 1.0f / (1.0f + g * (g + kk));
            const float a2 = g * a1;
            const float a3 = g * a2;
            const float v3 = x[i] - s2;
            const float v1 = a1 * s1 + a2 * v3;
            const float v2 = s2 + a2 * s1 + a3 * v3;
            s1 = 2.0f * v1 - s1;
            s2 = 2.0f * v2 - s2;
            o[i] = v2;
        }
        ph = p; ic1 = s1; ic2 = s2;
    }
};
using Dsp = SvfMod;
