// distortion: dm.cubicnl_demo without the bypass (ef.cubicnl_nodc with si.smoo on drive and offset):
// pregain 10^(2*drive), offset, clip to [-1, 1], cubic soft clipper x - x^3/3, then fi.dcblocker.
struct Distortion
{
    static constexpr int numInputs = 1, numOutputs = 1;

    float drive = 0.0f, offset = 0.0f;     // parameters
    float pSmoo, pDc;                      // derived (sample-rate constants)
    float drivePrev, driveErr, offsetPrev, offsetErr, xPrev, y;   // state

    void setDrive (float v) { drive = v; }
    void setOffset (float v) { offset = v; }

    void prepare (float sr)
    {
        pSmoo = 1.0f - 44.1f / sr;                 // si.smoo pole
        pDc = std::pow (0.995f, 44100.0f / sr);    // fi.dcblocker pole
    }

    void process (float* const* in, float* const* out, int n)
    {
        const float* x0 = in[0];
        float* o = out[0];
        const float d = drive, off = offset, ps = pSmoo, pd = pDc;
        float dp = drivePrev, de = driveErr, op = offsetPrev, oe = offsetErr, xp = xPrev, yy = y;
        for (int i = 0; i < n; ++i)
        {
            // si.smooth: y = x + e, e = pole * (e + x[n-1] - x); min(ma.MAX) is Faust's no-op
            de = ps * (std::min (3.4028235e38f, dp - d) + de);
            dp = d;
            oe = ps * (std::min (3.4028235e38f, op - off) + oe);
            op = off;
            const float x = std::max (-1.0f, std::min (1.0f, off + x0[i] * std::pow (10.0f, 2.0f * (d + de)) + oe));
            const float c = x * (1.0f - 0.33333334f * (x * x));
            yy = pd * yy + c - xp;
            xp = c;
            o[i] = yy;
        }
        drivePrev = dp; driveErr = de; offsetPrev = op; offsetErr = oe; xPrev = xp; y = yy;
    }
};
using Dsp = Distortion;
