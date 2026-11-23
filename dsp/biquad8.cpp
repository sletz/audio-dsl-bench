// biquad8: 8 identical RBJ lowpass biquads in series, direct form II, the state in an array of
// pairs loaded into locals for the block.
struct Biquad8
{
    static constexpr int numInputs = 1, numOutputs = 1;

    float cutoff = 1000.0f, q = 0.707f;    // parameters
    float sampleRate, b0, b1, b2, a1, a2;  // derived
    float w1[8], w2[8];                    // state

    void setCutoff (float v) { cutoff = v; update(); }
    void setQ (float v) { q = v; update(); }

    void update()
    {
        const float w0 = 6.2831853071795862f * cutoff / sampleRate;
        const float alpha = std::sin (w0) / (2.0f * q);
        const float cw = std::cos (w0);
        const float a0 = 1.0f + alpha;
        b0 = (1.0f - cw) * 0.5f / a0;
        b1 = (1.0f - cw) / a0;
        b2 = b0;
        a1 = -2.0f * cw / a0;
        a2 = (1.0f - alpha) / a0;
    }

    void prepare (float sr) { sampleRate = sr; update(); }

    void process (float* const* in, float* const* out, int n)
    {
        const float* x0 = in[0];
        float* y0 = out[0];
        float s1[8], s2[8];
        for (int k = 0; k < 8; ++k) { s1[k] = w1[k]; s2[k] = w2[k]; }
        for (int i = 0; i < n; ++i)
        {
            float x = x0[i];
            for (int k = 0; k < 8; ++k)
            {
                const float w = x + (-a1 * s1[k] - a2 * s2[k]);
                x = b0 * w + b1 * s1[k] + b2 * s2[k];
                s2[k] = s1[k];
                s1[k] = w;
            }
            y0[i] = x;
        }
        for (int k = 0; k < 8; ++k) { w1[k] = s1[k]; w2[k] = s2[k]; }
    }
};
using Dsp = Biquad8;
