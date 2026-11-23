// greyhole: dm.greyhole_demo, re.greyhole(dt, damp, size, early_diff, feedback, mod_depth, mod_freq) with
// the demo's defaults. 3 nested diffusers of 4 levels (24 rotations feeding de.fdelay1a allpasses whose
// lengths are primes chosen by size, smoothed by smooth_init), si.smooth(damp), and a feedback path of
// de.fdelay4 modulated by a coupled-form oscillator, de.sdelay(65536, 22050, dt*SR) and the feedback gain.
// Every control goes through linear_interp(x) = (x + x')/2 per sample, as in faust. Constants for 48 kHz
// like faust's (0.00056689343*SR, PI/SR). Each 16384-sample allpass line is padded by 16 floats (one cache
// line): 64 KB apart, the 24 lines would all fall in the same L1 cache sets.
struct Greyhole
{
    static constexpr int numInputs = 2, numOutputs = 2;

    // ma.primes, cut to its first 400 entries (size <= 3 reads at most index 321)
    static constexpr float primes[400] = {
        2.0f, 3.0f, 5.0f, 7.0f, 11.0f, 13.0f, 17.0f, 19.0f, 23.0f, 29.0f, 31.0f, 37.0f,
        41.0f, 43.0f, 47.0f, 53.0f, 59.0f, 61.0f, 67.0f, 71.0f, 73.0f, 79.0f, 83.0f, 89.0f,
        97.0f, 101.0f, 103.0f, 107.0f, 109.0f, 113.0f, 127.0f, 131.0f, 137.0f, 139.0f, 149.0f, 151.0f,
        157.0f, 163.0f, 167.0f, 173.0f, 179.0f, 181.0f, 191.0f, 193.0f, 197.0f, 199.0f, 211.0f, 223.0f,
        227.0f, 229.0f, 233.0f, 239.0f, 241.0f, 251.0f, 257.0f, 263.0f, 269.0f, 271.0f, 277.0f, 281.0f,
        283.0f, 293.0f, 307.0f, 311.0f, 313.0f, 317.0f, 331.0f, 337.0f, 347.0f, 349.0f, 353.0f, 359.0f,
        367.0f, 373.0f, 379.0f, 383.0f, 389.0f, 397.0f, 401.0f, 409.0f, 419.0f, 421.0f, 431.0f, 433.0f,
        439.0f, 443.0f, 449.0f, 457.0f, 461.0f, 463.0f, 467.0f, 479.0f, 487.0f, 491.0f, 499.0f, 503.0f,
        509.0f, 521.0f, 523.0f, 541.0f, 547.0f, 557.0f, 563.0f, 569.0f, 571.0f, 577.0f, 587.0f, 593.0f,
        599.0f, 601.0f, 607.0f, 613.0f, 617.0f, 619.0f, 631.0f, 641.0f, 643.0f, 647.0f, 653.0f, 659.0f,
        661.0f, 673.0f, 677.0f, 683.0f, 691.0f, 701.0f, 709.0f, 719.0f, 727.0f, 733.0f, 739.0f, 743.0f,
        751.0f, 757.0f, 761.0f, 769.0f, 773.0f, 787.0f, 797.0f, 809.0f, 811.0f, 821.0f, 823.0f, 827.0f,
        829.0f, 839.0f, 853.0f, 857.0f, 859.0f, 863.0f, 877.0f, 881.0f, 883.0f, 887.0f, 907.0f, 911.0f,
        919.0f, 929.0f, 937.0f, 941.0f, 947.0f, 953.0f, 967.0f, 971.0f, 977.0f, 983.0f, 991.0f, 997.0f,
        1009.0f, 1013.0f, 1019.0f, 1021.0f, 1031.0f, 1033.0f, 1039.0f, 1049.0f, 1051.0f, 1061.0f, 1063.0f, 1069.0f,
        1087.0f, 1091.0f, 1093.0f, 1097.0f, 1103.0f, 1109.0f, 1117.0f, 1123.0f, 1129.0f, 1151.0f, 1153.0f, 1163.0f,
        1171.0f, 1181.0f, 1187.0f, 1193.0f, 1201.0f, 1213.0f, 1217.0f, 1223.0f, 1229.0f, 1231.0f, 1237.0f, 1249.0f,
        1259.0f, 1277.0f, 1279.0f, 1283.0f, 1289.0f, 1291.0f, 1297.0f, 1301.0f, 1303.0f, 1307.0f, 1319.0f, 1321.0f,
        1327.0f, 1361.0f, 1367.0f, 1373.0f, 1381.0f, 1399.0f, 1409.0f, 1423.0f, 1427.0f, 1429.0f, 1433.0f, 1439.0f,
        1447.0f, 1451.0f, 1453.0f, 1459.0f, 1471.0f, 1481.0f, 1483.0f, 1487.0f, 1489.0f, 1493.0f, 1499.0f, 1511.0f,
        1523.0f, 1531.0f, 1543.0f, 1549.0f, 1553.0f, 1559.0f, 1567.0f, 1571.0f, 1579.0f, 1583.0f, 1597.0f, 1601.0f,
        1607.0f, 1609.0f, 1613.0f, 1619.0f, 1621.0f, 1627.0f, 1637.0f, 1657.0f, 1663.0f, 1667.0f, 1669.0f, 1693.0f,
        1697.0f, 1699.0f, 1709.0f, 1721.0f, 1723.0f, 1733.0f, 1741.0f, 1747.0f, 1753.0f, 1759.0f, 1777.0f, 1783.0f,
        1787.0f, 1789.0f, 1801.0f, 1811.0f, 1823.0f, 1831.0f, 1847.0f, 1861.0f, 1867.0f, 1871.0f, 1873.0f, 1877.0f,
        1879.0f, 1889.0f, 1901.0f, 1907.0f, 1913.0f, 1931.0f, 1933.0f, 1949.0f, 1951.0f, 1973.0f, 1979.0f, 1987.0f,
        1993.0f, 1997.0f, 1999.0f, 2003.0f, 2011.0f, 2017.0f, 2027.0f, 2029.0f, 2039.0f, 2053.0f, 2063.0f, 2069.0f,
        2081.0f, 2083.0f, 2087.0f, 2089.0f, 2099.0f, 2111.0f, 2113.0f, 2129.0f, 2131.0f, 2137.0f, 2141.0f, 2143.0f,
        2153.0f, 2161.0f, 2179.0f, 2203.0f, 2207.0f, 2213.0f, 2221.0f, 2237.0f, 2239.0f, 2243.0f, 2251.0f, 2267.0f,
        2269.0f, 2273.0f, 2281.0f, 2287.0f, 2293.0f, 2297.0f, 2309.0f, 2311.0f, 2333.0f, 2339.0f, 2341.0f, 2347.0f,
        2351.0f, 2357.0f, 2371.0f, 2377.0f, 2381.0f, 2383.0f, 2389.0f, 2393.0f, 2399.0f, 2411.0f, 2417.0f, 2423.0f,
        2437.0f, 2441.0f, 2447.0f, 2459.0f, 2467.0f, 2473.0f, 2477.0f, 2503.0f, 2521.0f, 2531.0f, 2539.0f, 2543.0f,
        2549.0f, 2551.0f, 2557.0f, 2579.0f, 2591.0f, 2593.0f, 2609.0f, 2617.0f, 2621.0f, 2633.0f, 2647.0f, 2657.0f,
        2659.0f, 2663.0f, 2671.0f, 2677.0f, 2683.0f, 2687.0f, 2689.0f, 2693.0f, 2699.0f, 2707.0f, 2711.0f, 2713.0f,
        2719.0f, 2729.0f, 2731.0f, 2741.0f
    };
    // index factor into the prime table of each allpass (re.greyhole's diffuser constants)
    static constexpr int primeIndex[24] = { 59, 49, 36, 46, 23, 33, 10, 20, 78, 68, 55, 65,
                                            42, 52, 29, 39, 97, 87, 74, 84, 61, 71, 48, 58 };

    float delayTime = 0.2f, damping = 0.0f, size = 1.0f, diffusion = 0.5f;   // parameters
    float feedback = 0.9f, modDepth = 0.1f, modFreq = 2.0f;
    float sampleRate, c1, c2, dts;          // derived: 0.00056689343*SR, PI/SR, floor(min(65533, SR*dt))
    float prime[24], sl[24];                // allpass lengths and 0.0001*prime (smooth_init input)

    float diffPrev, freqPrev, depthPrev, fbPrev, dampPrev;   // x' of linear_interp
    float imp = 1.0f;                       // impulse: 1 at the first sample
    float oscS, oscC;                       // fi.nlf2 oscillator
    float sdv, sdi, sd0, sd1;               // de.sdelay control: step, crossfade, delays
    float smL, spL, smR, spR;               // si.smooth error and previous input
    float y[24], xp[24], r[24];             // allpass smoothed lengths, previous delayed input, output
    int w;                                  // write position of every line
    float lineL[1024], lineR[1024];         // outputs, read back by de.fdelay4
    float sdL[65536], sdR[65536];           // de.sdelay lines
    float ap[24][16384 + 16];               // allpass lines, padded

    void setDelayTime (float v) { delayTime = v; update(); }
    void setDamping (float v) { damping = v; }
    void setSize (float v) { size = v; update(); }
    void setDiffusion (float v) { diffusion = v; }
    void setFeedback (float v) { feedback = v; }
    void setModDepth (float v) { modDepth = v; }
    void setModFreq (float v) { modFreq = v; }

    void update()
    {
        c1 = 0.00056689343f * sampleRate;
        c2 = 3.1415927f / sampleRate;
        dts = std::floor (std::min (65533.0f, sampleRate * delayTime));
        for (int k = 0; k < 24; ++k)
        {
            prime[k] = primes[std::min (399, std::max (0, int (float (primeIndex[k]) * size)))];
            sl[k] = 0.0001f * prime[k];
        }
    }

    void prepare (float sr) { sampleRate = sr; update(); }

    // de.fdelay1a allpass with a smooth_init length; writes its input, returns its next output.
    // Two forms of the output expression, as faust generates them for different allpasses.
    static float allpassA (float* b, int w, float in, float& y, float& xp, float r, float sl, float p, float imp)
    {
        y = sl + 0.9999f * (imp * p + y);
        const float dmo = -1.49999f + y;
        b[w & 16383] = in;
        const float xd = b[(w - int (std::min (8192.0f, std::max (0.0f, dmo)))) & 16383];
        const float fl = std::floor (dmo);
        const float rn = xp - (2.0f - y + fl) * (r - xd) / (y - fl);
        xp = xd;
        return rn;
    }

    static float allpassB (float* b, int w, float in, float& y, float& xp, float r, float sl, float p, float imp)
    {
        y = sl + 0.9999f * (imp * p + y);
        const float dmo = -1.49999f + y;
        b[w & 16383] = in;
        const float xd = b[(w - int (std::min (8192.0f, std::max (0.0f, dmo)))) & 16383];
        const float fl = std::floor (dmo);
        const float ca = 2.0f - y + fl;
        const float cb = y - fl;
        const float rn = -(r * ca / cb + ca * xd / cb + xp);
        xp = xd;
        return rn;
    }

    // de.fdelay4 (Lagrange, order 4) read of an output line at the modulated delay 8.500005 + t8
    static float fdelay4 (const float* line, int w, float t8)
    {
        const float t9 = 8.500005f + t8;
        const float t10 = std::floor (t9);
        const float t11 = 7.0f - t10 + t8;
        const float t12 = 8.0f - t10 + t8;
        const int i13 = int (std::min (516.0f, std::max (-4.0f, t9)));
        const float t14 = 9.0f - t10 + t8;
        const float t15 = 10.0f - t10 + t8;
        const float t16 = t15 * t14;
        const float t17 = t16 * t12;
        const float x0 = line[(w - (1 + std::min (516, std::max (0, i13)))) & 1023];
        const float x1 = line[(w - (1 + std::min (516, std::max (0, i13 + 1)))) & 1023];
        const float x2 = line[(w - (1 + std::min (516, std::max (0, i13 + 2)))) & 1023];
        const float x3 = line[(w - (1 + std::min (516, std::max (0, i13 + 3)))) & 1023];
        const float x4 = line[(w - (1 + std::min (516, std::max (0, i13 + 4)))) & 1023];
        return (6.0f - t10 + t8) * (t11 * (t12 * (0.041666668f * x0 * t14 - 0.16666667f * t15 * x1)
                                         + 0.25f * t16 * x2) - 0.16666667f * t17 * x3)
             + 0.041666668f * t17 * t11 * x4;
    }

    void process (float* const* in, float* const* out, int n)
    {
        const float* inL = in[0];
        const float* inR = in[1];
        float* outL = out[0];
        float* outR = out[1];

        const float pDiff = diffusion, pFreq = modFreq, pDepth = modDepth, pFb = feedback, pDamp = damping;
        const float k1 = c1, k2 = c2, dtsv = dts;
        float p[24], l[24], ys[24], xps[24], rs[24];
        for (int k = 0; k < 24; ++k) { p[k] = prime[k]; l[k] = sl[k]; ys[k] = y[k]; xps[k] = xp[k]; rs[k] = r[k]; }
        float dfp = diffPrev, frp = freqPrev, dpp = depthPrev, fbp = fbPrev, dmp = dampPrev;
        float im = imp, os_ = oscS, oc_ = oscC;
        float v_ = sdv, i_ = sdi, d0 = sd0, d1 = sd1;
        float sml = smL, spl = spL, smr = smR, spr = spR;
        int wp = w;

        for (int i = 0; i < n; ++i)
        {
            // linear_interp of the controls
            const float sdf = pDiff + dfp;
            dfp = pDiff;
            const float th = k2 * (pFreq + frp);
            frp = pFreq;
            const float dd = k1 * (pDepth + dpp);
            dpp = pDepth;
            const float fbs = pFb + fbp;
            fbp = pFb;
            const float dms = pDamp + dmp;
            dmp = pDamp;
            const float c = std::cos (0.5f * sdf);
            const float s = std::sin (0.5f * sdf);
            const float cn = std::cos (-0.5f * sdf);
            const float sn = std::sin (-0.5f * sdf);
            // modulation oscillator (fi.nlf2)
            const float os = std::sin (th);
            const float oc = std::cos (th);
            const float nS = oc_ * os + os_ * oc;
            const float nC = im - os * os_ + oc_ * oc;
            os_ = nS;
            oc_ = nC;
            // feedback path: de.fdelay4 read of the output lines, then de.sdelay
            const float fdL = fdelay4 (lineL, wp, dd * (1.0f + oc_));
            const float fdR = fdelay4 (lineR, wp, dd * (1.0f + os_));
            sdL[wp & 65535] = fdL;
            sdR[wp & 65535] = fdR;
            float v = 0.0f;
            if (v_ != 0.0f)
            {
                if (i_ > 0.0f && i_ < 1.0f) v = v_;
            }
            else if (i_ == 0.0f && dtsv != d0) v = 0.000045351473f;
            else if (i_ == 1.0f && dtsv != d1) v = -0.000045351473f;
            const float ni = std::max (0.0f, std::min (1.0f, i_ + v));
            float n0 = d0;
            if (i_ >= 1.0f && d1 != dtsv) n0 = dtsv;
            float n1 = d1;
            if (i_ <= 0.0f && d0 != dtsv) n1 = dtsv;
            v_ = v;
            i_ = ni;
            d0 = n0;
            d1 = n1;
            const int j0 = int (std::min (65536.0f, std::max (0.0f, d0)));
            const int j1 = int (std::min (65536.0f, std::max (0.0f, d1)));
            const float aL = sdL[(wp - j0) & 65535];
            const float x1 = inL[i] + 0.5f * (aL + i_ * (sdL[(wp - j1) & 65535] - aL)) * fbs;
            const float aR = sdR[(wp - j0) & 65535];
            const float x2 = inR[i] + 0.5f * fbs * (aR + i_ * (sdR[(wp - j1) & 65535] - aR));

            float rn[24];
            auto apA = [&] (int k, float x) { rn[k] = allpassA (ap[k], wp, x, ys[k], xps[k], rs[k], l[k], p[k], im); };
            auto apB = [&] (int k, float x) { rn[k] = allpassB (ap[k], wp, x, ys[k], xps[k], rs[k], l[k], p[k], im); };
            // diffuser 1 (angle diff)
            const float a1 = x1 * c - s * rs[6];
            const float a2 = c * a1 - s * rs[4];
            const float a3 = c * a2 - s * rs[2];
            const float b1 = c * x2 - s * rs[7];
            const float b2 = c * b1 - s * rs[5];
            const float b3 = c * b2 - s * rs[3];
            apA (0, c * a3 - rs[1] * s);
            apA (1, rs[0] * s - c * b3);
            apB (2, rs[0] * c + s * b3);
            apA (3, rs[1] * c + s * a3);
            apB (4, c * rs[3] + s * b2);
            apA (5, rs[2] * c + s * a2);
            apB (6, c * rs[5] + s * b1);
            apA (7, rs[4] * c + s * a1);
            const float l1 = rs[6] * c + s * x1;
            const float q1 = c * rs[7] + s * x2;
            // diffuser 2 (angle -diff)
            const float d1_ = l1 * cn - sn * rs[14];
            const float d2_ = cn * d1_ - sn * rs[12];
            const float d3_ = cn * d2_ - sn * rs[10];
            const float e1 = cn * q1 - sn * rs[15];
            const float e2 = cn * e1 - sn * rs[13];
            const float e3 = cn * e2 - sn * rs[11];
            apA (8, cn * d3_ - rs[9] * sn);
            apA (9, rs[8] * sn - cn * e3);
            apB (10, rs[8] * cn + sn * e3);
            apA (11, rs[9] * cn + sn * d3_);
            apB (12, cn * rs[11] + sn * e2);
            apA (13, rs[10] * cn + sn * d2_);
            apB (14, cn * rs[13] + sn * e1);
            apA (15, rs[12] * cn + sn * d1_);
            const float l2 = rs[14] * cn + sn * l1;
            const float q2 = cn * rs[15] + sn * q1;
            // diffuser 3 (angle diff)
            const float f1 = c * l2 - s * rs[22];
            const float f2 = c * f1 - s * rs[20];
            const float f3 = c * f2 - s * rs[18];
            const float g1 = c * q2 - s * rs[23];
            const float g2 = c * g1 - s * rs[21];
            const float g3 = c * g2 - s * rs[19];
            apA (16, c * f3 - s * rs[17]);
            apA (17, s * rs[16] - c * g3);
            apB (18, c * rs[16] + s * g3);
            apA (19, c * rs[17] + s * f3);
            apB (20, c * rs[19] + s * g2);
            apA (21, c * rs[18] + s * f2);
            apB (22, c * rs[21] + s * g1);
            apA (23, c * rs[20] + s * f1);
            const float yl = c * rs[22] + s * l2;
            const float yr = c * rs[23] + s * q2;
            // allpass outputs, one sample late
            for (int k = 0; k < 24; ++k) rs[k] = rn[k];
            // si.smooth(damp), the outputs and the lines read back by de.fdelay4
            sml = 0.5f * dms * (sml + std::min (3.4028234663852886e38f, spl - yl));
            spl = yl;
            const float oL = yl + sml;
            smr = 0.5f * dms * (smr + std::min (3.4028234663852886e38f, spr - yr));
            spr = yr;
            const float oR = yr + smr;
            lineL[wp & 1023] = oL;
            lineR[wp & 1023] = oR;
            outL[i] = oL;
            outR[i] = oR;
            im = 0.0f;
            wp = (wp + 1) & 65535;
        }

        for (int k = 0; k < 24; ++k) { y[k] = ys[k]; xp[k] = xps[k]; r[k] = rs[k]; }
        diffPrev = dfp; freqPrev = frp; depthPrev = dpp; fbPrev = fbp; dampPrev = dmp;
        imp = im; oscS = os_; oscC = oc_;
        sdv = v_; sdi = i_; sd0 = d0; sd1 = d1;
        smL = sml; spL = spl; smR = smr; spR = spr;
        w = wp;
    }
};
using Dsp = Greyhole;
