// clarinet: pm.clarinet_ui_MIDI of Faust's physmodels.lib, gate held at 1. Blower (pressure envelope, os.osc
// vibrato from a 65536-point sine table, low-passed breath noise), reed table, a bore of two de.fdelay4
// Lagrange-interpolated delay lines, and the bell reflection through a one-pole smoother.
struct Clarinet
{
    static constexpr int numInputs = 0, numOutputs = 2;
    static constexpr float C = 340.0f;      // pm.speedOfSound
    static constexpr int NMAX = 427;        // int(l2s(pm.maxLength) + 4), the de.fdelay4 bound at 48 kHz

    float freq = 440.0f, bend = 0.0f, gain = 0.600000024f, envAttack = 1.0f, sustain = 0.0f,
          reedStiffness = 0.5f, bellOpening = 0.5f, vibratoFreq = 5.0f, vibratoGain = 0.25f,
          outGain = 0.5f, gate = 1.0f;      // parameters
    float sampleRate;
    float pb, loopDelay, ratio, gt, vg, target, envPole, slope;    // derived (control rate)
    int incInt, incFrac;
    float g, gk, c1;                        // breath lowpass coefficients
    float sine[65536];                      // os.osc's table, sin(2 pi i / 65536)

    float bendE, bendXp, envE, envXp, bellE, bellXp;   // si.smooth states (error form)
    float gatePrev;
    int phase, phaseFrac;                   // os.osc phasor: 30-bit integer part and 24-bit fraction
    uint32_t seed;                          // no.noise
    float s1, s2;                           // breath lowpass (fi.lowpass(2, 2000), TPT SVF)
    float toMouth[2048];                    // wave travelling from the bell to the reed
    float toBell[2048];                     // wave travelling from the reed to the bell
    int w;                                  // write position of both lines (they advance together)
    float boreIn;                           // toMouth output, one sample late
    float mouth;                            // pressure at the reed, previous sample
    float bellOut;                          // toBell output reaching the bell
    float bellIn;                           // toBell output read this sample

    void setFreq (float v) { freq = v; }
    void setBend (float v) { bend = v; update(); }
    void setGain (float v) { gain = v; update(); }
    void setEnvAttack (float v) { envAttack = v; update(); }
    void setSustain (float v) { sustain = v; update(); }
    void setReedStiffness (float v) { reedStiffness = v; update(); }
    void setBellOpening (float v) { bellOpening = v; update(); }
    void setVibratoFreq (float v) { vibratoFreq = v; update(); }
    void setVibratoGain (float v) { vibratoGain = v; update(); }
    void setOutGain (float v) { outGain = v; }
    void setGate (float v) { gate = v; update(); }

    void update()
    {
        const float sr = sampleRate;
        pb = std::pow (bellOpening, 44100.0f / sr);
        loopDelay = C / sr * (0.964f * (pb / std::max (1.0f - pb, 0.01f)) + 5.99f);
        ratio = std::pow (2.0f, 0.083333336f * bend);
        gt = std::min (1.0f, gate + sustain);
        vg = 0.01f * vibratoGain;
        const float inc = 1073741824.0f * (1.0f / sr) * vibratoFreq;   // 2^30 * f / SR
        incInt = (int) std::floor (inc);
        incFrac = (int) (16777216.0f * (inc - std::floor (inc)));
        target = gain * gt;
        const float tau = 0.001f * envAttack;
        envPole = 0.0f;
        if (std::fabs (tau) >= 0.00000011920929f)
            envPole = std::exp (-((1.0f / sr) / tau));
        slope = 0.26f * reedStiffness - 0.44f;
    }

    void prepare (float sr)
    {
        sampleRate = sr;
        for (int i = 0; i < 65536; ++i)
            sine[i] = std::sin ((float) i * 6.28318548f / 65536.0f);
        g = std::tan (std::max (0.0f, std::min (3.14159274f * 2000.0f / sr, 1.5676547f)));
        gk = g + 1.4142135f;
        c1 = g * std::min (3.4028235e38f, 1.0f / (g * gk + 1.0f));
        update();
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
        const float PB = pb, LD = loopDelay, RT = ratio, GT = gt, VG = vg, TG = target, EP = envPole, SL = slope;
        const float F = freq, OG = outGain, G = g, GK = gk, C1 = c1;
        const int II = incInt, IF = incFrac;
        const float dmin = 1020.0f / sr, dk = 0.0014705883f * sr;
        const float* sn = sine;
        float* tm = toMouth;
        float* tb = toBell;
        float be = bendE, bxp = bendXp, ee = envE, exp_ = envXp, le = bellE, lxp = bellXp, gp = gatePrev;
        int ph = phase, phf = phaseFrac, lw = w;
        uint32_t sd = seed;
        float ls1 = s1, ls2 = s2, bi = boreIn, mo = mouth, bo = bellOut, bin = bellIn;
        for (int i = 0; i < n; ++i)
        {
            lw = (lw + 1) & 2047;
            // bell: the wave leaving the bore comes back through the smoother
            le = PB * (le + (lxp - bo));
            lxp = bo;
            tm[lw] = bo + le;
            // pitch: bend (si.polySmooth), vibrato (os.osc), envelope
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
            const float e = TG + ee;
            const float halfLength = 170.0f / F / (bendRatio * (1.0f + VG * vib * e));
            const float d = dk * std::max (dmin, halfLength - LD);
            // de.fdelay4 taps and fractional part
            const float dmo = d - 1.499995f;
            const float fl = std::floor (dmo);
            const int id = (int) std::min ((float) NMAX, std::max (-4.0f, dmo));
            int taps[5];
            for (int k = 0; k < 5; ++k)
                taps[k] = 1 + std::min (NMAX, std::max (0, id + k));
            const float fd = d - fl;
            const float fromBell = read4 (tm, lw, taps, fd);
            // blower: pressure + vibrato + breath noise
            sd = 12345u + 1103515245u * sd;
            const float nz = 0.0000000004656613f * (float) (int32_t) sd;
            const float t = nz - (GK * ls1 + ls2);
            const float v1 = ls1 + C1 * t;
            const float lp = ls2 + G * v1;
            ls1 += 2.0f * C1 * t;
            ls2 += 2.0f * G * v1;
            const float blow = e + VG * vib + 0.05f * e * lp;
            // mouthpiece: reed table on the pressure difference
            const float p = blow + bi;
            tb[lw] = blow - mo * std::min (1.0f, std::max (-1.0f, 0.7f - SL * mo));
            mo = p;
            bi = fromBell;
            // bell side: the toBell output reaches the bell one sample after it is read
            bo = bin;
            bin = read4 (tb, lw, taps, fd);
            const float o = OG * bo;
            o1[i] = o;
            o2[i] = o;
        }
        bendE = be; bendXp = bxp; envE = ee; envXp = exp_; bellE = le; bellXp = lxp; gatePrev = gp;
        phase = ph; phaseFrac = phf; w = lw;
        seed = sd;
        s1 = ls1; s2 = ls2; boreIn = bi; mouth = mo; bellOut = bo; bellIn = bin;
    }
};
using Dsp = Clarinet;
