// fm_pad: two detuned 2-operator FM voices through a tanh shaper: cheap phase recursions,
// heavy recursion-free math (4 sin + 2 tanh per sample).
struct FmPad
{
    static constexpr int numInputs = 0, numOutputs = 1;

    float freq = 220.0f, ratio = 1.5f, index = 3.0f, drive = 2.0f, detune = 1.003f;   // parameters
    float sampleRate, ic1, im1, ic2, im2;   // derived: phase increments
    float pc1, pm1, pc2, pm2;               // state: carrier and modulator phases

    void setFreq (float v) { freq = v; update(); }
    void setRatio (float v) { ratio = v; update(); }
    void setIndex (float v) { index = v; }
    void setDrive (float v) { drive = v; }
    void setDetune (float v) { detune = v; update(); }

    void update()
    {
        // grouped as Faust's code computes them: (1/sr) * f
        const float t = 1.0f / sampleRate;
        ic1 = t * freq;
        im1 = t * (freq * ratio);
        ic2 = t * freq * detune;
        im2 = t * (freq * ratio) * detune;
    }

    void prepare (float sr) { sampleRate = sr; update(); }

    void process (float* const*, float* const* out, int n)
    {
        float* o = out[0];
        const float w = 6.2831853071795862f;
        const float kc1 = ic1, km1 = im1, kc2 = ic2, km2 = im2, idx = index, drv = drive;
        float c1 = pc1, m1 = pm1, c2 = pc2, m2 = pm2;
        for (int i = 0; i < n; ++i)
        {
            // wrap (p + inc), written as Faust's code computes it
            c1 = kc1 - std::floor (kc1 + c1) + c1;
            m1 = km1 - std::floor (km1 + m1) + m1;
            c2 = kc2 - std::floor (kc2 + c2) + c2;
            m2 = km2 - std::floor (km2 + m2) + m2;
            const float v1 = std::tanh (drv * std::sin (w * c1 + idx * std::sin (w * m1)));
            const float v2 = std::tanh (drv * std::sin (w * c2 + idx * std::sin (w * m2)));
            o[i] = (v1 + v2) * 0.25f;
        }
        pc1 = c1; pm1 = m1; pc2 = c2; pm2 = m2;
    }
};
using Dsp = FmPad;
