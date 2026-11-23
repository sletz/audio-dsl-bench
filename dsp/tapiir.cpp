// tapiir: tapiir(6, 2) (examples/delayEcho/tapiir.dsp): 6 delay lines (de.delay, dsize = 524288) in a 6x6
// feedback matrix, 2 inputs, 2 output mixers. Tap i delays 0.05*(i+1) s, every tap gain is 0.1, input gains 1,
// gains 1. Same sums in the same order as the C++ faust generates. The lines hold dsize samples and the delay
// is clamped to dsize - 1, which the 5 s range never reaches at 48 kHz.
struct Tapiir
{
    static constexpr int numInputs = 2, numOutputs = 2;
    static constexpr int numLines = 6, size = 524288, mask = size - 1;

    // parameters: for each delay line, then for each output mixer
    float tap[numLines][numLines] = { { 0.1f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f }, { 0.1f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f },
                                      { 0.1f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f }, { 0.1f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f },
                                      { 0.1f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f }, { 0.1f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f } };
    float inGain[numLines][2] = { { 1.0f, 1.0f }, { 1.0f, 1.0f }, { 1.0f, 1.0f },
                                  { 1.0f, 1.0f }, { 1.0f, 1.0f }, { 1.0f, 1.0f } };
    float gain[numLines] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
    float delay[numLines] = { 0.05f, 0.1f, 0.15f, 0.2f, 0.25f, 0.3f };   // seconds
    float outTap[2][numLines] = { { 0.1f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f }, { 0.1f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f } };
    float outIn[2][2] = { { 1.0f, 1.0f }, { 1.0f, 1.0f } };
    float outGain[2] = { 1.0f, 1.0f };

    float sampleRate;           // derived
    int k[numLines];            // delays in samples
    float buf[numLines][size];  // state
    float y[numLines];          // line outputs, one sample late (the ~)
    int w;                      // write position of every line

    void setDelay (int line, float v) { delay[line] = v; update(); }
    void setTap (int line, int t, float v) { tap[line][t] = v; }

    void update()
    {
        for (int i = 0; i < numLines; ++i)
            k[i] = int (std::min (524287.0f, std::max (0.0f, sampleRate * delay[i])));
    }

    void prepare (float sr) { sampleRate = sr; update(); }

    void process (float* const* in, float* const* out, int n)
    {
        const float* in1 = in[0];
        const float* in2 = in[1];
        float* out1 = out[0];
        float* out2 = out[1];
        float yv[numLines];
        int kd[numLines];
        for (int j = 0; j < numLines; ++j) { yv[j] = y[j]; kd[j] = k[j]; }
        // the gains in locals: the stores into buf could otherwise alias them
        float tp[numLines][numLines], ig[numLines][2], g[numLines], ot[2][numLines], oi[2][2], og[2];
        std::memcpy (tp, tap, sizeof (tp));
        std::memcpy (ig, inGain, sizeof (ig));
        std::memcpy (g, gain, sizeof (g));
        std::memcpy (ot, outTap, sizeof (ot));
        std::memcpy (oi, outIn, sizeof (oi));
        std::memcpy (og, outGain, sizeof (og));
        int wi = w;
        for (int i = 0; i < n; ++i)
        {
            const float x0 = in1[i];
            const float x1 = in2[i];
            float nv[numLines];
            for (int l = 0; l < numLines; ++l)
            {
                float acc = ig[l][0] * x0 + ig[l][1] * x1;
                for (int j = 0; j < numLines; ++j)
                    acc += tp[l][j] * yv[j];
                buf[l][wi] = g[l] * acc;
                nv[l] = buf[l][(wi - kd[l]) & mask];
            }
            for (int j = 0; j < numLines; ++j)
                yv[j] = nv[j];
            float o[2];
            for (int m = 0; m < 2; ++m)
            {
                float acc = oi[m][0] * x0 + oi[m][1] * x1;
                for (int j = 0; j < numLines; ++j)
                    acc += ot[m][j] * yv[j];
                o[m] = og[m] * acc;
            }
            out1[i] = o[0];
            out2[i] = o[1];
            wi = (wi + 1) & mask;
        }
        for (int j = 0; j < numLines; ++j)
            y[j] = yv[j];
        w = wi;
    }
};
using Dsp = Tapiir;
