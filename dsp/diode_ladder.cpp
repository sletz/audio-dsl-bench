// diode_ladder: examples/filtering/diodeLadder.dsp of Faust: the 100 Hz os.sawtooth (saw2ptr, fixed-point
// phase) into ve.diodeLadder (ef.cubicnl(1, 0) * 1.5, then four zero-delay-feedback one-pole stages).
struct DiodeLadder
{
    static constexpr int numInputs = 0, numOutputs = 2;

    float Q = 1.0f, freq = 0.1f;            // parameters
    float qk;                               // derived (control rate)
    float c1, sp, p0, cm;                   // sample-rate constants
    int xi, xs;                             // saw2ptr increment in counts, whole and fraction
    int ph, phs;                            // phase counts and fraction
    float nE, nXp;                          // si.smoo of normFreq (error, previous input)
    float s1, s2, s3, s4;                   // the four stage states

    void setQ (float v) { Q = v; update(); }
    void setFreq (float v) { freq = v; }
    void update() { qk = (Q - 0.70710677f) / (25.0f - 0.70710677f); }

    void prepare (float sr)
    {
        c1 = 6.2831853071795862f / sr;
        sp = 1.0f - 44.1f / sr;
        const float t0 = 100.0f / sr;
        p0 = 0.01f * sr;
        cm = -1.0f - t0;
        const float xc = 1073741824.0f * (t0 - std::floor (t0));
        const float xfl = std::floor (xc);
        xi = (int) xfl;
        xs = (int) (16777216.0f * (xc - xfl));
        update();
    }

    void process (float* const*, float* const* out, int n)
    {
        float* o1 = out[0];
        float* o2 = out[1];
        const float fv = freq, kq = qk, C1 = c1, SP = sp, P0 = p0, CM = cm;
        const int XI = xi, XS = xs;
        int lph = ph, lphs = phs;
        float le = nE, lxp = nXp;
        float l1 = s1, l2 = s2, l3 = s3, l4 = s4;
        for (int i = 0; i < n; ++i)
        {
            // os.sawtooth(100)
            const int t = lphs + XS;
            lphs = t & 16777215;
            const int q = lph + XI + (t >> 24);
            lph = q & 1073741823;
            const float phi = (float) lph * 9.313226e-10f;
            float p = phi;
            if (q > 1073741823)
                p = phi * (1.0f - P0) + 1.0f;
            // ef.cubicnl(1, 0) * 1.5
            const float c = std::max (-1.0f, std::min (1.0f, 100.0f * (CM + 2.0f * p)));
            const float x = (c - c * c * c / 3.0f) * 1.5f;
            // coefficients
            le = SP * ((lxp - fv) + le);
            lxp = fv;
            const float nf = fv + le;
            const float g = std::tan (C1 * std::pow (10.0f, 1.0f + 3.0f * nf));
            const float k = (17.0f - std::pow (nf, 10.0f) * 9.7f) * kq;
            const float G4 = 0.5f * g / (1.0f + g);
            const float G3 = 0.5f * g / (1.0f + g - 0.5f * g * G4);
            const float G2 = 0.5f * g / (1.0f + g - 0.5f * g * G3);
            const float G1 = g / (1.0f + g - g * G2);
            const float Gamma = G1 * G2 * G3 * G4;
            const float SG1 = G4 * G3 * G2;
            const float SG3 = G4;
            const float alpha = g / (1.0f + g);
            const float alpha0 = 1.0f / (1.0f + k * Gamma);
            const float gam1 = 1.0f + G1 * G2;
            const float gam2 = 1.0f + G2 * G3;
            const float gam3 = 1.0f + G3 * G4;
            const float B1 = 1.0f / (1.0f + g - g * G2);
            const float B2 = 1.0f / (1.0f + g - 0.5f * g * G3);
            const float B3 = 1.0f / (1.0f + g - 0.5f * g * G4);
            const float B4 = 1.0f / (1.0f + g);
            const float d1 = g;
            const float d2 = 0.5f * g;
            const float d3 = 0.5f * g;
            // feedback sums S4 .. S1 of the previous states
            const float S4 = l4 * B4;
            const float S3 = (S4 * d3 + l3) * B3;
            const float S2 = (S3 * d2 + l2) * B2;
            const float S1 = (S2 * d1 + l1) * B1;
            // (the library uses SG3 for the S2 term, kept as is)
            const float u = x - S4 * k - S3 * SG3 * k - S2 * SG3 * k - S1 * SG1 * k;
            const float v1 = u * alpha0 * gam1 + S2 + S1 * G2;
            const float e1 = v1 - l1;
            const float lp1 = e1 * alpha + l1;
            const float v2 = (lp1 * gam2 + S3 + S2 * G3) * 0.5f;
            const float e2 = v2 - l2;
            const float lp2 = e2 * alpha + l2;
            const float v3 = (lp2 * gam3 + S4 + S3 * G4) * 0.5f;
            const float e3 = v3 - l3;
            const float lp3 = e3 * alpha + l3;
            const float v4 = lp3 * 0.5f;
            const float e4 = v4 - l4;
            const float y = e4 * alpha + l4;
            l1 = e1 * alpha * 2.0f + l1;
            l2 = e2 * alpha * 2.0f + l2;
            l3 = e3 * alpha * 2.0f + l3;
            l4 = e4 * alpha * 2.0f + l4;
            o1[i] = y;
            o2[i] = y;
        }
        ph = lph; phs = lphs;
        nE = le; nXp = lxp;
        s1 = l1; s2 = l2; s3 = l3; s4 = l4;
    }
};
using Dsp = DiodeLadder;
