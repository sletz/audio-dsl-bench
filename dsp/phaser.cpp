// phaser: dm.phaser2_demo (pf.phaser2_stereo with 4 notches, invert and vibrato mode off, no bypass). Each
// channel: a feedback loop around 4 second-order allpass sections (TPT state-variable filter in allpass form,
// x - bp(2k*x)) whose pole angles follow an LFO (fixed-point phasor, os.lf_sawpos; sine on the left, cosine on
// the right), then the dry input mixed with the allpass chain. Equations as the Faust compiler simplifies them.
struct Phaser
{
    static constexpr int numInputs = 2, numOutputs = 2;
    static constexpr float BIG = 3.4028235e38f;   // ma.MAX

    float speed = 0.5f, depth = 1.0f, fb = 0.0f, width = 1000.0f;   // parameters
    float frqmin = 100.0f, frqmax = 800.0f, fratio = 1.5f, level = 0.0f;
    float sampleRate, lvl, g1, g2, om2q, omc, kq, uq, th0, thr;     // derived
    float ratios[4];
    int incInt, incFrac;
    int phase, phaseFrac;   // os.lf_sawpos phasor: 30-bit integer part and 24-bit fraction
    float sl1[4], sl2[4], sr1[4], sr2[4];   // allpass section states (s1, s2), left and right
    float yl, yr;                           // chain outputs, fed back through fb

    void setSpeed (float v) { speed = v; update(); }
    void setDepth (float v) { depth = v; update(); }
    void setFb (float v) { fb = v; }
    void setWidth (float v) { width = v; update(); }
    void setFrqmin (float v) { frqmin = v; update(); }
    void setFrqmax (float v) { frqmax = v; update(); }
    void setFratio (float v) { fratio = v; update(); }
    void setLevel (float v) { level = v; update(); }

    void update()
    {
        const float sr = sampleRate;
        lvl = std::pow (10.0f, 0.05f * level);   // ba.db2linear
        g2 = 0.5f * depth;
        g1 = lvl * (1.0f - g2);
        const float hy = 0.5f * std::max (-(3.14159265358979323846f / sr * width), -80.0f);
        const float ex = std::exp (hy);
        const float sh = std::sinh (hy);         // om = -ma.expm1(-pi*width/SR) = -2*ex*sh
        om2q = (ex * ex) * (sh * sh);
        omc = 2.0f * ex * sh + 1.0f;
        const float fqq = sh * (ex * sh + 1.0f); // fq = -4*ex*fqq
        kq = 8.0f * ex * fqq;
        uq = 16.0f * ex * fqq;
        th0 = 6.2831853071795862f * frqmin;      // SR * (pole angle of the first notch), from frqmin ...
        thr = 3.14159265358979323846f * (frqmin - std::max (frqmin, frqmax));   // ... and frqmax
        ratios[0] = 1.0f / sr;                   // notch i at fratio^i times the first one
        ratios[1] = (1.0f / sr) * fratio;
        ratios[2] = (1.0f / sr) * (fratio * fratio);
        ratios[3] = (1.0f / sr) * (fratio * fratio * fratio);
        const float inc = 1073741824.0f * (1.0f / sr) * speed;   // 2^30 * f / SR
        incInt = int (std::floor (inc));
        incFrac = int (16777216.0f * (inc - std::floor (inc)));
    }

    void prepare (float sr) { sampleRate = sr; update(); }

    // One second-order allpass section (pf.vibrato2_mono's ap2p) at half pole angle h, with om2q = om^2/4,
    // omc = 1 - om, kq = -2 fq, uq = -4 fq (fq = 1 - R^2): returns the bandpass output bp, the section's
    // output being x - bp.
    static float ap2 (float& s1, float& s2, float x, float h, float om2q, float omc, float kq, float uq)
    {
        const float c = std::cos (h);
        const float s = std::sin (h);
        const float sq = om2q + omc * (c * c);   // S/4 (S = A(-1)), also the SVF's d
        const float pq = om2q + omc * (s * s);   // P/4 (P = A(1))
        const float iS = std::min (BIG, 0.25f / sq);
        const float g = std::sqrt (4.0f * pq * iS);
        const float t = sq * g * (iS * std::min (BIG, 1.0f / g) * ((4.0f * pq - kq) * s1 + uq * x) + s2);   // -g*hp
        const float bp = s1 - t;
        s1 = s1 - 2.0f * t;
        s2 = s2 + 2.0f * g * bp;
        return bp;
    }

    // The 4-section chain of one channel, with its feedback: returns the chain output y.
    static float chain (float* s1, float* s2, float x, float f, float b, const float* r, float om2q, float omc,
                        float kq, float uq)
    {
        const float a0 = ap2 (s1[0], s2[0], x + f, 0.5f * std::min (2.984513f, r[0] * b), om2q, omc, kq, uq);
        const float a1 = ap2 (s1[1], s2[1], x - a0 + f, 0.5f * std::min (2.984513f, r[1] * b), om2q, omc, kq, uq);
        const float a01 = a0 + a1;
        const float a2 = ap2 (s1[2], s2[2], x - a01 + f, 0.5f * std::min (2.984513f, r[2] * b), om2q, omc, kq, uq);
        const float a012 = a01 + a2;
        const float a3 = ap2 (s1[3], s2[3], x - a012 + f, 0.5f * std::min (2.984513f, r[3] * b), om2q, omc, kq, uq);
        return x - (a012 + a3) + f;
    }

    void process (float* const* in, float* const* out, int n)
    {
        const float* in1 = in[0];
        const float* in2 = in[1];
        float* out1 = out[0];
        float* out2 = out[1];
        const float lv = lvl, gg1 = g1, gg2 = g2, fbv = fb, a = om2q, b = omc, k = kq, u = uq, t0 = th0, tr = thr;
        const int ii = incInt, iff = incFrac;
        float r[4], l1[4], l2[4], q1[4], q2[4];
        for (int j = 0; j < 4; ++j)
        {
            r[j] = ratios[j];
            l1[j] = sl1[j]; l2[j] = sl2[j]; q1[j] = sr1[j]; q2[j] = sr2[j];
        }
        float l = yl, rr = yr;
        int ph = phase, pf = phaseFrac;
        for (int i = 0; i < n; ++i)
        {
            const float p = 0.00000037450704f * float (ph >> 6);   // 2 pi * os.lf_sawpos(speed)
            pf += iff;
            ph = (ii + ph + (pf >> 24)) & 1073741823;
            pf &= 16777215;
            // left channel, LFO phase 0 (sine)
            const float xl = lv * in1[i];
            l = chain (l1, l2, xl, fbv * l, t0 - tr * (1.0f - std::sin (p)), r, a, b, k, u);
            out1[i] = gg1 * in1[i] + gg2 * l;
            // right channel, LFO phase 1 (cosine)
            const float xr = lv * in2[i];
            rr = chain (q1, q2, xr, fbv * rr, t0 - tr * (1.0f - std::cos (p)), r, a, b, k, u);
            out2[i] = gg1 * in2[i] + gg2 * rr;
        }
        for (int j = 0; j < 4; ++j) { sl1[j] = l1[j]; sl2[j] = l2[j]; sr1[j] = q1[j]; sr2[j] = q2[j]; }
        yl = l;
        yr = rr;
        phase = ph;
        phaseFrac = pf;
    }
};
using Dsp = Phaser;
