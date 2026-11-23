// freeverb: Faust's freeverb.dsp structure: 8 damped combs in parallel, 4 allpasses in series. The 8 combs run
// side by side sample by sample (their recurrences are independent), summed c0 + c1 + ... + c7 into the output
// buffer; then each allpass runs over the chunk (up to 256 frames) in place, one after the other.
struct Freeverb
{
    static constexpr int numInputs = 1, numOutputs = 1;
    static constexpr int chunk = 256;
    static constexpr int combLengths[8] = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 };
    static constexpr int apLengths[4] = { 556, 441, 341, 225 };

    float room = 0.84f, damp = 0.2f;   // parameters
    float combBuf[11024];              // the 8 comb lines one after the other
    float apBuf[1563];                 // the 4 allpass lines one after the other
    int combIdx[8], apIdx[4];
    float combState[8];                // damping lowpass state of each comb

    void setRoom (float v) { room = v; }
    void setDamp (float v) { damp = v; }
    void prepare (float) {}

    void process (float* const* in, float* const* out, int n)
    {
        const float rm = room, dm = damp, dm1 = 1.0f - damp;
        for (int start = 0; start < n; start += chunk)
        {
            const int m = std::min (chunk, n - start);
            const float* x0 = in[0] + start;
            float* y0 = out[0] + start;
            int ci[8];
            float cs[8];
            for (int k = 0; k < 8; ++k) { ci[k] = combIdx[k]; cs[k] = combState[k]; }
            for (int i = 0; i < m; ++i)
            {
                const float x = x0[i] * 0.1f;
                float acc = 0.0f;
                float* line = combBuf;
                for (int k = 0; k < 8; ++k)
                {
                    const float y = line[ci[k]];
                    line[ci[k]] = x + rm * cs[k];
                    cs[k] = dm1 * y + dm * cs[k];
                    if (++ci[k] == combLengths[k])
                        ci[k] = 0;
                    acc = k == 0 ? y : acc + y;
                    line += combLengths[k];
                }
                y0[i] = acc;
            }
            for (int k = 0; k < 8; ++k) { combIdx[k] = ci[k]; combState[k] = cs[k]; }
            float* line = apBuf;
            for (int k = 0; k < 4; ++k)
            {
                const int size = apLengths[k];
                int j = apIdx[k];
                for (int i = 0; i < m; ++i)
                {
                    const float v = y0[i];
                    const float b = line[j];
                    line[j] = v + 0.5f * b;
                    if (++j == size)
                        j = 0;
                    y0[i] = b - v;
                }
                apIdx[k] = j;
                line += size;
            }
        }
    }
};
using Dsp = Freeverb;
