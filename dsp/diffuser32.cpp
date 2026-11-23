// diffuser32: 32 Schroeder allpass combs in series (as fi.allpass_comb), alternating +-0.6. The stages are in
// series and have no parameters, so each one runs over the whole block in place in the output buffer.
struct Diffuser32
{
    static constexpr int numInputs = 1, numOutputs = 1;
    static constexpr int numStages = 32;
    static constexpr int lengths[numStages] = { 113, 127, 137, 149, 157, 173, 191, 211, 223, 239, 263,
                                                283, 307, 337, 367, 397, 431, 467, 509, 557, 599, 653,
                                                709, 769, 839, 911, 991, 1087, 1171, 1277, 1381, 1499 };
    static constexpr int totalLength = 17524;   // sum of lengths

    float buf[totalLength];   // the 32 delay lines one after the other
    int idx[numStages];       // read/write index of each line

    void prepare (float) {}

    void process (float* const* in, float* const* out, int n)
    {
        float* y = out[0];
        std::memcpy (y, in[0], (size_t) n * sizeof (float));
        float* line = buf;
        for (int k = 0; k < numStages; ++k)
        {
            const int size = lengths[k];
            const float a = (k & 1) ? -0.6f : 0.6f;
            int j = idx[k];
            for (int i = 0; i < n; ++i)
            {
                const float r = line[j];
                const float u = y[i] - a * r;
                line[j] = u;
                if (++j == size)
                    j = 0;
                y[i] = r + a * u;
            }
            idx[k] = j;
            line += size;
        }
    }
};
using Dsp = Diffuser32;
