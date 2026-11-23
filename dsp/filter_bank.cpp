// filter_bank: dm.filterbank_demo = fi.mth_octave_filterbank_default(1, 10000, 10), each band through
// its fader (dB, si.smoo), summed; no bypass. Analyzer: a dyadic tree of 5th-order Butterworth
// highpass/lowpass splits at 10000 * 2^(n-9) Hz, n = 9 ... 1, each a first-order section and two
// second-order ones (a1 = 1.618034, 0.618034); as in single-precision Faust (fi.tpt_df_fmin = 2304 Hz)
// the splits below 2304 Hz are trapezoidal (tf1s, svf.lp/hp) and the others direct form (tf1s_df,
// tf2s_df). Delay equalizer: band i goes through the allpasses fi.highpass_plus_lowpass(5, fc(j))
// (tf2s(1,-1.618,1,1.618,1) or its direct form), j = 1 ... 8-i. Same equations and operation order
// as dsp/filter_bank.cmajor, written with one small inline function per kind of section.
struct FilterBank
{
    static constexpr int numInputs = 1, numOutputs = 1;

    // coefficients of a trapezoidal split level (n = 1 ... 6), also used by its TPT allpass
    struct TptCoefs { float t, r, gkA, rA, gkB, rB, ga, ra; };
    // coefficients of a direct-form split level (n = 7 ... 9), a1A/a2A also used by its DF allpass
    struct DfCoefs { float lpb, hpb, af, a1A, a2A, lpA, hpA, a1B, a2B, lpB, hpB; };

    // section states
    struct TptSplit { float s, a1, a2, b1, b2; };          // tf1s state, two svf states
    struct DfSplit { float y1, a1, a2, b1, b2; };          // tf1 output, two biquad states
    struct TptAp { float s1, s0; };                        // tf2s states
    struct DfAp { float w1, w2; };                         // direct form states
    struct Smoo { float e, xq; };                          // si.smoo: error, previous input

    struct State
    {
        float xp[10];               // previous input of the direct-form split at level n (7 ... 9)
        DfSplit dfHp[10], dfLp[10]; // levels 7 ... 9
        TptSplit hp[7], lp[7];      // levels 1 ... 6
        TptAp ap[8][7];             // band b, level j = 1 ... 6 (j <= 8 - b)
        DfAp apDf[2][9];            // band b = 0, 1, level j = 7, 8 (j <= 8 - b)
        Smoo sm[11];                // band slider 1 ... 10
    };

    float band[11] = { 0.0f, -10.0f, -10.0f, -10.0f, -10.0f, -10.0f,
                       -10.0f, -10.0f, -10.0f, -10.0f, -10.0f };   // parameters Band1 ... Band10 (dB)
    float sampleRate;                                              // derived
    TptCoefs tc[7];
    DfCoefs dc[10];
    float smoo;
    State st;                                                      // state

    void setBand (int i, float v) { band[i] = v; }   // i = 1 ... 10

    void update()
    {
        const float sr = sampleRate;
        // PI * fc(n), fc(n) = 10000 * 2^(n-9)
        static constexpr float piFc[10] = { 0.0f, 122.71846f, 245.43693f, 490.87385f, 981.7477f,
                                            1963.4954f, 3926.9908f, 7853.9816f, 15707.963f, 31415.927f };
        for (int n = 1; n <= 6; ++n)
        {
            TptCoefs& c = tc[n];
            c.t = std::tan (std::max (0.0f, std::min (piFc[n] / sr, 1.5676547f)));
            c.r = std::min (3.4028235e38f, 1.0f / (1.0f + c.t));
            c.gkA = c.t + 1.618034f;
            c.rA = std::min (3.4028235e38f, 1.0f / (1.0f + c.t * c.gkA));
            c.gkB = c.t + 0.618034f;
            c.rB = std::min (3.4028235e38f, 1.0f / (1.0f + c.t * c.gkB));
            c.ga = 1.618034f * c.t;
            c.ra = std::min (3.4028235e38f, 1.0f / (1.0f + c.ga * (1.0f + c.ga * 0.38196602f)));
        }
        for (int n = 7; n <= 9; ++n)
        {
            DfCoefs& c = dc[n];
            const float t = std::tan (std::max (0.0f, std::min (piFc[n] / sr, 1.5676547f)));
            const float k = 1.0f / t;
            const float ksq = k * k;
            const float d = 1.0f + k;
            c.lpb = 1.0f / d;
            c.hpb = k / d;
            c.af = (1.0f - k) / d;
            const float dA = 1.0f + 1.618034f * k + ksq;
            c.a1A = 2.0f * (1.0f - ksq) / dA;
            c.a2A = (1.0f - 1.618034f * k + ksq) / dA;
            c.lpA = 1.0f / dA;
            c.hpA = ksq / dA;
            const float dB = 1.0f + 0.618034f * k + ksq;
            c.a1B = 2.0f * (1.0f - ksq) / dB;
            c.a2B = (1.0f - 0.618034f * k + ksq) / dB;
            c.lpB = 1.0f / dB;
            c.hpB = ksq / dB;
        }
        smoo = 1.0f - 44.1f / sr;   // si.smoo pole
    }

    void prepare (float sr) { sampleRate = sr; update(); }

    // hp of a direct-form split: tf1s_df, then tf2s_df with a1 = 1.618034 and a1 = 0.618034
    static inline float dfHp (DfSplit& s, const DfCoefs& c, float x, float xp)
    {
        s.y1 = c.hpb * (x - xp) - c.af * s.y1;
        const float aw = s.y1 - (c.a1A * s.a1 + c.a2A * s.a2);
        const float ay = c.hpA * (s.a2 + (aw - 2.0f * s.a1));
        s.a2 = s.a1;
        s.a1 = aw;
        const float bw = ay - (c.a1B * s.b1 + c.a2B * s.b2);
        const float by = c.hpB * (s.b2 + (bw - 2.0f * s.b1));
        s.b2 = s.b1;
        s.b1 = bw;
        return by;
    }

    // lp of a direct-form split
    static inline float dfLp (DfSplit& s, const DfCoefs& c, float x, float xp)
    {
        s.y1 = c.lpb * (x + xp) - c.af * s.y1;
        const float aw = s.y1 - (c.a1A * s.a1 + c.a2A * s.a2);
        const float ay = c.lpA * (s.a2 + aw + 2.0f * s.a1);
        s.a2 = s.a1;
        s.a1 = aw;
        const float bw = ay - (c.a1B * s.b1 + c.a2B * s.b2);
        const float by = c.lpB * (s.b2 + bw + 2.0f * s.b1);
        s.b2 = s.b1;
        s.b1 = bw;
        return by;
    }

    // hp of a trapezoidal split: tf1s, then svf.hp with k = 1.618034 and k = 0.618034
    static inline float tptHp (TptSplit& s, const TptCoefs& c, float x)
    {
        const float t = c.t;
        const float u = (x - s.s) * c.r;
        const float v = t * u + s.s;
        s.s = v + t * u;
        const float ah = (u - c.gkA * s.a1 - s.a2) * c.rA;
        const float av1 = t * ah + s.a1;
        const float av2 = t * av1 + s.a2;
        s.a1 = av1 + t * ah;
        s.a2 = av2 + t * av1;
        const float ay = u - 1.618034f * av1 - av2;
        const float bh = (ay - c.gkB * s.b1 - s.b2) * c.rB;
        const float bv1 = t * bh + s.b1;
        const float bv2 = t * bv1 + s.b2;
        s.b1 = bv1 + t * bh;
        s.b2 = bv2 + t * bv1;
        return ay - 0.618034f * bv1 - bv2;
    }

    // lp of a trapezoidal split: tf1s, then svf.lp with k = 1.618034 and k = 0.618034
    static inline float tptLp (TptSplit& s, const TptCoefs& c, float x)
    {
        const float t = c.t;
        const float u = (x - s.s) * c.r;
        const float v = t * u + s.s;
        s.s = v + t * u;
        const float ah = (v - c.gkA * s.a1 - s.a2) * c.rA;
        const float av1 = t * ah + s.a1;
        const float av2 = t * av1 + s.a2;
        s.a1 = av1 + t * ah;
        s.a2 = av2 + t * av1;
        const float bh = (av2 - c.gkB * s.b1 - s.b2) * c.rB;
        const float bv1 = t * bh + s.b1;
        const float bv2 = t * bv1 + s.b2;
        s.b1 = bv1 + t * bh;
        s.b2 = bv2 + t * bv1;
        return bv2;
    }

    // delay-equalizing allpass (highpass_plus_lowpass, order 5), trapezoidal tf2s
    static inline float tptAp (TptAp& s, const TptCoefs& c, float x)
    {
        const float u = (x - s.s1 - 0.38196602f * (c.ga * s.s1 + s.s0)) * c.ra;
        const float v1 = c.ga * u + s.s1;
        const float v0 = c.ga * v1 + s.s0;
        const float y = u - v1 + 0.38196602f * v0;
        s.s1 = 2.0f * v1 - s.s1;
        s.s0 = 2.0f * v0 - s.s0;
        return y;
    }

    // the same allpass in direct form
    static inline float dfAp (DfAp& s, const DfCoefs& c, float x)
    {
        const float w = x - (c.a1A * s.w1 + c.a2A * s.w2);
        const float y = s.w2 + (c.a1A * s.w1 + c.a2A * w);
        s.w2 = s.w1;
        s.w1 = w;
        return y;
    }

    // si.smoo (error form) of a band slider, then ba.db2linear
    static inline float fader (Smoo& s, float pole, float v)
    {
        s.e = pole * (s.e + std::min (3.4028235e38f, s.xq - v));
        s.xq = v;
        return std::pow (10.0f, 0.05f * (v + s.e));
    }

    void process (float* const* in, float* const* out, int n)
    {
        const float* x0 = in[0];
        float* o = out[0];
        TptCoefs t[7];
        DfCoefs d[10];
        float b[11];
        for (int k = 1; k <= 6; ++k) t[k] = tc[k];
        for (int k = 7; k <= 9; ++k) d[k] = dc[k];
        for (int k = 1; k <= 10; ++k) b[k] = band[k];
        const float pole = smoo;
        State s = st;

        for (int i = 0; i < n; ++i)
        {
            // analyzer: band i is hp(9-i) of the lowpassed signal, the dc band is lp(1)
            float hpOut[10];
            float x = x0[i];
            for (int k = 9; k >= 7; --k)
            {
                hpOut[k] = dfHp (s.dfHp[k], d[k], x, s.xp[k]);
                const float l = dfLp (s.dfLp[k], d[k], x, s.xp[k]);
                s.xp[k] = x;
                x = l;
            }
            for (int k = 6; k >= 1; --k)
            {
                hpOut[k] = tptHp (s.hp[k], t[k], x);
                x = tptLp (s.lp[k], t[k], x);
            }

            // delay equalizer: band bd (from hp(9-bd)) through the allpasses at fc(1) ... fc(8-bd)
            float bandOut[8];
            for (int bd = 0; bd < 8; ++bd)
            {
                float y = hpOut[9 - bd];
                const int last = 8 - bd;
                for (int j = 1; j <= std::min (last, 6); ++j)
                    y = tptAp (s.ap[bd][j], t[j], y);
                for (int j = 7; j <= last; ++j)
                    y = dfAp (s.apDf[bd][j], d[j], y);
                bandOut[bd] = y;
            }

            // faders, summed from the highest band down
            float g[11];
            for (int k = 10; k >= 1; --k)
                g[k] = fader (s.sm[k], pole, b[k]);
            o[i] = bandOut[0] * g[10] + bandOut[1] * g[9] + bandOut[2] * g[8] + bandOut[3] * g[7]
                 + bandOut[4] * g[6] + bandOut[5] * g[5] + bandOut[6] * g[4] + bandOut[7] * g[3]
                 + hpOut[1] * g[2] + x * g[1];
        }
        st = s;
    }
};
using Dsp = FilterBank;
