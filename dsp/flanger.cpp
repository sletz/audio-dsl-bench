// flanger: dm.flanger_demo (pf.flanger_stereo with dmax = 8192, invert off, no bypass). Each channel: a feedback
// comb (x + fb*y into a fractional delay line, de.fdelay: linear interpolation) whose delay follows a recursive
// quadrature LFO (fi.nlf2 driven by an impulse: os.oscrs on the left, os.oscrc on the right); the output is half
// the sum of the input and the delayed signal one sample late.
struct Flanger
{
    static constexpr int numInputs = 2, numOutputs = 2;
    static constexpr int size = 16384, mask = size - 1;

    float flangeDelay = 10.0f, delayOffset = 1.0f, speed = 0.5f;   // parameters
    float depth = 1.0f, fb = 0.0f, level = 0.0f;
    float sampleRate, lvl, off, half, sw, cw;                       // derived
    float bufL[size], bufR[size];   // de.fdelay(8192, d) lines
    int w;                          // write index, shared by both lines
    float oscS, oscC;               // fi.nlf2 states: sine (os.oscrs) and cosine (os.oscrc)
    float imp = 1.0f;               // impulse driving fi.nlf2 (1 - 1')
    float yl, yr;                   // delay line outputs, previous sample

    void setFlangeDelay (float v) { flangeDelay = v; update(); }
    void setDelayOffset (float v) { delayOffset = v; update(); }
    void setSpeed (float v) { speed = v; update(); }
    void setDepth (float v) { depth = v; }
    void setFb (float v) { fb = v; }
    void setLevel (float v) { level = v; update(); }

    void update()
    {
        lvl = std::pow (10.0f, 0.05f * level);   // ba.db2linear
        off = 0.001f * delayOffset;              // delay in seconds: off + half * (1 + lfo)
        half = 0.0005f * flangeDelay;
        const float wf = 6.2831853071795862f / sampleRate * speed;
        sw = std::sin (wf);
        cw = std::cos (wf);
    }

    void prepare (float sr) { sampleRate = sr; update(); }

    // de.fdelay(8192, d) read after the write at wi: linear interpolation between the taps int(d) and
    // int(d) + 1 (0 <= d <= 8193)
    static float read (const float* buf, int wi, float d)
    {
        const int id = int (std::min (8193.0f, std::max (-1.0f, d)));
        const float fl = std::floor (d);
        return buf[(wi - std::min (8193, std::max (0, id))) & mask] * (1.0f - d + fl)
             + (d - fl) * buf[(wi - std::min (8193, std::max (0, id + 1))) & mask];
    }

    void process (float* const* in, float* const* out, int n)
    {
        const float* in1 = in[0];
        const float* in2 = in[1];
        float* out1 = out[0];
        float* out2 = out[1];
        const float sr = sampleRate, lv = lvl, of = off, hf = half, swv = sw, cwv = cw, dp = depth, fbv = fb;
        float os = oscS, oc = oscC, l = yl, r = yr;
        float im = imp;
        int wi = w;
        for (int i = 0; i < n; ++i)
        {
            const float s = swv * oc + cwv * os;
            const float c = im - swv * os + cwv * oc;
            os = s;
            oc = c;
            im = 0.0f;
            // left channel
            const float xl = lv * in1[i];
            bufL[wi] = xl + fbv * l;
            const float dl = std::max (0.0f, -1.0f + sr * (of + hf * (1.0f + s)));   // de.fdelay(dmax, curdel - 1)
            const float nl = read (bufL, wi, dl);
            out1[i] = 0.5f * (xl + dp * l);
            l = nl;
            // right channel
            const float xr = lv * in2[i];
            bufR[wi] = xr + fbv * r;
            const float dr = std::max (0.0f, -1.0f + sr * (of + hf * (1.0f + c)));
            const float nr = read (bufR, wi, dr);
            out2[i] = 0.5f * (xr + dp * r);
            r = nr;
            wi = (wi + 1) & mask;
        }
        imp = im;
        oscS = os;
        oscC = oc;
        yl = l;
        yr = r;
        w = wi;
    }
};
using Dsp = Flanger;
