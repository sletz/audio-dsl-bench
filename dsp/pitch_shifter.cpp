// pitch_shifter: ef.transpose(window, xfade, shift) of Faust's misceffects.lib (shift at 5 semitones).
// A phasor d runs through [0, window) (fmod); two taps of a linearly interpolated delay line, at d and
// d + window, are crossfaded by min(d / xfade, 1).
struct PitchShifter
{
    static constexpr int numInputs = 1, numOutputs = 1;

    float window = 1000.0f, xfade = 10.0f, shift = 5.0f;   // parameters
    float step, ix, omw;                                     // derived
    float buf[32768];                                        // de.fdelay line (d + window < 20001)
    int iota;
    float d;                                                 // read position, in [0, window)

    void setWindow (float v) { window = v; update(); }
    void setXfade (float v) { xfade = v; update(); }
    void setShift (float v) { shift = v; update(); }

    void update()
    {
        step = window + (1.0f - std::pow (2.0f, 0.083333336f * shift));
        ix = 1.0f / xfade;
        omw = 1.0f - window;
    }

    void prepare (float) { update(); }

    void process (float* const* in, float* const* out, int n)
    {
        const float* x = in[0];
        float* y = out[0];
        int w = iota;
        float dd = d;
        const float st = step, ixv = ix, om = omw, win = window;
        for (int i = 0; i < n; ++i)
        {
            buf[w] = x[i];
            dd = std::fmod (st + dd, win);
            const int i1 = int (std::min (65537.0f, std::max (-1.0f, dd)));
            const float f1 = std::floor (dd);
            const float xf = std::min (ixv * dd, 1.0f);
            const float d2 = win + dd;
            const int i2 = int (std::min (65537.0f, std::max (-1.0f, d2)));
            const float f2 = std::floor (d2);
            const float a0 = buf[(w - std::min (65537, std::max (0, i1))) & 32767];
            const float a1 = buf[(w - std::min (65537, std::max (0, i1 + 1))) & 32767];
            const float b0 = buf[(w - std::min (65537, std::max (0, i2))) & 32767];
            const float b1 = buf[(w - std::min (65537, std::max (0, i2 + 1))) & 32767];
            y[i] = (a0 * (1.0f - dd + f1) + (dd - f1) * a1) * xf
                 + (b0 * (om - dd + f2) + (win - f2 + dd) * b1) * (1.0f - xf);
            w = (w + 1) & 32767;
        }
        iota = w;
        d = dd;
    }
};
using Dsp = PitchShifter;
