// autopan: examples/misc/autopan.dsp with Amount at 1. Two os.oscp LFOs (os.osc's 65536-point sine and
// cosine tables, fixed-point phasor; the right one offset by the smoothed phase), each through a tanh
// saturator (interpolated by the smoothed shape) and it.remap(-1, 1, 1 - amount, 1), as channel gains.
struct Autopan
{
    static constexpr int numInputs = 2, numOutputs = 2;

    float amount = 1.0f, rate = 1.0f, phase = 180.0f, shape = 0.0f;   // parameters
    float sampleRate, pSmoo;                                          // derived
    int incInt, incFrac;
    float sine[65536];          // os.oscsin's table, sin(2 pi i / 65536)
    float cosine[65536];        // os.osccos's table, cos(2 pi i / 65536)
    int ph, phFrac;             // os.oscp phasor: 30-bit integer part and 24-bit fraction
    float shapePrev, shapeErr;  // si.smoo on shape
    float phasePrev, phaseErr;  // si.smoo on phase

    void setAmount (float v) { amount = v; }
    void setRate (float v) { rate = v; update(); }
    void setPhase (float v) { phase = v; }
    void setShape (float v) { shape = v; }

    void update()
    {
        const float f = 1.0f / sampleRate * rate;
        const float inc = 1073741824.0f * (f - std::rint (f));   // 2^30 * f / SR, wrapped
        const float incFloor = std::floor (inc);
        incInt = int (incFloor);
        incFrac = int (16777216.0f * (inc - incFloor));
        pSmoo = 1.0f - 44.1f / sampleRate;                        // si.smoo pole
    }

    void prepare (float sr)
    {
        sampleRate = sr;
        for (int i = 0; i < 65536; ++i)
        {
            sine[i] = std::sin (float (i) * 6.2831853071795862f / 65536.0f);
            cosine[i] = std::cos (float (i) * 6.2831853071795862f / 65536.0f);
        }
        update();
    }

    void process (float* const* in, float* const* out, int n)
    {
        const float* xl = in[0];
        const float* xr = in[1];
        float* yl = out[0];
        float* yr = out[1];
        int p0 = ph, pf = phFrac;
        float shp = shapePrev, she = shapeErr, php = phasePrev, phe = phaseErr;
        const int ii = incInt, fi = incFrac;
        const float ps = pSmoo, am = amount, shv = shape, phv = phase;
        for (int i = 0; i < n; ++i)
        {
            const int k = ((p0 >> 6) >> 8) & 65535;
            const float s = sine[k];
            pf += fi;
            p0 = (ii + p0 + (pf >> 24)) & 1073741823;
            pf &= 16777215;
            // si.smooth: y = x + e, e = pole * (e + x[n-1] - x); min(ma.MAX) is Faust's no-op
            she = ps * (std::min (3.4028235e38f, shp - shv) + she);
            shp = shv;
            const float sh = shv + she;
            yl[i] = xl[i] * (1.0f + am * (-1.0f + 0.5f * (1.0f + s + sh * (std::tanh (10.0f * s) - s))));
            phe = ps * (std::min (3.4028235e38f, php - phv) + phe);
            php = phv;
            const float p = 0.017453292f * (phv + phe);              // ma.deg2rad
            const float r = s * std::cos (p) + cosine[k] * std::sin (p);   // oscp(rate, p)
            yr[i] = xr[i] * (1.0f + am * (-1.0f + 0.5f * (1.0f + r + sh * (std::tanh (10.0f * r) - r))));
        }
        ph = p0; phFrac = pf;
        shapePrev = shp; shapeErr = she; phasePrev = php; phaseErr = phe;
    }
};
using Dsp = Autopan;
