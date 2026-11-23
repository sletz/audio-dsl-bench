// virtual_analog: dm.virtual_analog_oscillator_demo of Faust's demos.lib, controls at their defaults: three
// detuned voices of saw (os.sawN(2)), square, pulse train and triangle, plus pink noise and the input, mixed.
// Phases are the fixed-point _phasor_fix of oscillators.lib, reset to 0 at the first sample.
struct VirtualAnalog
{
    static constexpr int numInputs = 1, numOutputs = 1;

    float ssaw = 1.0f, spt = 0.0f, duty = 0.5f, ssq = 0.0f, stri = 0.0f, spn = 0.0f, sei = 0.0f;
    float mixdb = -20.0f, freq = 49.0f, detune1 = -0.1f, detune2 = 0.1f, portamento = 0.1f;  // parameters
    float ampT, frT, porP, dt1, dt2, triG, ptG, dutyC;      // derived (control rate)
    float c1, c2, c3, c4, c5, c6, c7, sr;                   // sample-rate constants

    float aE, aXp, fE, fXp, dE, dXp;        // si.smooth states (error, previous input)
    float gate;                             // 1@1 of sawN (0 at the first sample)
    int pm, sm;                             // phase masks, 0 at the first sample (hard reset)
    int pa[3], sa[3];                       // saw phasors (counts, fraction)
    int pb[3], sb[3];                       // square / pulse phasors (frequency clipped at 23.45 Hz)
    float va[3], vb[3];                     // previous squared ramps
    float d1[16384], d2[16384], d3[16384];
    int w;
    float tr1, tr2, tr3;                    // triangle integrators
    uint32_t seed;                          // no.noise
    float k1, k2, k3;                       // pink filter (fi.iir) states

    void setPulseTrain (float v) { spt = v; update(); }
    void setDuty (float v) { duty = v; update(); }
    void setTriangle (float v) { stri = v; update(); }
    void setMixDb (float v) { mixdb = v; update(); }
    void setFreq (float v) { freq = v; update(); }
    void setDetune1 (float v) { detune1 = v; update(); }
    void setDetune2 (float v) { detune2 = v; update(); }
    void setPortamento (float v) { portamento = v; update(); }

    void update()
    {
        ampT = std::pow (10.0f, 0.05f * mixdb);                         // ba.db2linear
        frT = 440.0f * std::pow (2.0f, 0.083333336f * (freq - 49.0f));  // ba.pianokey2hz
        porP = std::exp (-(c3 / portamento));                           // ba.tau2pole
        dt1 = 0.01f * detune1 + 1.0f;
        dt2 = 0.01f * detune2 + 1.0f;
        triG = c5 * stri;
        ptG = c2 * spt;
        dutyC = 1.0f - duty;
    }

    void prepare (float s)
    {
        sr = s;
        c1 = 1.0f - 44.1f / sr;             // si.smoo pole
        c2 = 0.083333336f * sr;             // SR / 12: sawN(2) scale SR / (4 f) times amp / 3
        c3 = 1.0f / sr;
        c4 = 0.5f * sr;                     // square delay (1 - 0.5) SR
        c5 = 1.3333334f / sr;               // triangle gain 4 f / SR times 1 / 3
        c6 = 0.25f * sr;                    // sawN(2) scale SR / 4 inside the triangle integrator
        c7 = 1.0f - 48.0f / sr;             // triangle leak
        update();
    }

    // _phasor_fix and the square of the DPW ramp of sawN(2): returns x^2 - x'^2, x = 2 phi - 1
    static inline float saw2 (int& p, int& s, float& vp, float inc, int pmask, int smask)
    {
        const float xc = 1073741824.0f * (inc - std::round (inc));  // increment in counts
        const float fl = std::floor (xc);
        const int t = s + (int) (16777216.0f * (xc - fl));          // fraction sum, carry in bit 24
        s = t & smask;
        p = (p + (int) fl + (t >> 24)) & pmask;
        const float x = -1.0f + 1.1920929e-07f * (float) (p >> 6);
        const float v = x * x;
        const float d = v - vp;
        vp = v;
        return d;
    }

    // pulsetrainN's diffdel: x - x@int(del) (1 - frac(del)) - x@(int(del) + 1) frac(del)
    static inline float diffdel (const float* buf, int w, float x, float del)
    {
        const int id = (int) del;
        const float fl = std::floor (del);
        return x - buf[(w - id) & 16383] * (1.0f - del + fl) - (del - fl) * buf[(w - id - 1) & 16383];
    }

    void process (float* const* in, float* const* out, int n)
    {
        const float* i0 = in[0];
        float* o0 = out[0];
        const float A = ampT, F = frT, PP = porP, DT1 = dt1, DT2 = dt2, TG = triG, PG = ptG, DC = dutyC;
        const float SSAW = ssaw, SSQ = ssq, SPN = spn, SEI = sei, DU = duty;
        const float C1 = c1, C2 = c2, C3 = c3, C4 = c4, C6 = c6, C7 = c7, SR = sr;
        float ae = aE, axp = aXp, fe = fE, fxp = fXp, de = dE, dxp = dXp, gt = gate;
        int lpm = pm, lsm = sm;
        int pa1 = pa[0], pa2 = pa[1], pa3 = pa[2], sa1 = sa[0], sa2 = sa[1], sa3 = sa[2];
        int pb1 = pb[0], pb2 = pb[1], pb3 = pb[2], sb1 = sb[0], sb2 = sb[1], sb3 = sb[2];
        float va1 = va[0], va2 = va[1], va3 = va[2], vb1 = vb[0], vb2 = vb[1], vb3 = vb[2];
        float lt1 = tr1, lt2 = tr2, lt3 = tr3, l1 = k1, l2 = k2, l3 = k3;
        uint32_t sd = seed;
        int lw = w;
        float* b1 = d1;
        float* b2 = d2;
        float* b3 = d3;
        for (int i = 0; i < n; ++i)
        {
            ae = C1 * ((axp - A) + ae);
            axp = A;
            fe = PP * ((fxp - F) + fe);
            fxp = F;
            const float f1 = F + fe;            // smoothed frequency, voice 1
            const float f2 = DT1 * f1;
            const float f3 = DT2 * f1;
            // saws: sawN(2) (x^2 - x'^2) / f, scaled by SR / 4 below
            const float fa1 = std::max (20.0f, std::fabs (f1));
            const float fa2 = std::max (20.0f, std::fabs (f2));
            const float fa3 = std::max (20.0f, std::fabs (f3));
            const float q1 = saw2 (pa1, sa1, va1, C3 * fa1, lpm, lsm);
            const float q2 = saw2 (pa2, sa2, va2, C3 * fa2, lpm, lsm);
            const float q3 = saw2 (pa3, sa3, va3, C3 * fa3, lpm, lsm);
            // square / pulse saws at max(f, 23.45 Hz), written to their delay lines
            const float fc1 = std::max (f1, 23.44895f);
            const float fc2 = std::max (f2, 23.44895f);
            const float fc3 = std::max (f3, 23.44895f);
            const float fb1 = std::max (20.0f, std::fabs (fc1));
            const float fb2 = std::max (20.0f, std::fabs (fc2));
            const float fb3 = std::max (20.0f, std::fabs (fc3));
            const float x1 = gt * saw2 (pb1, sb1, vb1, C3 * fb1, lpm, lsm) / fb1;
            const float x2 = gt * saw2 (pb2, sb2, vb2, C3 * fb2, lpm, lsm) / fb2;
            const float x3 = gt * saw2 (pb3, sb3, vb3, C3 * fb3, lpm, lsm) / fb3;
            b1[lw] = x1;
            b2[lw] = x2;
            b3[lw] = x3;
            const float sq1 = diffdel (b1, lw, x1, std::max (0.0f, std::min (8191.0f, C4 / fc1)));
            const float sq2 = diffdel (b2, lw, x2, std::max (0.0f, std::min (8191.0f, C4 / fc2)));
            const float sq3 = diffdel (b3, lw, x3, std::max (0.0f, std::min (8191.0f, C4 / fc3)));
            lt1 = C6 * sq1 + C7 * lt1;
            lt2 = C6 * sq2 + C7 * lt2;
            lt3 = C6 * sq3 + C7 * lt3;
            // pulse trains: delay (1 - duty) SR / f, duty through si.smooth(0.99)
            de = 0.99f * ((dxp - DU) + de);
            dxp = DU;
            const float dd = DC - de;
            const float pt1 = diffdel (b1, lw, x1, std::max (0.0f, std::min (8191.0f, SR * (dd / fc1))));
            const float pt2 = diffdel (b2, lw, x2, std::max (0.0f, std::min (8191.0f, SR * (dd / fc2))));
            const float pt3 = diffdel (b3, lw, x3, std::max (0.0f, std::min (8191.0f, SR * (dd / fc3))));
            // pink noise: no.noise through no.pink_filter
            sd = sd * 1103515245u + 12345u;
            const float k0 = 0.5221894f * l3 + 4.656613e-10f * (float) (int32_t) sd + 2.494956f * l1
                             - 2.0172658f * l2;
            const float pink = 0.049922034f * k0 + 0.0506127f * l2 - (0.095993534f * l1 + 0.004408786f * l3);
            l3 = l2;
            l2 = l1;
            l1 = k0;
            const float saws = SSAW * gt * (q1 / fa1 + q2 / fa2 + q3 / fa3);
            const float sqs = SSQ * (sq1 + sq2 + sq3);
            const float tris = TG * f1 * (lt1 + DT1 * lt2 + DT2 * lt3);
            const float pts = PG * (pt1 + pt2 + pt3);
            o0[i] = SEI * i0[i] + (A + ae) * (C2 * (saws + sqs) + tris + pts + SPN * pink);
            gt = 1.0f;
            lpm = 1073741823;
            lsm = 16777215;
            lw = (lw + 1) & 16383;
        }
        aE = ae; aXp = axp; fE = fe; fXp = fxp; dE = de; dXp = dxp; gate = gt;
        pm = lpm; sm = lsm;
        pa[0] = pa1; pa[1] = pa2; pa[2] = pa3; sa[0] = sa1; sa[1] = sa2; sa[2] = sa3;
        pb[0] = pb1; pb[1] = pb2; pb[2] = pb3; sb[0] = sb1; sb[1] = sb2; sb[2] = sb3;
        va[0] = va1; va[1] = va2; va[2] = va3; vb[0] = vb1; vb[1] = vb2; vb[2] = vb3;
        tr1 = lt1; tr2 = lt2; tr3 = lt3; k1 = l1; k2 = l2; k3 = l3;
        seed = sd;
        w = lw;
    }
};
using Dsp = VirtualAnalog;
