// bubble: examples/gameaudio/bubble.dsp of Faust, drop held at 1 (one bubble at the first sample): os.osc whose
// frequency rises linearly with time under an exponential decay smoothed by si.smooth(0.99), then
// dm.freeverb_demo (re.stereo_freeverb: 8 lowpass-feedback combs and 4 allpasses per channel, 48 kHz lengths).
struct Bubble
{
    static constexpr int numInputs = 0, numOutputs = 2;

    // comb and allpass lengths of the left channel; the right one is detuned by the stereo spread (23 + 2)
    static constexpr int CL0 = 1214, CL1 = 1293, CL2 = 1389, CL3 = 1475, CL4 = 1547, CL5 = 1622, CL6 = 1694, CL7 = 1760;
    static constexpr int AL0 = 605, AL1 = 480, AL2 = 371, AL3 = 244;
    static constexpr int SP = 25;

    float drop = 1.0f, freq = 600.0f, damp = 0.5f, roomSize = 0.5f, wet = 0.333299994f;    // parameters
    float sampleRate;
    float dmp, dmp1, wet2, f0, sigma, decay, fb, dry;   // derived (control rate)
    float sine[65536];                  // os.osc's table, sin(2 pi i / 65536)

    bool started;                       // os.osc phasor reset at the first sample
    float dropPrev;
    int t;                              // bubble time, in samples
    int phase, phaseFrac;               // os.osc phasor: 30-bit integer part and 24-bit fraction
    float ampPrev, ampE;                // si.smooth(0.99) of the decay (error form)
    // freeverb: comb delays, their write positions and feedback lowpass states; allpass delays and positions
    float cl0[CL0], cl1[CL1], cl2[CL2], cl3[CL3], cl4[CL4], cl5[CL5], cl6[CL6], cl7[CL7];
    float al0[AL0], al1[AL1], al2[AL2], al3[AL3];
    float cr0[CL0 + SP], cr1[CL1 + SP], cr2[CL2 + SP], cr3[CL3 + SP], cr4[CL4 + SP], cr5[CL5 + SP],
          cr6[CL6 + SP], cr7[CL7 + SP];
    float ar0[AL0 + SP], ar1[AL1 + SP], ar2[AL2 + SP], ar3[AL3 + SP];
    int ic[16], ia[8];
    float lp[16];

    void setDrop (float v) { drop = v; }
    void setFreq (float v) { freq = v; update(); }
    void setDamp (float v) { damp = v; update(); }
    void setRoomSize (float v) { roomSize = v; update(); }
    void setWet (float v) { wet = v; update(); }

    void update()
    {
        const float sr = sampleRate;
        dmp = (17640.0f / sr) * damp;           // freeverb damping: scaledamp * origSR / SR
        dmp1 = 1.0f - dmp;
        wet2 = 0.2f * wet;                      // g * fixedgain, both channels summed by stereo_freeverb
        f0 = (1.0f / sr) * freq;
        const float dc = 0.043f * freq + 0.0014f * std::pow (freq, 1.5f);  // bubble damping
        sigma = (0.075f / sr) * dc;
        decay = (1.0f / sr) * dc;
        fb = (12348.0f / sr) * roomSize + 0.7f; // comb feedback: scaleroom * origSR / SR + offsetroom
        dry = 1.0f - wet;
    }

    void prepare (float sr)
    {
        sampleRate = sr;
        for (int i = 0; i < 65536; ++i)
            sine[i] = std::sin ((float) i * 6.28318548f / 65536.0f);
        update();
    }

    // lowpass-feedback comb: returns the delayed sample, writes x + fb * lowpass(delayed)
    template <int L>
    static inline float comb (float* buf, int& i, float& s, float x, float d, float d1, float f)
    {
        const float y = buf[i];
        s = d * s + d1 * y;
        buf[i] = x + f * s;
        if (++i == L)
            i = 0;
        return y;
    }

    template <int L>
    static inline void push (float* buf, int& i, float x)
    {
        buf[i] = x;
        if (++i == L)
            i = 0;
    }

    void process (float* const*, float* const* out, int n)
    {
        float* o1 = out[0];
        float* o2 = out[1];
        const float DMP = dmp, DMP1 = dmp1, W2 = wet2, F0 = f0, SG = sigma, DE = decay, FB = fb, DRY = dry, DR = drop;
        const float* sn = sine;
        bool st = started;
        float dp = dropPrev, ap = ampPrev, ae = ampE;
        int lt = t, ph = phase, phf = phaseFrac;
        int c[16], a[8];
        float s[16];
        for (int k = 0; k < 16; ++k) { c[k] = ic[k]; s[k] = lp[k]; }
        for (int k = 0; k < 8; ++k) a[k] = ia[k];
        for (int i = 0; i < n; ++i)
        {
            // bubble time: restarts on the rising edge of drop
            if (DR > dp)
                lt = 1;
            else
                lt++;
            dp = DR;
            const float ft = (float) lt;
            // os.osc at f0 * (1 + sigma * time)
            const float inc = F0 * (1.0f + SG * ft);
            const float fi = 1073741824.0f * (inc - std::rint (inc));
            const float fl = std::floor (fi);
            const int frac = phf + (int) (16777216.0f * (fi - fl));
            if (st)
            {
                phf = frac & 16777215;
                ph = (ph + (int) fl + (frac >> 24)) & 1073741823;
            }
            st = true;
            // exp(-damp * time) : si.smooth(0.99)
            const float av = std::exp (-(DE * ft));
            ae = 0.99f * (ae + (ap - av));
            ap = av;
            const float bubble = sn[((ph >> 6) >> 8) & 65535] * (av + ae);
            const float x = W2 * bubble;
            const float d = DRY * bubble;
            // left freeverb
            const float y7 = comb<CL7> (cl7, c[7], s[7], x, DMP, DMP1, FB);
            const float y6 = comb<CL6> (cl6, c[6], s[6], x, DMP, DMP1, FB);
            const float y5 = comb<CL5> (cl5, c[5], s[5], x, DMP, DMP1, FB);
            const float y4 = comb<CL4> (cl4, c[4], s[4], x, DMP, DMP1, FB);
            const float y3 = comb<CL3> (cl3, c[3], s[3], x, DMP, DMP1, FB);
            const float y2 = comb<CL2> (cl2, c[2], s[2], x, DMP, DMP1, FB);
            const float y1 = comb<CL1> (cl1, c[1], s[1], x, DMP, DMP1, FB);
            const float y0 = comb<CL0> (cl0, c[0], s[0], x, DMP, DMP1, FB);
            const float b0 = al0[a[0]];
            const float b1 = al1[a[1]];
            const float b2 = al2[a[2]];
            const float b3 = al3[a[3]];
            const float s0 = y7 + y6 + y5 + y4 + y3 + y2 + y1 + 0.5f * b0 + y0;
            push<AL0> (al0, a[0], s0);
            const float s1 = b0 + 0.5f * (b1 - s0);
            push<AL1> (al1, a[1], s1);
            const float s2 = b1 + 0.5f * (b2 - s1);
            push<AL2> (al2, a[2], s2);
            const float s3 = b2 + 0.5f * (b3 - s2);
            push<AL3> (al3, a[3], s3);
            o1[i] = b3 + d - 0.5f * s3;
            // right freeverb
            const float z7 = comb<CL7 + SP> (cr7, c[15], s[15], x, DMP, DMP1, FB);
            const float z6 = comb<CL6 + SP> (cr6, c[14], s[14], x, DMP, DMP1, FB);
            const float z5 = comb<CL5 + SP> (cr5, c[13], s[13], x, DMP, DMP1, FB);
            const float z4 = comb<CL4 + SP> (cr4, c[12], s[12], x, DMP, DMP1, FB);
            const float z3 = comb<CL3 + SP> (cr3, c[11], s[11], x, DMP, DMP1, FB);
            const float z2 = comb<CL2 + SP> (cr2, c[10], s[10], x, DMP, DMP1, FB);
            const float z1 = comb<CL1 + SP> (cr1, c[9], s[9], x, DMP, DMP1, FB);
            const float z0 = comb<CL0 + SP> (cr0, c[8], s[8], x, DMP, DMP1, FB);
            const float c0 = ar0[a[4]];
            const float c1 = ar1[a[5]];
            const float c2 = ar2[a[6]];
            const float c3 = ar3[a[7]];
            const float r0 = z7 + z6 + z5 + z4 + z3 + z2 + z1 + 0.5f * c0 + z0;
            push<AL0 + SP> (ar0, a[4], r0);
            const float r1 = c0 + 0.5f * (c1 - r0);
            push<AL1 + SP> (ar1, a[5], r1);
            const float r2 = c1 + 0.5f * (c2 - r1);
            push<AL2 + SP> (ar2, a[6], r2);
            const float r3 = c2 + 0.5f * (c3 - r2);
            push<AL3 + SP> (ar3, a[7], r3);
            o2[i] = d + c3 - 0.5f * r3;
        }
        started = st;
        dropPrev = dp; ampPrev = ap; ampE = ae;
        t = lt; phase = ph; phaseFrac = phf;
        for (int k = 0; k < 16; ++k) { ic[k] = c[k]; lp[k] = s[k]; }
        for (int k = 0; k < 8; ++k) ia[k] = a[k];
    }
};
using Dsp = Bubble;
