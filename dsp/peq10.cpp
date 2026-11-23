// peq10: 10-band parametric EQ (RBJ peaking, octave bands from 31.25 Hz, Q = 1.41), direct form I.
struct Peq10
{
    static constexpr int numInputs = 1, numOutputs = 1;

    float gains[10] = { 3.0f, -2.0f, 4.0f, -3.0f, 2.0f, -4.0f, 3.0f, -2.0f, 1.0f, -1.0f };   // parameters (dB)
    float sampleRate, b0[10], b1[10], b2[10], a1[10], a2[10];   // derived
    float x1[10], x2[10], y1[10], y2[10];                       // state

    void setGain (int band, float v) { gains[band] = v; update(); }

    void update()
    {
        for (int k = 0; k < 10; ++k)
        {
            const float A = std::pow (10.0f, gains[k] / 40.0f);
            const float w0 = 6.2831853071795862f * (31.25f * std::pow (2.0f, float (k))) / sampleRate;
            const float al = std::sin (w0) / (2.0f * 1.41f);
            const float cw = std::cos (w0);
            const float a0 = 1.0f + al / A;
            b0[k] = (1.0f + al * A) / a0;
            b1[k] = -2.0f * cw / a0;
            b2[k] = (1.0f - al * A) / a0;
            a1[k] = -2.0f * cw / a0;
            a2[k] = (1.0f - al / A) / a0;
        }
    }

    void prepare (float sr) { sampleRate = sr; update(); }

    void process (float* const* in, float* const* out, int n)
    {
        const float* x0 = in[0];
        float* o = out[0];
        float c0[10], c1[10], c2[10], d1[10], d2[10];
        float sx1[10], sx2[10], sy1[10], sy2[10];
        for (int k = 0; k < 10; ++k) { c0[k] = b0[k]; c1[k] = b1[k]; c2[k] = b2[k]; d1[k] = a1[k]; d2[k] = a2[k]; }
        for (int k = 0; k < 10; ++k) { sx1[k] = x1[k]; sx2[k] = x2[k]; sy1[k] = y1[k]; sy2[k] = y2[k]; }
        for (int i = 0; i < n; ++i)
        {
            float x = x0[i];
            for (int k = 0; k < 10; ++k)
            {
                const float y = c0[k] * x + c1[k] * sx1[k] + c2[k] * sx2[k] + (-d1[k] * sy1[k] - d2[k] * sy2[k]);
                sx2[k] = sx1[k];
                sx1[k] = x;
                sy2[k] = sy1[k];
                sy1[k] = y;
                x = y;
            }
            o[i] = x;
        }
        for (int k = 0; k < 10; ++k) { x1[k] = sx1[k]; x2[k] = sx2[k]; y1[k] = sy1[k]; y2[k] = sy2[k]; }
    }
};
using Dsp = Peq10;
