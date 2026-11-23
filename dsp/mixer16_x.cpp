// mixer16_x: 16-channel mixer, dB gain and constant-power pan per channel, master gain. The
// per-channel coefficients are recomputed when a parameter changes.
struct Mixer16
{
    static constexpr int numInputs = 16, numOutputs = 2;

    float gain[16] = { -6.0f, -6.0f, -6.0f, -6.0f, -6.0f, -6.0f, -6.0f, -6.0f,
                       -6.0f, -6.0f, -6.0f, -6.0f, -6.0f, -6.0f, -6.0f, -6.0f };
    float pan[16] = { 0.000000000f, 0.066666667f, 0.133333333f, 0.200000000f, 0.266666667f, 0.333333333f,
                      0.400000000f, 0.466666667f, 0.533333333f, 0.600000000f, 0.666666667f, 0.733333333f,
                      0.800000000f, 0.866666667f, 0.933333333f, 1.000000000f };
    float master = -3.0f;
    float gl[16], gr[16];   // derived

    void setGain (int i, float v) { gain[i] = v; update(); }
    void setPan (int i, float v) { pan[i] = v; update(); }
    void setMaster (float v) { master = v; update(); }

    void update()
    {
        const float m = std::pow (10.0f, master / 20.0f);
        for (int i = 0; i < 16; ++i)
        {
            const float g = std::pow (10.0f, gain[i] / 20.0f) * m;
            gl[i] = g * std::cos (pan[i] * 3.1415926535897932f * 0.5f);
            gr[i] = g * std::sin (pan[i] * 3.1415926535897932f * 0.5f);
        }
    }

    void prepare (float) { update(); }

    void process (float* const* in, float* const* out, int n)
    {
        const float* x[16];
        float cl[16], cr[16];
        for (int k = 0; k < 16; ++k) { x[k] = in[k]; cl[k] = gl[k]; cr[k] = gr[k]; }
        float* yl = out[0];
        float* yr = out[1];
        for (int i = 0; i < n; ++i)
        {
            float l = 0.0f, r = 0.0f;
            for (int k = 0; k < 16; ++k)
            {
                l += x[k][i] * cl[k];
                r += x[k][i] * cr[k];
            }
            yl[i] = l;
            yr[i] = r;
        }
    }
};
using Dsp = Mixer16;
