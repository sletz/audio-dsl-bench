// tanh_drive: input times drive through tanh, halved. Hand-written C++ port.
struct TanhDrive
{
    static constexpr int numInputs = 1, numOutputs = 1;

    float drive = 4.0f;   // parameter

    void setDrive (float v) { drive = v; }
    void prepare (float) {}

    void process (float* const* in, float* const* out, int n)
    {
        const float* x = in[0];
        float* y = out[0];
        const float d = drive;
        for (int i = 0; i < n; ++i)
            y[i] = std::tanh (x[i] * d) * 0.5f;
    }
};
using Dsp = TanhDrive;
