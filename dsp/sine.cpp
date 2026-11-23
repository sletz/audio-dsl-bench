// sine: phasor + sin. Hand-written C++ port: parameters are members whose setters recompute the derived
// coefficients (update()), the state is loaded into locals for the block and stored back at its end.
struct Sine
{
    static constexpr int numInputs = 0, numOutputs = 1;

    float freq = 440.0f;           // parameter
    float sampleRate, inc;         // derived
    float phase;                   // state

    void setFreq (float v) { freq = v; update(); }
    void update() { inc = freq / sampleRate; }
    void prepare (float sr) { sampleRate = sr; update(); }

    void process (float* const*, float* const* out, int n)
    {
        float* o = out[0];
        float p = phase;
        const float k = inc;
        for (int i = 0; i < n; ++i)
        {
            p += k;
            p -= std::floor (p);
            o[i] = std::sin (6.2831853071795862f * p) * 0.5f;
        }
        phase = p;
    }
};
using Dsp = Sine;
