// frenchbell: pm.frenchBell_ui of Faust's physmodels.lib, gate held at 1 (struck once): the strike (noise
// through a 10 Hz highpass and a lowpass at strikeCutOff, TPT SVFs, times an en.ar envelope) excites 50
// modes (pm.modeFilter, Chamberlin form) in parallel, averaged.
struct FrenchBell
{
    static constexpr int numInputs = 0, numOutputs = 2;
    static constexpr int M = 50;
    static constexpr float BIG = 3.4028235e38f;        // ma.MAX

    // pm.modeFilter: 0.5*ln(1000)/t60 with t60 = 30*(1 - f/2555.3)^3, and pi*f, per mode
    static constexpr float DECAY[M] = {
        0.202687562f, 0.203040808f, 0.259523958f, 0.261662602f, 0.305520266f, 0.30564943f, 0.392140836f, 0.422799528f,
        0.579614997f, 0.580058396f, 0.589833498f, 0.591507912f, 0.594091773f, 0.685432851f, 0.690533578f, 0.766673744f,
        0.771469116f, 1.20678282f, 1.24625671f, 1.25343716f, 1.69183767f, 1.69671214f, 2.15210032f, 2.15801215f,
        2.70792055f, 2.73561692f, 2.97851849f, 3.00400162f, 4.54476452f, 4.61840248f, 4.83327723f, 5.40900278f,
        7.17300463f, 7.77074337f, 13.6434813f, 13.9355412f, 15.3085737f, 15.4016008f, 17.7427902f, 19.2863369f,
        22.6243153f, 24.4510536f, 46.6008148f, 48.3385239f, 110.371704f, 112.874847f, 114.054665f, 126.426628f,
        559.485779f, 613.32312f
    };
    static constexpr float ANGLE[M] = {
        1379.40112f, 1383.25891f, 1905.2251f, 1921.95093f, 2229.32764f, 2230.14429f, 2692.23804f, 2824.4519f,
        3343.81689f, 3345.01074f, 3371.02319f, 3375.42139f, 3382.17578f, 3598.44312f, 3609.37573f, 3760.76904f,
        3769.62842f, 4359.58789f, 4398.73242f, 4405.67529f, 4750.27637f, 4753.41797f, 5002.89209f, 5005.65674f,
        5225.88232f, 5235.37012f, 5313.43848f, 5321.13525f, 5670.04053f, 5682.63867f, 5717.91846f, 5802.96143f,
        6002.73535f, 6056.04834f, 6393.36084f, 6404.85938f, 6454.90479f, 6458.07764f, 6530.39746f, 6571.45801f,
        6646.91895f, 6682.19922f, 6942.47998f, 6955.64307f, 7213.56787f, 7219.63135f, 7222.42725f, 7249.60205f,
        7553.771f, 7568.06543f
    };
    // modesGains of pm.frenchBellModel: 7 strike positions x 50 modes
    static constexpr float GAINS[7 * M] = {
        0.972890019f, 0.542393029f, 0.495831996f, 0.897966027f, 0.552366972f, 0.557895005f, 0.61421299f, 0.353693992f,
        0.436039001f, 0.441024005f, 0.674912989f, 0.566753983f, 0.755007982f, 0.699029982f, 0.164398f, 0.910040021f,
        0.628373027f, 0.201718003f, 0.429517001f, 0.503714979f, 0.871173978f, 0.106885999f, 0.76117301f, 0.673601985f,
        0.291936994f, 0.588590026f, 0.31527999f, 0.41308099f, 0.274464011f, 0.494062006f, 0.696120977f, 0.612010002f,
        0.382757008f, 0.995113015f, 0.228806004f, 0.198449001f, 0.595847011f, 0.306263f, 0.252397001f, 0.0883567035f,
        0.236085996f, 1.0f, 0.245278001f, 0.379388005f, 0.198824003f, 0.548892021f, 0.492763996f, 0.42087099f,
        0.794637024f, 0.605633974f, 1.0f, 0.604158998f, 0.399841011f, 0.799346983f, 0.507187009f, 0.509809971f,
        0.477382988f, 0.310225993f, 0.426975995f, 0.437622994f, 0.735711992f, 0.630728006f, 0.625784993f, 0.651167989f,
        0.277738005f, 0.850857973f, 0.578078985f, 0.645353973f, 0.383700013f, 0.370597988f, 0.78254199f, 0.181325004f,
        0.614391029f, 0.740683973f, 0.342440993f, 0.586185992f, 0.286909014f, 0.405196995f, 0.259214997f, 0.566982985f,
        0.748219013f, 0.655897975f, 0.368259996f, 0.940814018f, 0.336156994f, 0.413702011f, 0.561556995f, 0.402175993f,
        0.117697999f, 0.329369009f, 0.254570991f, 0.870706022f, 0.260980994f, 0.274122f, 0.206247002f, 0.645299971f,
        0.400757998f, 0.36362201f, 0.636834025f, 0.584565997f, 0.975602984f, 0.616936982f, 0.295401007f, 0.650447011f,
        0.464221001f, 0.465056986f, 0.312467009f, 0.238358006f, 0.383695006f, 0.399673998f, 0.753911972f, 0.662011981f,
        0.504267991f, 0.599247992f, 0.378665f, 0.725363016f, 0.493214011f, 1.0f, 0.332836002f, 0.265888989f,
        0.674072981f, 0.320401013f, 0.440017998f, 0.769782007f, 0.316419005f, 0.529968023f, 0.380508989f, 0.578675985f,
        0.249013007f, 0.591383994f, 0.761717021f, 0.687057018f, 0.324436992f, 0.818863988f, 0.505369008f, 0.672484994f,
        0.461782992f, 0.426198006f, 0.0678874999f, 0.43532899f, 0.347954988f, 0.708393991f, 0.293321997f, 0.328985989f,
        0.258253992f, 0.893512011f, 0.320131004f, 0.433553994f, 0.459302008f, 0.542213023f, 0.817241013f, 0.516229987f,
        0.205302f, 0.467354f, 0.388682991f, 0.388215989f, 0.171261996f, 0.150865003f, 0.291720003f, 0.311414003f,
        0.658876002f, 0.570647001f, 0.38361901f, 0.502454996f, 0.364113986f, 0.532312989f, 0.352988988f, 1.0f,
        0.261983991f, 0.219591007f, 0.544031024f, 0.337199003f, 0.279172987f, 0.668303013f, 0.208439007f, 0.399230003f,
        0.418673992f, 0.648617983f, 0.234133005f, 0.504728973f, 0.645346999f, 0.572851002f, 0.232828006f, 0.614292026f,
        0.48527199f, 0.666263998f, 0.316570014f, 0.320354998f, 0.191421002f, 0.340131015f, 0.342069f, 0.538371027f,
        0.281130999f, 0.393115014f, 0.251394004f, 0.890725017f, 0.310644001f, 0.503700018f, 0.299089998f, 0.442478001f,
        0.733128011f, 0.455217004f, 0.199322f, 0.315699011f, 0.375856012f, 0.376489997f, 0.0291450005f, 0.0200283006f,
        0.279578f, 0.316799998f, 0.655956984f, 0.546842992f, 0.349665999f, 0.470248997f, 0.353765011f, 0.286794007f,
        0.180185005f, 1.0f, 0.210831001f, 0.280133009f, 0.535853028f, 0.376488f, 0.153669998f, 0.634745002f,
        0.0510449f, 0.485574991f, 0.593110979f, 0.917883992f, 0.380477011f, 0.422924995f, 0.599372983f, 0.311421007f,
        0.135654002f, 0.359954f, 0.295747995f, 0.474438995f, 0.353338987f, 0.116742998f, 0.45431301f, 0.112857997f,
        0.359310001f, 0.483897001f, 0.301607996f, 0.577341974f, 0.262663007f, 0.79498601f, 0.549579978f, 0.808085978f,
        0.152511001f, 0.439590991f, 0.535941005f, 0.308017999f, 0.419836998f, 0.579191029f, 0.250427991f, 0.252119988f,
        0.102860004f, 0.288331985f, 0.599678993f, 0.665108025f, 0.636285007f, 0.495234013f, 0.251612991f, 0.208526999f,
        0.0939071998f, 0.458415002f, 0.318951994f, 0.235349998f, 0.215188995f, 0.313412011f, 0.0472786985f, 0.0386893004f,
        0.0863358974f, 0.222345993f, 0.361510992f, 0.997036994f, 0.238508999f, 0.382140011f, 1.0f, 0.203554004f,
        0.472086996f, 0.509015024f, 0.206370994f, 0.441013008f, 0.794008017f, 0.971623003f, 0.796383977f, 0.55043f,
        0.687780023f, 0.554297984f, 0.0436545014f, 0.0595576987f, 0.214684993f, 0.581470013f, 0.277319998f, 0.36846599f,
        0.121381998f, 0.152739003f, 0.0782243982f, 0.190496996f, 0.31626901f, 0.180455998f, 0.405196011f, 0.518715978f,
        0.159364998f, 0.165808007f, 0.194371998f, 0.614763975f, 0.553415f, 0.644793987f, 0.441780001f, 0.441489995f,
        0.221588001f, 1.0f, 0.579370975f, 0.606338978f, 0.529166996f, 0.0214475002f, 0.41704601f, 0.663151979f,
        0.894841015f, 0.475751996f, 0.168289006f, 0.468589991f, 0.304603994f, 0.666369021f, 0.308337003f, 0.295091003f,
        0.768944979f, 0.350910991f, 0.490680009f, 0.516583979f, 0.40011701f, 0.820686996f, 0.545485973f, 0.709357023f,
        0.509759009f, 0.472005993f, 0.152455002f, 0.981265008f, 0.459134012f, 0.698665977f, 0.247153997f, 0.623252988f,
        0.255508006f, 0.830815017f, 0.0887089968f, 0.126082003f, 0.0770623013f, 0.263327986f
    };

    int strikePosition = 0;
    float strikeCutOff = 6500.0f, strikeSharpness = 0.5f, gain = 1.0f, gate = 1.0f;  // parameters
    float sampleRate;
    float gl, glk, cl, invAtt;          // from the parameters
    float gi[M];                        // input gain per mode, referred to 44.1 kHz
    float f2[M], qq1[M], op[M], b0[M];  // pm.modeFilter coefficients per mode
    float gh, ghk, gh9, ch;             // 10 Hz highpass coefficients
    float low[M], band[M];              // mode states: lowpass / f and bandpass
    uint32_t seed;                      // no.noise
    float h1, h2, l1, l2;               // 10 Hz highpass and lowpass at strikeCutOff
    float gatePrev;
    int count;                          // en.ar: samples since the strike
    float xp;                           // previous strike sample (the modes take x + x')

    void setStrikePosition (int v) { strikePosition = v; update(); }
    void setStrikeCutOff (float v) { strikeCutOff = v; update(); }
    void setStrikeSharpness (float v) { strikeSharpness = v; update(); }
    void setGain (float v) { gain = v; }
    void setGate (float v) { gate = v; }

    void update()
    {
        const float sr = sampleRate;
        const int p = M * strikePosition;
        for (int i = 0; i < M; ++i)
            gi[i] = std::min (11025.0f / sr * (op[i] * GAINS[p + i] / b0[i]), BIG);
        gl = std::tan (std::max (0.0f, std::min (3.14159274f / sr * strikeCutOff, 1.5676547f)));
        glk = gl + 1.4142135f;
        cl = gl * std::min (BIG, 1.0f / (gl * glk + 1.0f));
        invAtt = 1.0f / std::max (1.0f, 0.002f * sr * strikeSharpness);
    }

    void prepare (float sr)
    {
        sampleRate = sr;
        for (int i = 0; i < M; ++i)
        {
            const float t = std::tanh (DECAY[i] / sr);
            const float t2 = t * t;
            const float s = std::sin (ANGLE[i] / sr);
            b0[i] = t2 + s * s * (1.0f - t2);
            op[i] = (t + 1.0f) * (t + 1.0f);
            f2[i] = std::min (4.0f * (b0[i] / op[i]), BIG);
            qq1[i] = std::min (t / b0[i], BIG) + 1.0f;
        }
        gh = std::tan (std::max (0.0f, std::min (3.14159274f * 10.0f / sr, 1.5676547f)));
        ghk = gh + 1.4142135f;
        gh9 = -1.4142135f - gh;
        ch = gh * std::min (BIG, 1.0f / (gh * ghk + 1.0f));
        update();
    }

    void process (float* const*, float* const* out, int n)
    {
        float* o1 = out[0];
        float* o2 = out[1];
        float lo[M], bd[M], cf[M], cq[M], cg[M];
        for (int i = 0; i < M; ++i)
        {
            lo[i] = low[i]; bd[i] = band[i];
            cf[i] = f2[i]; cq[i] = qq1[i]; cg[i] = gi[i];
        }
        const float GH = gh, GHK = ghk, GH9 = gh9, CH = ch, GL = gl, GLK = glk, CL = cl, IA = invAtt;
        const float G = gain, GT = gate;
        uint32_t sd = seed;
        float lh1 = h1, lh2 = h2, ll1 = l1, ll2 = l2, gp = gatePrev, lxp = xp;
        int cnt = count;
        for (int j = 0; j < n; ++j)
        {
            // strike: noise -> highpass -> lowpass, times the attack-release envelope
            sd = 12345u + 1103515245u * sd;
            const float nz = 0.0000000004656613f * (float) (int32_t) sd;
            const float t1 = nz - (GHK * lh1 + lh2);
            const float v1 = lh1 + CH * t1;
            const float hp = nz + GH9 * v1 - lh2;
            lh1 += 2.0f * CH * t1;
            lh2 += 2.0f * GH * v1;
            const float t3 = hp - (GLK * ll1 + ll2);
            const float w1 = ll1 + CL * t3;
            const float lp = ll2 + GL * w1;
            ll1 += 2.0f * CL * t3;
            ll2 += 2.0f * GL * w1;
            const int rise = GT > gp ? 1 : 0;
            const int keep = GT <= gp ? 1 : 0;
            if (cnt > 0)
                cnt = rise + (cnt + 1) * keep;
            else
                cnt = rise + cnt * keep;
            gp = GT;
            const float tt = IA * (float) cnt;
            const float x = G * lp * std::max (0.0f, std::min (tt, 2.0f - tt));
            const float xin = x + lxp;
            lxp = x;
            // the 50 modes
            for (int i = 0; i < M; ++i)
            {
                const float l = lo[i] + bd[i];
                bd[i] = bd[i] + cf[i] * (cg[i] * xin - (lo[i] + bd[i] * cq[i]));
                lo[i] = l;
            }
            float acc = 0.0f;
            for (int i = 0; i < M; ++i)
                acc += bd[i];
            const float o = 0.02f * acc;
            o1[j] = o;
            o2[j] = o;
        }
        for (int i = 0; i < M; ++i) { low[i] = lo[i]; band[i] = bd[i]; }
        seed = sd;
        h1 = lh1; h2 = lh2; l1 = ll1; l2 = ll2; gatePrev = gp; xp = lxp;
        count = cnt;
    }
};
using Dsp = FrenchBell;
