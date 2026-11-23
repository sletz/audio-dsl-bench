// fir32c: 32-tap FIR, coefficients sin(0.3 (i+1)) / (i+1), circular history. The history is kept
// twice (h[j] and h[j + 32]) so the 32 taps read one contiguous window without index wrapping.
struct Fir32c
{
    static constexpr int numInputs = 1, numOutputs = 1;

    float c[32];          // coefficients
    float h[64];          // state: doubled circular history
    int idx;              // state: write index in [0, 32)

    void prepare (float)
    {
        for (int i = 0; i < 32; ++i)
            c[i] = std::sin (0.3f * float (i + 1)) / float (i + 1);
    }

    void process (float* const* in, float* const* out, int n)
    {
        const float* x = in[0];
        float* y = out[0];
        int w = idx;
        for (int i = 0; i < n; ++i)
        {
            // h[w + 32 - j] holds x[n - j]
            h[w] = h[w + 32] = x[i];
            const float* hp = h + w + 32;
            float acc = 0.0f;
            for (int j = 0; j < 32; ++j)
                acc += c[j] * hp[-j];
            y[i] = acc;
            w = (w + 1) & 31;
        }
        idx = w;
    }
};
using Dsp = Fir32c;
