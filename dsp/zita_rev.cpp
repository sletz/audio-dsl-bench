// zita_rev: re.zita_rev1_stereo(60, f1, f2, t60dc, t60m, fsmax = 48000): 8-line FDN with allpass diffusers,
// two-band decay filters and a Hadamard feedback matrix; delay lengths computed for 48 kHz exactly as
// reverbs.lib does (floor(0.5 + SR*t)). The 8 allpass and the 8 feedback lines each sit one after the
// other in one buffer.
struct ZitaRev
{
    static constexpr int numInputs = 2, numOutputs = 2;
    static constexpr float tdel[8] = { 7350.0f, 10099.0f, 6136.0f, 12331.0f, 8386.0f, 9231.0f, 6000.0f, 10560.0f };
    static constexpr int inLen = 1920;   // input delay rdel - 20 = 40 ms
    static constexpr int apLen[8] = { 977, 1172, 1517, 1312, 1099, 1406, 646, 918 };
    static constexpr int apOff[8] = { 0, 977, 2149, 3666, 4978, 6077, 7483, 8129 };
    static constexpr int fbLen[8] = { 6372, 8926, 4618, 11018, 7286, 7824, 5353, 9641 };
    static constexpr int fbOff[8] = { 0, 6372, 15298, 19916, 30934, 38220, 46044, 51397 };
    static constexpr float apG[8] = { 0.6f, -0.6f, 0.6f, -0.6f, 0.6f, -0.6f, 0.6f, -0.6f };

    float f1 = 200.0f, f2 = 6000.0f, t60dc = 3.0f, t60m = 2.0f;   // parameters
    float sampleRate, gk, gk2, t1, inv;                           // derived
    float shelf[8], gm[8], pole[8];                               // decay filters, from the parameters
    float hp[8];         // Hadamard outputs of the previous sample
    float s1[8];         // one-pole TPT lowpass states (low shelf)
    float e[8], xp[8];   // si.smooth error states and previous inputs
    float dl[inLen], dr[inLen];
    float apBuf[9047];
    float fbBuf[61038];
    int di, api[8], fbi[8];

    void setF1 (float v) { f1 = v; update(); }
    void setF2 (float v) { f2 = v; update(); }
    void setT60dc (float v) { t60dc = v; update(); }
    void setT60m (float v) { t60m = v; update(); }

    void update()
    {
        const float sr = sampleRate;
        const float pi = 3.14159265358979323846f;
        const float g1 = std::tan (std::max (0.0f, std::min (pi * f1 / sr, 0.499f * pi)));
        gk = g1 * (1.0f / (g1 + 1.0f));
        gk2 = 2.0f * gk;
        float c = std::cos (6.2831853071795862f * f2 / sr);
        if (f2 > 0.49f * sr)
            c = -1.0f;
        for (int i = 0; i < 8; ++i)
        {
            const float g0 = std::exp (-6.9077554f * tdel[i] / (t60dc * sr));
            const float gM = std::exp (-6.9077554f * tdel[i] / (t60m * sr));
            shelf[i] = g0 / gM - 1.0f;
            gm[i] = gM;
            const float gs = gM * gM;
            const float mbo2 = (1.0f - gs * c) / (1.0f - gs);
            pole[i] = mbo2 - std::sqrt (std::max (0.0f, mbo2 * mbo2 - 1.0f));
        }
        t1 = 0.525f / std::sqrt (t60m);
        inv = 1.0f / std::sqrt (8.0f);
    }

    void prepare (float sr) { sampleRate = sr; update(); }

    void process (float* const* in, float* const* out, int n)
    {
        const float* in1 = in[0];
        const float* in2 = in[1];
        float* out1 = out[0];
        float* out2 = out[1];
        const float k = gk, k2 = gk2, iv = inv, tt = t1;
        float sh_[8], gm_[8], po_[8], h[8], st[8], er[8], xpr[8];
        int ai[8], fi[8];
        for (int c = 0; c < 8; ++c)
        {
            sh_[c] = shelf[c]; gm_[c] = gm[c]; po_[c] = pole[c];
            h[c] = hp[c]; st[c] = s1[c]; er[c] = e[c]; xpr[c] = xp[c];
            ai[c] = api[c]; fi[c] = fbi[c];
        }
        int d = di;
        for (int i = 0; i < n; ++i)
        {
            // damping filters on the delayed Hadamard outputs
            float z[8];
            for (int c = 0; c < 8; ++c)
            {
                const float x = h[c];
                const float dd = x - st[c];
                const float v = st[c] + k * dd;
                st[c] = st[c] + k2 * dd;
                const float sh = gm_[c] * (x + sh_[c] * v);
                er[c] = po_[c] * (er[c] + (xpr[c] - sh));
                xpr[c] = sh;
                z[c] = iv * (sh + er[c]) + 1.0e-20f;
            }
            const float xl = 0.3f * dl[d];
            dl[d] = in1[i];
            const float xr = 0.3f * dr[d];
            dr[d] = in2[i];
            if (++d == inLen)
                d = 0;
            // feedback delays, then the allpass diffusers: (f +- x) - a*r (a - b == a + (-b) exactly)
            const float xin[8] = { xl, xl, -xl, -xl, xr, xr, -xr, -xr };
            float q[8];
            for (int c = 0; c < 8; ++c)
            {
                float* fb = fbBuf + fbOff[c];
                const float f = fb[fi[c]];
                fb[fi[c]] = z[c];
                if (++fi[c] == fbLen[c])
                    fi[c] = 0;
                float* ap = apBuf + apOff[c];
                const float r = ap[ai[c]];
                const float u = (f + xin[c]) - apG[c] * r;
                ap[ai[c]] = u;
                if (++ai[c] == apLen[c])
                    ai[c] = 0;
                q[c] = r + apG[c] * u;
            }
            // ro.hadamard(8): butterfly(8), then hadamard(4) on each half
            const float b0 = q[0] + q[4], b1 = q[1] + q[5], b2 = q[2] + q[6], b3 = q[3] + q[7];
            const float b4 = q[0] - q[4], b5 = q[1] - q[5], b6 = q[2] - q[6], b7 = q[3] - q[7];
            const float c0 = b0 + b2, c1 = b1 + b3, c2 = b0 - b2, c3 = b1 - b3;
            const float c4 = b4 + b6, c5 = b5 + b7, c6 = b4 - b6, c7 = b5 - b7;
            h[0] = c0 + c1; h[1] = c0 - c1; h[2] = c2 + c3; h[3] = c2 - c3;
            h[4] = c4 + c5; h[5] = c4 - c5; h[6] = c6 + c7; h[7] = c6 - c7;
            out1[i] = (h[1] + h[2]) * tt;
            out2[i] = (h[1] - h[2]) * tt;
        }
        for (int c = 0; c < 8; ++c)
        {
            hp[c] = h[c]; s1[c] = st[c]; e[c] = er[c]; xp[c] = xpr[c];
            api[c] = ai[c]; fbi[c] = fi[c];
        }
        di = d;
    }
};
using Dsp = ZitaRev;
