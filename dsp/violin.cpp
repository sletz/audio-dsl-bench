// violin: pm.violin_ui_MIDI of Faust's physmodels.lib, gate held at 1. Bowed string: two segments (nut to bow,
// bow to bridge), each a pair of de.fdelay4 Lagrange-interpolated delay lines, nut and bridge reflections
// (pm.bridgeFilter), the bow table, and the body resonance (fi.resonbp at 500 Hz, TPT SVF).
struct Violin
{
    static constexpr int numInputs = 0, numOutputs = 2;
    static constexpr float C = 340.0f;                  // pm.speedOfSound
    static constexpr int NMAX = 427;                    // the de.fdelay4 bound at 48 kHz
    static constexpr float RHO_BRIDGE = 0.989264667f;   // pm.bridgeFilter(0.2, 0.9): t60 = 2 s
    static constexpr float RHO_NUT = 0.99880147f;       // pm.bridgeFilter(0.6, 0.1): t60 = 18 s

    float freq = 440.0f, bend = 0.0f, gain = 0.600000024f, envAttack = 1.0f, sustain = 0.0f, pressure = 0.5f,
          position = 0.699999988f, vibratoFrequency = 6.0f, vibratoGain = 0.5f, outGain = 0.5f,
          gate = 1.0f;                  // parameters
    float sampleRate;
    float ratio, gt, vg, target, envPole, smoo;     // derived (control rate)
    int incInt, incFrac;
    float g, gc, gk;                    // body resonance coefficients
    float sine[65536];                  // os.osc's table, sin(2 pi i / 65536)

    float bendE, bendXp, envE, envXp, posE, posXp, pressE, pressXp;    // si.smooth states (error form)
    float gatePrev;
    int phase, phaseFrac;               // os.osc phasor: 30-bit integer part and 24-bit fraction
    float bridgeToBow[2048], bowToBridge[2048];     // bow-to-bridge segment, waves going to the bow / bridge
    float bowToNut[2048], nutToBow[2048];           // nut-to-bow segment, waves going to the nut / bow
    int w;                              // write position of the four lines (they advance together)
    float fromBridge, fromNut;          // waves reaching the bow, previous sample
    float an1, an2, an3, atNutIn;       // wave reaching the nut, history (pm.bridgeFilter taps)
    float ab1, ab2, ab3, atBridgeIn;    // wave reaching the bridge, history
    float s1, s2;                       // body resonance (fi.resonbp(500, 2, 1))

    void setFreq (float v) { freq = v; }
    void setBend (float v) { bend = v; update(); }
    void setGain (float v) { gain = v; update(); }
    void setEnvAttack (float v) { envAttack = v; update(); }
    void setSustain (float v) { sustain = v; update(); }
    void setPressure (float v) { pressure = v; }
    void setPosition (float v) { position = v; }
    void setVibratoFrequency (float v) { vibratoFrequency = v; update(); }
    void setVibratoGain (float v) { vibratoGain = v; update(); }
    void setOutGain (float v) { outGain = v; }
    void setGate (float v) { gate = v; update(); }

    void update()
    {
        const float sr = sampleRate;
        ratio = std::pow (2.0f, 0.083333336f * bend);
        gt = std::min (1.0f, gate + sustain);
        vg = 0.01f * vibratoGain;
        const float inc = 1073741824.0f * (1.0f / sr) * vibratoFrequency;   // 2^30 * f / SR
        incInt = (int) std::floor (inc);
        incFrac = (int) (16777216.0f * (inc - std::floor (inc)));
        target = gain * gt;
        const float tau = 0.001f * envAttack;
        envPole = 0.0f;
        if (std::fabs (tau) >= 0.00000011920929f)
            envPole = std::exp (-((1.0f / sr) / tau));
        smoo = 1.0f - 44.1f / sr;                                           // si.smoo
    }

    void prepare (float sr)
    {
        sampleRate = sr;
        for (int i = 0; i < 65536; ++i)
            sine[i] = std::sin ((float) i * 6.28318548f / 65536.0f);
        g = std::tan (std::max (0.0f, std::min (3.14159274f * 500.0f / sr, 1.5676547f)));
        gc = g * std::min (3.4028235e38f, 1.0f / (g * (g + 0.5f) + 1.0f));
        gk = -0.5f - g;
        update();
    }

    // de.fdelay4 taps (5 integer delays) and fractional part for a delay of d samples
    static inline float lagrangeTaps (float d, int* taps)
    {
        const int id = (int) std::min ((float) NMAX, std::max (-4.0f, d - 1.499995f));
        for (int k = 0; k < 5; ++k)
            taps[k] = std::min (NMAX, std::max (0, id + k));
        return d - std::floor (d - 1.499995f);
    }

    // de.fdelay4: 4th-order Lagrange interpolation of the 5 taps around the delay
    static inline float read4 (const float* buf, int w, const int* taps, float fd)
    {
        const float x0 = buf[(w - taps[0]) & 2047];
        const float x1 = buf[(w - taps[1]) & 2047];
        const float x2 = buf[(w - taps[2]) & 2047];
        const float x3 = buf[(w - taps[3]) & 2047];
        const float x4 = buf[(w - taps[4]) & 2047];
        const float a = fd - 1.0f;
        const float b = fd - 2.0f;
        const float c = fd - 3.0f;
        const float q = fd - 4.0f;
        const float p1 = fd * a;
        const float p2 = p1 * b;
        const float p3 = p2 * c;
        return q * (c * (b * (0.041666668f * x0 * a - 0.16666667f * fd * x1) + 0.25f * p1 * x2) - 0.16666667f * p2 * x3)
               + 0.041666668f * p3 * x4;
    }

    void process (float* const*, float* const* out, int n)
    {
        float* o1 = out[0];
        float* o2 = out[1];
        const float sr = sampleRate;
        const float RT = ratio, GT = gt, VG = vg, TG = target, EP = envPole, SM = smoo;
        const float F = freq, OG = outGain, PS = position, PR = pressure, G = g, GC = gc, GK = gk;
        const int II = incInt, IF = incFrac;
        const float sk = 0.0014705883f * sr, loopC = 12.0f * C / sr;
        const float* sn = sine;
        float* bb = bridgeToBow;
        float* br = bowToBridge;
        float* bn = bowToNut;
        float* nb = nutToBow;
        float be = bendE, bxp = bendXp, ee = envE, exp_ = envXp, pe = posE, pxp = posXp, re = pressE, rxp = pressXp;
        float gp = gatePrev;
        int ph = phase, phf = phaseFrac, lw = w;
        float fbr = fromBridge, fnu = fromNut;
        float n1 = an1, n2 = an2, n3 = an3, nin = atNutIn;
        float b1 = ab1, b2 = ab2, b3 = ab3, bin = atBridgeIn;
        float ls1 = s1, ls2 = s2;
        for (int i = 0; i < n; ++i)
        {
            lw = (lw + 1) & 2047;
            // bridge reflection: -pm.bridgeFilter(0.2, 0.9)
            bb[lw] = -RHO_BRIDGE * (0.6f * b2 + 0.2f * (b1 + b3));
            // pitch: bend (si.polySmooth), vibrato (os.osc), envelope (bow velocity)
            float hold = 0.0f;
            if (GT == gp || GT == 0.0f)
                hold = 0.999f;
            gp = GT;
            be = hold * (be + (bxp - RT));
            bxp = RT;
            const float bendRatio = RT + be;
            const float vib = sn[((ph >> 6) >> 8) & 65535];
            phf += IF;
            ph = (II + ph + (phf >> 24)) & 1073741823;
            phf &= 16777215;
            ee = EP * (ee + (exp_ - TG));
            exp_ = TG;
            const float velocity = TG + ee;
            const float stringLength = C / F / (bendRatio * (1.0f + VG * vib * velocity)) - loopC;
            pe = SM * (pe + (pxp - PS));
            pxp = PS;
            const float p = PS + pe;
            int lower[5], upper[5];
            const float fdLower = lagrangeTaps (sk * stringLength * ((1.0f - PS) - pe), lower);
            const float fdUpper = lagrangeTaps (sk * stringLength * p, upper);
            const float reachBow = read4 (bb, lw - 1, lower, fdLower);
            const float reachNut = read4 (bn, lw - 1, upper, fdUpper);
            // nut reflection: -pm.bridgeFilter(0.6, 0.1)
            const float n0 = nin;
            nb[lw] = -RHO_NUT * (0.8f * n2 + 0.1f * (n1 + n3));
            const float reachBowFromNut = read4 (nb, lw - 2, upper, fdUpper);
            // bow: velocity difference through the bow table
            const float dv = velocity - (fbr + fnu);
            re = SM * (re + (rxp - PR));
            rxp = PR;
            const float slope = 5.0f - 4.0f * (PR + re);
            const float f = dv * std::min (1.0f, std::pow (0.75f + std::fabs (dv * slope), -4.0f));
            bn[lw] = fbr + f;
            br[lw] = fnu + f;
            const float reachBridge = read4 (br, lw, lower, fdLower);
            // body resonance on the wave reaching the bridge
            const float b0 = bin;
            const float v1 = ls1 + GC * (b1 + GK * ls1 - ls2);
            ls1 = 2.0f * v1 - ls1;
            ls2 = 2.0f * (ls2 + G * v1) - ls2;
            const float o = OG * v1;
            o1[i] = o;
            o2[i] = o;
            // one-sample state updates
            fbr = reachBow;
            fnu = reachBowFromNut;
            nin = reachNut;
            bin = reachBridge;
            n3 = n2; n2 = n1; n1 = n0;
            b3 = b2; b2 = b1; b1 = b0;
        }
        bendE = be; bendXp = bxp; envE = ee; envXp = exp_; posE = pe; posXp = pxp; pressE = re; pressXp = rxp;
        gatePrev = gp;
        phase = ph; phaseFrac = phf; w = lw;
        fromBridge = fbr; fromNut = fnu;
        an1 = n1; an2 = n2; an3 = n3; atNutIn = nin;
        ab1 = b1; ab2 = b2; ab3 = b3; atBridgeIn = bin;
        s1 = ls1; s2 = ls2;
    }
};
using Dsp = Violin;
