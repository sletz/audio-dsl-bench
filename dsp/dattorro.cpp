// dattorro: dm.dattorro_rev_demo of Faust's demos.lib: re.dattorro_rev(0, ...) with every control smoothed by
// si.smoo, then the demo's dry/wet mix and output level. Fixed delay lengths; allpass_f(t, a) is a line of
// t + 1 samples; the bandwidth filter recurses on y[n-2].
struct Dattorro
{
    static constexpr int numInputs = 2, numOutputs = 2;

    float prefilter = 0.7f, inDiffusion1 = 0.625f, inDiffusion2 = 0.625f, fbDiffusion1 = 0.625f,
          fbDiffusion2 = 0.625f, decay = 0.7f, damping = 0.625f, drywet = 0.0f, level = -6.0f;   // parameters
    float dwp1, omdmp, ombw, gl;        // derived (control rate)
    float c;                            // si.smoo pole
    // si.smoo states: error e and previous input xp
    float eBw, xpBw, eId1, xpId1, eId2, xpId2, eDd1, xpDd1, eDd2, xpDd2;
    float eDec, xpDec, eDmp, xpDmp, eDw, xpDw, eG, xpG;
    float pf1, pf2;                     // bandwidth filter output, one and two samples ago
    float lp0, lp1;                     // damping lowpasses
    float o1p, o2p;                     // tank outputs of the previous sample
    float ap142[143], ap107[108], ap379[380], ap277[278];      // input diffusion allpasses
    float ap672[673], dl4453[4453], ap1800[1801], dl1800[1800]; // tank, block 0
    float ap908[909], dl4217[4217], ap2656[2657], dl2656[2656]; // tank, block 1
    int i142, i107, i379, i277, i672, i4453, i1800, j1800, i908, i4217, i2656, j2656;

    void setPrefilter (float v) { prefilter = v; update(); }
    void setInDiffusion1 (float v) { inDiffusion1 = v; }
    void setInDiffusion2 (float v) { inDiffusion2 = v; }
    void setFbDiffusion1 (float v) { fbDiffusion1 = v; }
    void setFbDiffusion2 (float v) { fbDiffusion2 = v; }
    void setDecay (float v) { decay = v; }
    void setDamping (float v) { damping = v; update(); }
    void setDrywet (float v) { drywet = v; update(); }
    void setLevel (float v) { level = v; update(); }

    void update()
    {
        dwp1 = drywet + 1.0f;
        omdmp = 1.0f - damping;
        ombw = 1.0f - prefilter;
        gl = std::pow (10.0f, 0.05f * level);
    }

    void prepare (float sr)
    {
        c = 1.0f - 44.1f / sr;
        update();
    }

    // read-before-write delay line position: one step, wrapping at L
    template <int L>
    static inline void step (int& i)
    {
        if (++i == L)
            i = 0;
    }

    void process (float* const* in, float* const* out, int n)
    {
        const float* in0 = in[0];
        const float* in1 = in[1];
        float* out0 = out[0];
        float* out1 = out[1];
        const float C = c, DWP1 = dwp1, OMDMP = omdmp, OMBW = ombw, GL = gl;
        const float BW = prefilter, ID1 = inDiffusion1, ID2 = inDiffusion2, DD1 = fbDiffusion1, DD2 = fbDiffusion2;
        const float DEC = decay, DMP = damping, DW = drywet;
        float ebw = eBw, xbw = xpBw, eid1 = eId1, xid1 = xpId1, eid2 = eId2, xid2 = xpId2;
        float edd1 = eDd1, xdd1 = xpDd1, edd2 = eDd2, xdd2 = xpDd2, edec = eDec, xdec = xpDec;
        float edmp = eDmp, xdmp = xpDmp, edw = eDw, xdw = xpDw, eg = eG, xg = xpG;
        float lpf1 = pf1, lpf2 = pf2, l0 = lp0, l1 = lp1, p1 = o1p, p2 = o2p;
        int k142 = i142, k107 = i107, k379 = i379, k277 = i277, k672 = i672, k4453 = i4453, k1800 = i1800,
            m1800 = j1800, k908 = i908, k4217 = i4217, k2656 = i2656, m2656 = j2656;
        for (int i = 0; i < n; ++i)
        {
            const float x0 = in0[i];
            edw = C * ((xdw - DW) + edw);  xdw = DW;
            const float dw = DWP1 + edw;
            const float dry = 1.0f - 0.5f * dw;
            edd1 = C * ((xdd1 - DD1) + edd1);  xdd1 = DD1;
            const float dd1 = DD1 + edd1;
            // tank block 1: allpass(908, -dd1), delay 4217, damping, allpass(2656, dd2), delay 2656
            const float r1 = ap908[k908];
            const float w1 = r1 * dd1 + p2;
            ap908[k908] = w1;  step<909> (k908);
            edmp = C * ((xdmp - DMP) + edmp);  xdmp = DMP;
            const float dmp = DMP + edmp;
            const float y4217 = dl4217[k4217];  dl4217[k4217] = r1 + -(dd1 * w1);  step<4217> (k4217);
            const float omd = OMDMP - edmp;
            l1 = l1 * dmp + y4217 * omd;
            edec = C * ((xdec - DEC) + edec);  xdec = DEC;
            const float dec = DEC + edec;
            edd2 = C * ((xdd2 - DD2) + edd2);  xdd2 = DD2;
            const float dd2 = DD2 + edd2;
            const float r4 = ap2656[k2656];
            const float w4 = l1 * dec - r4 * dd2;
            ap2656[k2656] = w4;  step<2657> (k2656);
            eid2 = C * ((xid2 - ID2) + eid2);  xid2 = ID2;
            const float id2 = ID2 + eid2;
            // input: bandwidth filter, then 4 diffusion allpasses
            ebw = C * ((xbw - BW) + ebw);  xbw = BW;
            const float x1 = in1[i];
            const float pf = lpf2 * (OMBW - ebw) + 0.5f * (x0 + x1) * (BW + ebw);
            lpf2 = lpf1;
            lpf1 = pf;
            eid1 = C * ((xid1 - ID1) + eid1);  xid1 = ID1;
            const float id1 = ID1 + eid1;
            const float r20 = ap142[k142];
            const float t13 = pf - id1 * r20;
            ap142[k142] = t13;  step<143> (k142);
            const float r19 = ap107[k107];
            const float t14 = r20 - id1 * (r19 - t13);
            ap107[k107] = t14;  step<108> (k107);
            const float r18 = ap379[k379];
            const float t15 = r19 + id1 * t14 - id2 * r18;
            ap379[k379] = t15;  step<380> (k379);
            const float r24 = ap277[k277];
            const float t16 = r18 - id2 * (r24 - t15);
            ap277[k277] = t16;  step<278> (k277);
            const float t17 = id2 * t16;
            const float y2656 = dl2656[m2656];  dl2656[m2656] = r4 + dd2 * w4;  step<2656> (m2656);
            const float o1 = t17 + dec * y2656 + r24;
            // tank block 0: allpass(672, -dd1), delay 4453, damping, allpass(1800, dd2), delay 1800
            const float r7 = ap672[k672];
            const float w7 = dd1 * r7 + p1;
            ap672[k672] = w7;  step<673> (k672);
            const float y4453 = dl4453[k4453];  dl4453[k4453] = r7 + -(dd1 * w7);  step<4453> (k4453);
            l0 = dmp * l0 + omd * y4453;
            const float r10 = ap1800[k1800];
            const float w10 = dec * l0 - dd2 * r10;
            ap1800[k1800] = w10;  step<1801> (k1800);
            const float y1800 = dl1800[m1800];  dl1800[m1800] = r10 + dd2 * w10;  step<1800> (m1800);
            const float o2 = r24 + t17 + dec * y1800;
            eg = C * ((xg - GL) + eg);  xg = GL;
            const float g = GL + eg;
            out0[i] = (x0 * dry + 0.5f * dw * o1) * g;
            out1[i] = g * (x1 * dry + 0.5f * dw * o2);
            p1 = o1;
            p2 = o2;
        }
        eBw = ebw; xpBw = xbw; eId1 = eid1; xpId1 = xid1; eId2 = eid2; xpId2 = xid2;
        eDd1 = edd1; xpDd1 = xdd1; eDd2 = edd2; xpDd2 = xdd2; eDec = edec; xpDec = xdec;
        eDmp = edmp; xpDmp = xdmp; eDw = edw; xpDw = xdw; eG = eg; xpG = xg;
        pf1 = lpf1; pf2 = lpf2; lp0 = l0; lp1 = l1; o1p = p1; o2p = p2;
        i142 = k142; i107 = k107; i379 = k379; i277 = k277; i672 = k672; i4453 = k4453; i1800 = k1800;
        j1800 = m1800; i908 = k908; i4217 = k4217; i2656 = k2656; j2656 = m2656;
    }
};
using Dsp = Dattorro;
