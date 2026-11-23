// hoa_octo: examples/ambisonics/fourSourcesToOcto.dsp. Four mono sources encoded at order 3 by ho.map
// (distance gain, circular harmonics W, S1, C1, S2, C2, S3, C3, ho.wider opening min(r, 1)), radius and
// angle smoothed by si.smooth(tau2pole(0.02)), summed, then ho.optimInPhase(3) and ho.decoder(3, 8)
// folded in one 8 x 7 matrix of constants.
struct HoaOcto
{
    static constexpr int numInputs = 4, numOutputs = 8;

    // Speaker k at 2 pi k / 8: 0.5 * 2/8 for W, then 2/8 * inPhase weight (0.75, 0.3, 0.05) * sin/cos(n a_k)
    static constexpr float M[8][7] = {
        { 0.125f, 0.0f, 0.1875f, 0.0f, 0.075f, 0.0f, 0.0125f },
        { 0.125f, 0.13258252f, 0.13258252f, 0.075f, 0.0f, 0.008838835f, -0.008838835f },
        { 0.125f, 0.1875f, 0.0f, 0.0f, -0.075f, -0.0125f, 0.0f },
        { 0.125f, 0.13258252f, -0.13258252f, -0.075f, 0.0f, 0.008838835f, 0.008838835f },
        { 0.125f, 0.0f, -0.1875f, 0.0f, 0.075f, 0.0f, -0.0125f },
        { 0.125f, -0.13258252f, -0.13258252f, 0.075f, 0.0f, -0.008838835f, 0.008838835f },
        { 0.125f, -0.1875f, 0.0f, 0.0f, -0.075f, 0.0125f, 0.0f },
        { 0.125f, -0.13258252f, 0.13258252f, -0.075f, 0.0f, -0.008838835f, -0.008838835f } };

    float radius[4] = { 1.0f, 1.0f, 1.0f, 1.0f };   // parameters
    float angle[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    float pole;                                     // derived
    // si.smooth (error form) states: y = x + e, e = pole * (e + x[n-1] - x)
    float rE[4], rXp[4], aE[4], aXp[4];

    void setRadius (int i, float v) { radius[i] = v; }
    void setAngle (int i, float v) { angle[i] = v; }

    void prepare (float sr) { pole = std::exp (-(50.0f / sr)); }   // ba.tau2pole(0.02)

    static float smooth (float p, float& e, float& xp, float x)
    {
        e = p * (std::min (3.4028235e38f, xp - x) + e);
        xp = x;
        return x + e;
    }

    // ho.map(3, x, r, a) added to the 7 channels W, S1, C1, S2, C2, S3, C3
    static void encode (float* ch, float x, float r, float a)
    {
        const float op = std::min (r, 1.0f);
        const float g = x * (1.0f + 1.3862944f * (1.0f - op)) / std::max (r * r, 1.0f);
        // ho.wider weights of orders 1, 2, 3, clipped to [0, 1]
        const float g1 = g * std::clamp (2.0f * op, 0.0f, 1.0f);
        const float g2 = g * std::clamp (2.4663033f * (-0.6931472f + 1.3862944f * op), 0.0f, 1.0f);
        const float g3 = g * std::clamp (3.4760594f * (-1.0986123f + 1.3862944f * op), 0.0f, 1.0f);
        const float a2 = 2.0f * a;
        const float a3 = 3.0f * a;
        ch[0] += g;
        ch[1] += g1 * std::sin (a);
        ch[2] += g1 * std::cos (a);
        ch[3] += g2 * std::sin (a2);
        ch[4] += g2 * std::cos (a2);
        ch[5] += g3 * std::sin (a3);
        ch[6] += g3 * std::cos (a3);
    }

    void process (float* const* in, float* const* out, int n)
    {
        const float* x[4];
        float* y[8];
        float re[4], rxp[4], ae[4], axp[4], rv[4], av[4];
        for (int s = 0; s < 4; ++s)
        {
            x[s] = in[s];
            re[s] = rE[s]; rxp[s] = rXp[s]; ae[s] = aE[s]; axp[s] = aXp[s];
            rv[s] = radius[s]; av[s] = angle[s];
        }
        for (int k = 0; k < 8; ++k) y[k] = out[k];
        const float p = pole;
        for (int i = 0; i < n; ++i)
        {
            float ch[7] = {};
            for (int s = 0; s < 4; ++s)
            {
                const float r = smooth (p, re[s], rxp[s], rv[s]);
                const float a = smooth (p, ae[s], axp[s], av[s]);
                encode (ch, x[s][i], r, a);
            }
            for (int k = 0; k < 8; ++k)
            {
                float acc = 0.0f;
                for (int j = 0; j < 7; ++j) acc += M[k][j] * ch[j];
                y[k][i] = acc;
            }
        }
        for (int s = 0; s < 4; ++s) { rE[s] = re[s]; rXp[s] = rxp[s]; aE[s] = ae[s]; aXp[s] = axp[s]; }
    }
};
using Dsp = HoaOcto;
