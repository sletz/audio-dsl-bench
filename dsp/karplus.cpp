// karplus: pm.ks_ui_MIDI of Faust's physmodels.lib, gate held at 1 (plucked once at the first sample):
// a waveguide of two de.fdelay4 Lagrange-interpolated delay lines, rigid termination on one side and the
// averaging reflection filter scaled by the damping on the other.
struct Karplus
{
    static constexpr int numInputs = 0, numOutputs = 2;

    float freq = 440.0f, bend = 0.0f, gain = 0.800000012f, sustain = 0.0f,
          damping = 0.00999999978f, gate = 1.0f;     // parameters
    float len, ratio, gt, dampC;            // derived (control rate)
    float c1, c2, c4, smoo;                 // sample-rate constants
    int nmax;                               // int(c4): the integer taps clamped by c4

    float bendE, bendXp, dampE, dampXp;     // si.smooth states (error form)
    float gatePrev;
    float toEnd[2048];                      // wave going to the reflection filter
    float toStart[2048];                    // wave coming back from it
    int w;                                  // write position of both lines
    float r2;                               // toEnd output, one sample late
    float r3a, r3b;                         // excitation + r2, one and two samples late
    float r4a, r4b;                         // toStart output, one and two samples late

    void setFreq (float v) { freq = v; update(); }
    void setBend (float v) { bend = v; update(); }
    void setGain (float v) { gain = v; }
    void setSustain (float v) { sustain = v; update(); }
    void setDamping (float v) { damping = v; update(); }
    void setGate (float v) { gate = v; update(); }

    void update()
    {
        len = 340.0f / freq;                // pm.f2l
        ratio = std::pow (2.0f, 0.083333336f * bend);
        gt = std::min (1.0f, gate + sustain);
        dampC = 1.0f - damping;
    }

    void prepare (float sr)
    {
        c1 = 0.0014705883f * sr;            // SR / (2 * pm.speedOfSound): half of pm.l2s
        c2 = 2210.0f / sr;                  // 6.5 samples of loop delay, in meters
        c4 = 0.00882353f * sr + 4.0f;       // the de.fdelay4 bound: l2s(pm.maxLength) + 4
        smoo = 1.0f - 44.1f / sr;           // si.smoo
        nmax = (int) c4;
        update();
    }

    void process (float* const*, float* const* out, int n)
    {
        float* o1 = out[0];
        float* o2 = out[1];
        const float rt = ratio, g = gt, dc = dampC, dv = damping, gn = gain, L = len;
        const float C1 = c1, C2 = c2, C4 = c4, SM = smoo;
        const int NM = nmax;
        float be = bendE, bxp = bendXp, de = dampE, dxp = dampXp, gp = gatePrev;
        float lr2 = r2, l3a = r3a, l3b = r3b, l4a = r4a, l4b = r4b;
        int lw = w;
        float* te = toEnd;
        float* ts = toStart;
        for (int i = 0; i < n; ++i)
        {
            lw = (lw + 1) & 2047;
            // pitch bend (si.polySmooth)
            float hold = 0.0f;
            if (g == gp || g == 0.0f)
                hold = 0.999f;
            be = hold * (be + (bxp - rt));
            bxp = rt;
            const float bendRatio = rt + be;
            // delay of each line, de.fdelay4 fractional part and Lagrange products
            const float d = C1 * (L / bendRatio - C2);
            const float dmo = d - 1.499995f;
            const float fl = std::floor (dmo);
            const float t3 = d - fl;
            const float t4 = -1.0f - fl + d;
            const float t5 = t3 * t4;
            const float t6 = -2.0f - fl + d;
            const float t7 = t5 * t6;
            const float t8 = -3.0f - fl + d;
            const float t9 = t7 * t8;
            // excitation: pm.impulseExcitation(gate) * gain
            const float dg = g - gp;
            gp = g;
            float exc = 0.0f;
            if (dg > 0.0f)
                exc = gn * dg;
            // reflection: pm.ksReflexionFilter * ((1 - damping) * 0.2 + 0.8)
            de = SM * (de + (dxp - dv));
            dxp = dv;
            const float t12 = 0.5f * (l4a + l4b) * (0.8f + 0.2f * (dc - de));
            te[lw] = t12;
            const float t13 = -4.0f - fl + d;
            const int id = (int) std::min (C4, std::max (-4.0f, dmo));
            const int k0 = 1 + std::min (NM, std::max (0, id));
            const int k1 = 1 + std::min (NM, std::max (0, id + 1));
            const int k2 = 1 + std::min (NM, std::max (0, id + 2));
            const int k3 = 1 + std::min (NM, std::max (0, id + 3));
            const int k4 = 1 + std::min (NM, std::max (0, id + 4));
            const float x0 = te[(lw - k0) & 2047];
            const float x1 = te[(lw - k1) & 2047];
            const float x2 = te[(lw - k2) & 2047];
            const float x3 = te[(lw - k3) & 2047];
            const float x4 = te[(lw - k4) & 2047];
            const float r2n = t13 * (t8 * (t6 * (0.041666668f * x0 * t4 - 0.16666667f * t3 * x1) + 0.25f * t5 * x2)
                                     - 0.16666667f * t7 * x3)
                              + 0.041666668f * t9 * x4;
            // rigid termination: the excitation enters both directions
            const float r3n = exc + lr2;
            ts[lw] = exc + l3b;
            const float y0 = ts[(lw - k0) & 2047];
            const float y1 = ts[(lw - k1) & 2047];
            const float y2 = ts[(lw - k2) & 2047];
            const float y3 = ts[(lw - k3) & 2047];
            const float y4 = ts[(lw - k4) & 2047];
            const float t21 = t13 * (t8 * (t6 * (0.041666668f * t4 * y0 - 0.16666667f * t3 * y1) + 0.25f * t5 * y2)
                                     - 0.16666667f * t7 * y3);
            const float t22 = 0.041666668f * t9 * y4;
            const float o = t22 + t12 + t21;
            o1[i] = o;
            o2[i] = o;
            lr2 = r2n;
            l3b = l3a;
            l3a = r3n;
            l4b = l4a;
            l4a = t21 + t22;
        }
        bendE = be; bendXp = bxp; dampE = de; dampXp = dxp; gatePrev = gp;
        r2 = lr2; r3a = l3a; r3b = l3b; r4a = l4a; r4b = l4b;
        w = lw;
    }
};
using Dsp = Karplus;
