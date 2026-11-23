// noise: linear congruential noise (Faust's no.noise) through a one-pole lowpass. The LCG runs in
// uint32_t (wrapping arithmetic, no signed-overflow UB) and is read back as a signed 32-bit int.
struct Noise
{
    static constexpr int numInputs = 0, numOutputs = 1;

    float damp = 0.9f;    // parameter
    float lp;             // state
    uint32_t seed;        // state

    void setDamp (float v) { damp = v; }
    void prepare (float) {}

    void process (float* const*, float* const* out, int n)
    {
        float* o = out[0];
        uint32_t s = seed;
        float y = lp;
        const float d = damp, g = 1.0f - damp;
        for (int i = 0; i < n; ++i)
        {
            s = s * 1103515245u + 12345u;
            const float x = float (int32_t (s)) * (1.0f / 2147483647.0f);
            y = x * g + y * d;
            o[i] = y;
        }
        seed = s;
        lp = y;
    }
};
using Dsp = Noise;
