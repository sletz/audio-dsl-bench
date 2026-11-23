// crybaby: ve.crybaby (dm.crybaby_demo without bypass). A biquad bandpass whose pole radius and angle
// follow the wah pedal, written on its state w and output v = w - w'; f2 = 1 + a1 + a2, fq = 1 - a2 and
// the gain go through si.smooth, f2 and fq corrected by e = s^(n+1) (ma.FTZ) so smooth(1) starts at 1.
struct CryBaby
{
    static constexpr int numInputs = 1, numOutputs = 1;

    float wah = 0.8f;                          // parameter
    float sampleRate, g, f2, fq, s;            // derived, smoothing pole
    float imp = 1.0f;                          // 1 - 1': 1 on the first sample, then 0
    float ei;                                  // s^(n+1)
    float gE, gXp, f2E, f2Xp, fqE, fqXp;       // si.smooth states
    float w, v;                                // filter state, output v = w - w'

    void setWah (float x) { wah = x; update(); }

    void update()
    {
        const float wc = std::max (0.0f, std::min (1.0f, wah));
        const float Q = std::pow (2.0f, 2.0f * (1.0f - wc) + 1.0f);
        const float fr = 450.0f * std::pow (2.0f, 2.3f * wc);
        g = 0.1f * std::pow (4.0f, wc) * 44100.0f / sampleRate;
        const float frn = fr / sampleRate;
        const float d = 3.1415926535897932f * frn / Q;
        const float sn = std::sin (3.1415926535897932f * frn);
        f2 = d * d + 4.0f * (1.0f - d) * (sn * sn);
        fq = d * (2.0f - d);
    }

    void prepare (float sr)
    {
        sampleRate = sr;
        s = std::pow (0.999f, 44100.0f / sr);
        update();
    }

    void process (float* const* in, float* const* out, int n)
    {
        const float* x = in[0];
        float* y = out[0];
        float im = imp, e1 = ei, ge = gE, gxp = gXp, f2e = f2E, f2xp = f2Xp, fqe = fqE, fqxp = fqXp;
        float ww = w, vv = v;
        const float sp = s, gt = g, f2t = f2, fqt = fq;
        for (int i = 0; i < n; ++i)
        {
            e1 = (im + e1) * sp;
            im = 0.0f;
            const float e = std::fabs (e1) > 1.1754944e-38f ? e1 : 0.0f;   // ma.FTZ
            ge = sp * (ge + (gxp - gt));
            gxp = gt;
            const float gs = gt + ge;
            f2e = sp * (f2e + (f2xp - f2t));
            f2xp = f2t;
            const float f2s = f2t + f2e + e;
            fqe = sp * (fqe + (fqxp - fqt));
            fqxp = fqt;
            const float fqs = fqt + fqe + e;
            const float vn = vv + (gs * x[i] - f2s * ww - fqs * vv);
            ww = ww + vn;
            vv = vn;
            y[i] = vn;
        }
        imp = im; ei = e1; gE = ge; gXp = gxp; f2E = f2e; f2Xp = f2xp; fqE = fqe; fqXp = fqxp;
        w = ww; v = vv;
    }
};
using Dsp = CryBaby;
