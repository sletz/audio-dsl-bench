// additive64: 64 harmonic partials with 1/k amplitudes, one phase per partial.
struct Additive64
{
    static constexpr int numInputs = 0, numOutputs = 1;

    float freq = 110.0f;               // parameter
    float sampleRate, inc[64], amp[64]; // derived
    float phase[64];                   // state

    void setFreq (float v) { freq = v; update(); }

    void update()
    {
        // (k + 1) / sr * freq, the grouping Faust's code uses (its (k+1)/sr is a constant)
        for (int k = 0; k < 64; ++k)
            inc[k] = float (k + 1) / sampleRate * freq;
    }

    void prepare (float sr)
    {
        sampleRate = sr;
        for (int k = 0; k < 64; ++k)
            amp[k] = 1.0f / float (k + 1);
        update();
    }

    void process (float* const*, float* const* out, int n)
    {
        float* o = out[0];
        float p[64];
        for (int k = 0; k < 64; ++k) p[k] = phase[k];
        for (int i = 0; i < n; ++i)
        {
            float acc = 0.0f;
            for (int k = 0; k < 64; ++k)
            {
                // wrap (p + inc), written as Faust's code computes it
                const float q = inc[k] - std::floor (inc[k] + p[k]) + p[k];
                p[k] = q;
                acc += std::sin (6.2831853071795862f * q) * amp[k];
            }
            o[i] = acc * 0.1f;
        }
        for (int k = 0; k < 64; ++k) phase[k] = p[k];
    }
};
using Dsp = Additive64;
