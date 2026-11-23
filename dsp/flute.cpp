// flute: pm.flute_ui of Faust's physmodels.lib. Blower (smoothed pressure, os.osc vibrato from a 65536-point
// sine table, low-passed breath noise); tube of head and foot segments, each a pair of de.fdelay4 delay lines,
// split at the embouchure (jet table); reflections *0.95 (the foot one through si.smooth); fi.dcblocker out.
// The order of operations is the one of the C++ that faust generates.
struct Flute
{
    static constexpr int numInputs = 0, numOutputs = 2;
    static constexpr int NMAX = 568;    // int(l2s(maxTubeLength = 4) + 4), the de.fdelay4 bound at 48 kHz
    static constexpr float BIG = 3.4028235e38f;

    float pressure = 0.899999976f, breathGain = 0.100000001f, breathCutoff = 2000.0f, vibratoFreq = 5.0f,
          vibratoGain = 0.25f, tubeLength = 0.800000012f, mouthPosition = 0.5f, outGain = 0.5f;  // parameters
    float sampleRate;
    float vg, bg, g, gk, c1, mp;        // derived (control rate)
    int incInt, incFrac;
    float footPole, dcPole, smoo, wk;   // sample-rate constants
    float sine[65536];                  // os.osc's table, sin(2 pi i / 65536)

    float tubeE, tubeXp, mouthE, mouthXp, pressE, pressXp;  // si.smoo states (error form)
    float footE;                        // foot lowpass (si.smooth) error state
    float footUp[1024];                 // foot segment, wave going up from the foot to the embouchure
    float headUp[1024];                 // head segment, wave going up from the embouchure to the head
    float headDown[1024];               // head segment, wave going down from the head to the embouchure
    float footDown[1024];               // foot segment, wave going down from the embouchure to the foot
    int w;                              // write position of the four lines (each written once per sample)
    float upIn;                         // footUp output, one sample late (reaches the embouchure)
    float headTop, headTop2;            // headUp output, one and two samples late (reflected at the head)
    float downIn;                       // headDown output, one sample late (reaches the embouchure)
    float footOut;                      // footDown output, one sample late
    float foot1, foot2;                 // wave at the foot, previous sample and two samples back
    float dc;                           // fi.dcblocker state
    int phase, phaseFrac;               // os.osc phasor: 30-bit integer part and 24-bit fraction
    uint32_t seed;                      // no.noise
    float s1, s2;                       // breath lowpass (fi.lowpass(2, breathCutoff), TPT SVF)

    void setPressure (float v) { pressure = v; }
    void setBreathGain (float v) { breathGain = v; update(); }
    void setBreathCutoff (float v) { breathCutoff = v; update(); }
    void setVibratoFreq (float v) { vibratoFreq = v; update(); }
    void setVibratoGain (float v) { vibratoGain = v; update(); }
    void setTubeLength (float v) { tubeLength = v; }
    void setMouthPosition (float v) { mouthPosition = v; update(); }
    void setOutGain (float v) { outGain = v; }

    void update()
    {
        const float sr = sampleRate;
        vg = 0.03f * vibratoGain;
        const float inc = 1073741824.0f * (1.0f / sr) * vibratoFreq;   // 2^30 * f / SR
        incInt = (int) std::floor (inc);
        incFrac = (int) (16777216.0f * (inc - std::floor (inc)));
        bg = 0.05f * breathGain;
        g = std::tan (std::max (0.0f, std::min (3.14159274f / sr * breathCutoff, 1.5676547f)));
        gk = g + 1.4142135f;
        c1 = g * std::min (BIG, 1.0f / (g * gk + 1.0f));
        mp = mouthPosition - 0.5f;
    }

    void prepare (float sr)
    {
        sampleRate = sr;
        for (int i = 0; i < 65536; ++i)
            sine[i] = std::sin ((float) i * 6.28318548f / 65536.0f);
        footPole = std::pow (0.7f, 44100.0f / sr);
        dcPole = std::pow (0.995f, 44100.0f / sr);
        smoo = 1.0f - 44.1f / sr;
        wk = 2136.283f / sr;
        update();
    }

    // de.fdelay4 taps (5 integer delays) for a delay of d samples; returns floor(d - 1.499995)
    static inline float lagrangeTaps (float d, int* taps)
    {
        const float dmo = d - 1.499995f;
        const int id = (int) std::min ((float) NMAX, std::max (-4.0f, dmo));
        for (int k = 0; k < 5; ++k)
            taps[k] = 1 + std::min (NMAX, std::max (0, id + k));
        return std::floor (dmo);
    }

    // de.fdelay4 read; swapped selects the order of one product (0.041666668 * a * x0), as in faust's C++
    template <bool swapped>
    static inline float read4 (const float* buf, int w, const int* taps, float d, float fl)
    {
        const float fd = d - fl;
        const float a = (-1.0f - fl) + d;
        const float b = (-2.0f - fl) + d;
        const float c = (-3.0f - fl) + d;
        const float q = (-4.0f - fl) + d;
        const float p1 = fd * a;
        const float p2 = p1 * b;
        const float p3 = p2 * c;
        const float x0 = buf[(w - taps[0]) & 1023];
        const float x1 = buf[(w - taps[1]) & 1023];
        const float x2 = buf[(w - taps[2]) & 1023];
        const float x3 = buf[(w - taps[3]) & 1023];
        const float x4 = buf[(w - taps[4]) & 1023];
        const float a0 = swapped ? 0.041666668f * a * x0 : 0.041666668f * x0 * a;
        return q * (c * (b * (a0 - 0.16666667f * fd * x1) + 0.25f * p1 * x2) - 0.16666667f * p2 * x3)
               + 0.041666668f * p3 * x4;
    }

    void process (float* const*, float* const* out, int n)
    {
        float* o1 = out[0];
        float* o2 = out[1];
        const float sr = sampleRate;
        const float VG = vg, BG = bg, G = g, GK = gk, C1 = c1, MP = mp, FP = footPole, DP = dcPole, SM = smoo, WK = wk;
        const float PR = pressure, TL = tubeLength, MPOS = mouthPosition, OG = outGain;
        const int II = incInt, IF = incFrac;
        const float* sn = sine;
        float* fu = footUp;
        float* hu = headUp;
        float* hd = headDown;
        float* fdn = footDown;
        float te = tubeE, txp = tubeXp, me = mouthE, mxp = mouthXp, pre = pressE, pxp = pressXp, fe = footE;
        int lw = w, ph = phase, phf = phaseFrac;
        float lup = upIn, ht = headTop, ht2 = headTop2, ldn = downIn, fo = footOut, f1 = foot1, f2 = foot2, ldc = dc;
        uint32_t sd = seed;
        float ls1 = s1, ls2 = s2;
        for (int i = 0; i < n; ++i)
        {
            lw = (lw + 1) & 1023;
            // foot: the wave reaching the foot is reflected (*0.95) through the dispersion lowpass
            fe = FP * (fe + std::min (BIG, -(0.95f * (f1 - f2))));
            fu[lw] = 0.95f * f1 + fe;
            // tube length in samples from the period wanted (pm.fluteModel's fit)
            te = SM * (std::min (BIG, txp - TL) + te);
            txp = TL;
            const float tl = TL + te;
            const float wt = WK / tl;
            const float num = -6.418f + sr * (0.000037619047f + 0.0029411765f * tl
                                              - 0.0006464499f * tl
                                                    * std::atan2 (FP * std::sin (wt), 1.0f - FP * std::cos (wt)));
            me = SM * (std::min (BIG, mxp - MPOS) + me);
            mxp = MPOS;
            const float em = 0.4f * (MP + me);  // 0.4 * (smoothed mouthPosition - 0.5), as faust reorders it
            const float epos = 0.27f + em;      // embouchure position on the tube
            const float den = 1.0f - 0.5f * epos;
            // foot segment: embouchure to foot distance, in samples
            int footTaps[5];
            const float footD = 0.6287726f * (num * (0.73f - em) / den);
            const float footFl = lagrangeTaps (footD, footTaps);
            const float up = read4<false> (fu, lw, footTaps, footD, footFl);
            hu[lw] = lup;
            // head segment: head to embouchure distance, in samples
            int headTaps[5];
            const float headD = 0.6287726f * (num * epos / den);
            const float headFl = lagrangeTaps (headD, headTaps);
            const float top = read4<false> (hu, lw, headTaps, headD, headFl);
            hd[lw] = 0.95f * ht2;
            const float down = read4<true> (hd, lw, headTaps, headD, headFl);
            // blower: pressure + vibrato + breath noise
            pre = SM * (std::min (BIG, pxp - PR) + pre);
            pxp = PR;
            const float pe = pre;               // smoothed pressure = pressure + pe
            const float vib = sn[((ph >> 6) >> 8) & 65535];
            phf += IF;
            ph = (II + ph + (phf >> 24)) & 1073741823;
            phf &= 16777215;
            sd = 12345u + 1103515245u * sd;
            const float t = 0.0000000004656613f * (float) (int32_t) sd - (GK * ls1 + ls2);
            const float v1 = ls1 + C1 * t;
            const float lp = ls2 + G * v1;
            ls1 += 2.0f * C1 * t;
            ls2 += 2.0f * G * v1;
            // embouchure: jet table on the blow minus half the wave coming down from the head
            const float x = PR - 0.5f * ldn + pe + VG * vib + BG * (PR + pe) * lp;
            fdn[lw] = 0.5f * lup + std::min (1.0f, std::max (-1.0f, x * (-1.0f + x * x)));
            const float foot = read4<true> (fdn, lw, footTaps, footD, footFl);
            // one-sample delays of the chain
            lup = up;
            ht2 = ht;
            ht = top;
            ldn = down;
            f2 = f1;
            f1 = fo;
            fo = foot;
            // fi.dcblocker on the wave at the foot
            ldc = DP * ldc + f1 - f2;
            const float o = OG * ldc;
            o1[i] = o;
            o2[i] = o;
        }
        tubeE = te; tubeXp = txp; mouthE = me; mouthXp = mxp; pressE = pre; pressXp = pxp; footE = fe;
        w = lw; phase = ph; phaseFrac = phf;
        upIn = lup; headTop = ht; headTop2 = ht2; downIn = ldn; footOut = fo; foot1 = f1; foot2 = f2; dc = ldc;
        seed = sd;
        s1 = ls1; s2 = ls2;
    }
};
using Dsp = Flute;
