// moog_vcf: dm.moog_vcf_demo, default branch: ve.moog_vcf(res^4, freq), four one-poles with a global
// feedback mk = -4 res^4, then the output gain; corner frequency and gain smoothed by si.smoo.
struct MoogVcf
{
    static constexpr int numInputs = 1, numOutputs = 1;

    float freq = 25.0f, res = 0.9f, outgain = 5.0f;   // parameters (piano key, 0..1, dB)
    float frT, mk, gT;                                // derived targets
    float c1, sp;                                     // 2 pi / SR, si.smoo pole
    float frE, frXp, gE, gXp;                         // si.smooth states (error and previous input)
    float y1, y2, y3, y4;                             // the four one-pole states

    void setFreq (float v) { freq = v; update(); }
    void setRes (float v) { res = v; update(); }
    void setOutgain (float v) { outgain = v; update(); }

    void update()
    {
        frT = 440.0f * std::pow (2.0f, 0.083333336f * (freq - 49.0f));   // ba.pianokey2hz
        const float r4 = res * res * res * res;
        mk = -4.0f * std::max (0.0f, std::min (r4, 0.999999f));          // mk > -4 for stability
        gT = std::pow (10.0f, 0.05f * outgain);                          // ba.db2linear
    }

    void prepare (float sr)
    {
        c1 = 6.2831853071795862f / sr;
        sp = 1.0f - 44.1f / sr;
        update();
    }

    void process (float* const* in, float* const* out, int n)
    {
        const float* x = in[0];
        float* y = out[0];
        float fe = frE, fxp = frXp, ge = gE, gxp = gXp;
        float s1 = y1, s2 = y2, s3 = y3, s4 = y4;
        const float ft = frT, gt = gT, k = mk, cc = c1, p0 = sp;
        for (int i = 0; i < n; ++i)
        {
            fe = p0 * (fe + (fxp - ft));
            fxp = ft;
            const float fr = ft + fe;
            const float a = cc * fr;     // 1 - p, as faust simplifies it
            const float p = 1.0f - a;
            s1 = p * s1 + a * (x[i] + k * s4);
            s2 = p * s2 + a * s1;
            s3 = p * s3 + a * s2;
            s4 = p * s4 + a * s3;
            ge = p0 * (ge + (gxp - gt));
            gxp = gt;
            y[i] = s4 * (gt + ge);
        }
        frE = fe; frXp = fxp; gE = ge; gXp = gxp;
        y1 = s1; y2 = s2; y3 = s3; y4 = s4;
    }
};
using Dsp = MoogVcf;
