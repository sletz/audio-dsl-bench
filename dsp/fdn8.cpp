// fdn8: 8-line feedback delay network: one-pole damping, Hadamard feedback matrix, mono in, sum out.
// The 8 delay lines share a power-of-two size and one write index.
struct Fdn8
{
    static constexpr int numInputs = 1, numOutputs = 1;
    static constexpr int size = 2048, mask = size - 1;
    // delay of each line in the delay-line part (L - 1; the feedback adds one sample)
    static constexpr int delays[8] = { 1008, 1150, 1278, 1380, 1498, 1606, 1732, 1860 };
    // Hadamard signs: (-1)^popcount(i & j)
    static constexpr float H[8][8] = {
        { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
        { 1.0f, -1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f, -1.0f },
        { 1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, -1.0f, -1.0f },
        { 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, -1.0f, -1.0f, 1.0f },
        { 1.0f, 1.0f, 1.0f, 1.0f, -1.0f, -1.0f, -1.0f, -1.0f },
        { 1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, -1.0f, 1.0f },
        { 1.0f, 1.0f, -1.0f, -1.0f, -1.0f, -1.0f, 1.0f, 1.0f },
        { 1.0f, -1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f } };

    float g = 0.9f, damp = 0.3f;   // parameters
    float gn;                      // derived
    float yp[8], s[8];             // state
    float d[8][size];
    int w;

    void setG (float v) { g = v; update(); }
    void setDamp (float v) { damp = v; }
    void update() { gn = g / std::sqrt (8.0f); }
    void prepare (float) { update(); }

    void process (float* const* in, float* const* out, int n)
    {
        const float* x0 = in[0];
        float* y0 = out[0];
        const float k = gn, dm = damp, dm1 = 1.0f - damp;
        float y[8], st[8];
        for (int c = 0; c < 8; ++c) { y[c] = yp[c]; st[c] = s[c]; }
        int wi = w;
        for (int i = 0; i < n; ++i)
        {
            for (int c = 0; c < 8; ++c)
                st[c] = dm1 * y[c] + dm * st[c];
            float v[8];
            for (int r = 0; r < 8; ++r)
            {
                float acc = 0.0f;
                for (int c = 0; c < 8; ++c)
                    acc += H[r][c] * st[c];
                v[r] = acc * k;
            }
            const float x = x0[i];
            for (int c = 0; c < 8; ++c)
            {
                y[c] = d[c][(wi - delays[c]) & mask];
                d[c][wi] = v[c] + x;
            }
            wi = (wi + 1) & mask;
            y0[i] = (y[0] + y[1] + y[2] + y[3] + y[4] + y[5] + y[6] + y[7]) * 0.25f;
        }
        for (int c = 0; c < 8; ++c) { yp[c] = y[c]; s[c] = st[c]; }
        w = wi;
    }
};
using Dsp = Fdn8;
