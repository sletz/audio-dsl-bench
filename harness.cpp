// One benchmark binary per DSP: onda AOT objects, Faust C++ classes and the
// Faust LLVM JIT measured by the same loop, on the same input, interleaved.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
#include <pthread.h>
#include <sys/qos.h>

#include <faust/dsp/dsp.h>
#include <faust/dsp/llvm-dsp.h>
#include <faust/gui/UI.h>
#include <faust/gui/meta.h>

#include "onda_meta.h"  // ONDA_STATE_SIZE, ONDA_STATE_ALIGN, ONDA_PARAM_SIZE, onda_write_defaults, ONDA_NIN, ONDA_NOUT

static const int BS = 256;
static const int SR = 48000;
static const int NCH = 16;  // input and output channels provided to every engine

extern "C" {
uint32_t onda_init_strict(const void*, void*, int, void* const*, const int32_t*, const int32_t*, const float*, void*);
uint32_t onda_process_strict(void*, const void*, const void* const*, void* const*, int32_t, int32_t, int32_t,
                             void* const*, const int32_t*, const int32_t*, const float*, void*);
uint32_t onda_init_fast(const void*, void*, int, void* const*, const int32_t*, const int32_t*, const float*, void*);
uint32_t onda_process_fast(void*, const void*, const void* const*, void* const*, int32_t, int32_t, int32_t,
                           void* const*, const int32_t*, const int32_t*, const float*, void*);
}
dsp* make_faust_scal_strict();
dsp* make_faust_scal_fast();
dsp* make_faust_vec_strict();
dsp* make_faust_vec_fast();
dsp* make_faust_vecacc_fast();
dsp* make_faust_ocpp_strict();
dsp* make_faust_ocpp_fast();
dsp* make_faust_best_fast();
dsp* make_faust_ocpp48_strict();
dsp* make_faust_best48_fast();

typedef uint32_t (*init_fn)(const void*, void*, int, void* const*, const int32_t*, const int32_t*, const float*, void*);
typedef uint32_t (*proc_fn)(void*, const void*, const void* const*, void* const*, int32_t, int32_t, int32_t,
                            void* const*, const int32_t*, const int32_t*, const float*, void*);

struct Engine {
    std::string name;
    virtual ~Engine() {}
    virtual void init() = 0;
    virtual void compute(float** in, float** out) = 0;
};

struct OndaEngine : Engine {
    init_fn fi;
    proc_fn fp;
    void* state = nullptr;
    alignas(64) unsigned char params[ONDA_PARAM_SIZE + 64];
    OndaEngine(const char* n, init_fn i, proc_fn p) : fi(i), fp(p) { name = n; }
    ~OndaEngine() { free(state); }
    void init() override {
        free(state);
        size_t sz = (ONDA_STATE_SIZE + ONDA_STATE_ALIGN - 1) / ONDA_STATE_ALIGN * ONDA_STATE_ALIGN;
        state = aligned_alloc(ONDA_STATE_ALIGN < 16 ? 16 : ONDA_STATE_ALIGN, sz ? sz : 64);
        memset(state, 0, sz);
        memset(params, 0, sizeof(params));
        onda_write_defaults(params);
        uint32_t s = fi(params, state, 1, nullptr, nullptr, nullptr, nullptr, nullptr);
        if (s) { fprintf(stderr, "onda init failed %u\n", s); exit(1); }
    }
    void compute(float** in, float** out) override {
        fp(state, params, (const void* const*)in, (void* const*)out, 0, BS, 3, nullptr, nullptr, nullptr, nullptr, nullptr);
    }
};

struct FaustEngine : Engine {
    std::unique_ptr<dsp> d;
    FaustEngine(const char* n, dsp* p) : d(p) { name = n; }
    void init() override { d->init(SR); }
    void compute(float** in, float** out) override { d->compute(BS, in, out); }
};

static double now_ns() {
    return (double)std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

int main(int argc, char** argv) {
    pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
    if (argc < 3) { fprintf(stderr, "usage: %s file.dsp name [rounds]\n", argv[0]); return 1; }
    const char* dspfile = argv[1];
    const char* tag = argv[2];
    int rounds = argc > 3 ? atoi(argv[3]) : 21;

    std::vector<std::unique_ptr<Engine>> engines;
    // a Faust build that rejected the program yields no dsp: that engine is left out
    auto add_faust = [&](const char* n, dsp* p) { if (p) engines.emplace_back(new FaustEngine(n, p)); };
    engines.emplace_back(new OndaEngine("onda", onda_init_strict, onda_process_strict));
    engines.emplace_back(new OndaEngine("onda-fm", onda_init_fast, onda_process_fast));
    add_faust("cpp-scal", make_faust_scal_strict());
    add_faust("cpp-scal-fm", make_faust_scal_fast());
    add_faust("cpp-vec", make_faust_vec_strict());
    add_faust("cpp-vec-fm", make_faust_vec_fast());
    add_faust("cpp-vec-acc", make_faust_vecacc_fast());
    // faust of the master-dev-certification-4 branch: old C++ backend, and the option set elected by fcautotool
    add_faust("ocpp", make_faust_ocpp_strict());
    add_faust("ocpp-fm", make_faust_ocpp_fast());
    add_faust("best-fm", make_faust_best_fast());
    // the same two with SR fixed at 48 kHz at compile time (sr48/platform.lib), as onda does
    add_faust("ocpp48", make_faust_ocpp48_strict());
    add_faust("best48-fm", make_faust_best48_fast());
    std::vector<llvm_dsp_factory*> factories;
    const char* llvm_variants[][6] = {{"llvm-scal", "-I", FAUSTLIB, "-scal", nullptr, nullptr},
                                      {"llvm-vec", "-I", FAUSTLIB, "-vec", "-vs", "32"},
                                      // SR fixed at 48 kHz: sr48/ holds the libraries with platform.lib's SR = 48000.0
                                      {"llvm48", "-I", SR48DIR, "-scal", nullptr, nullptr}};
    for (auto& v : llvm_variants) {
        std::string err;
        int n = v[4] ? 5 : 3;
        llvm_dsp_factory* f = createDSPFactoryFromFile(dspfile, n, (const char**)&v[1], "", err, -1);
        if (!f) { fprintf(stderr, "LLVM factory failed: %s\n", err.c_str()); return 1; }
        factories.push_back(f);
        engines.emplace_back(new FaustEngine(v[0], f->createDSPInstance()));
    }

    // Input: 64 blocks of deterministic white noise, cycled.
    const int NB = 64;
    std::vector<float> noise((size_t)NB * BS * NCH);
    uint32_t seed = 22222;
    for (auto& x : noise) { seed = seed * 1664525u + 1013904223u; x = ((int32_t)seed) * (0.5f / 2147483648.0f); }
    std::vector<float> obuf(BS * NCH, 0.f);

    auto run_blocks = [&](Engine& e, int nblocks, int start) {
        for (int b = 0; b < nblocks; b++) {
            int k = (start + b) % NB;
            float* in[NCH];
            float* out[NCH];
            for (int c = 0; c < NCH; c++) { in[c] = &noise[((size_t)k * NCH + c) * BS]; out[c] = &obuf[(size_t)c * BS]; }
            e.compute(in, out);
        }
    };

    // 1. Output equivalence: 4 s rendered from a fresh instance, against cpp-scal (strict).
    const int NCMP = 4 * SR / BS;
    std::vector<std::vector<float>> outs(engines.size());
    for (size_t i = 0; i < engines.size(); i++) {
        Engine& e = *engines[i];
        e.init();
        outs[i].reserve((size_t)NCMP * BS * NCH);
        for (int b = 0; b < NCMP; b++) {
            int k = b % NB;
            float* in[NCH];
            float* out[NCH];
            for (int c = 0; c < NCH; c++) { in[c] = &noise[((size_t)k * NCH + c) * BS]; out[c] = &obuf[(size_t)c * BS]; }
            e.compute(in, out);
            outs[i].insert(outs[i].end(), obuf.begin(), obuf.end());
        }
    }
    const std::vector<float>& ref = outs[2];
    double refrms = 0;
    for (float x : ref) refrms += (double)x * x;
    refrms = sqrt(refrms / ref.size());

    // 2. Timing: warm-up, then rounds interleaved across engines (shuffled order each round).
    for (auto& e : engines) { e->init(); run_blocks(*e, 2000, 0); }
    // calibrate blocks per round to ~40 ms for the slowest engine
    double worst = 0;
    for (auto& e : engines) {
        double t0 = now_ns(); run_blocks(*e, 200, 0); double t = (now_ns() - t0) / 200;
        worst = std::max(worst, t);
    }
    int K = std::max(50, (int)(40e6 / worst));
    std::vector<std::vector<double>> times(engines.size());
    std::vector<size_t> order(engines.size());
    for (size_t i = 0; i < order.size(); i++) order[i] = i;
    uint32_t rs = 12345;
    for (int r = 0; r < rounds; r++) {
        for (size_t i = order.size() - 1; i > 0; i--) { rs = rs * 1664525u + 1013904223u; std::swap(order[i], order[rs % (i + 1)]); }
        for (size_t idx : order) {
            Engine& e = *engines[idx];
            double t0 = now_ns();
            run_blocks(e, K, r * 7);
            times[idx].push_back((now_ns() - t0) / ((double)K * BS));
        }
    }
    // Print: tag engine median_ns_per_sample min mad maxabsdiff relrms
    for (size_t i = 0; i < engines.size(); i++) {
        auto t = times[i];
        std::sort(t.begin(), t.end());
        double med = t[t.size() / 2];
        std::vector<double> dev;
        for (double x : t) dev.push_back(fabs(x - med));
        std::sort(dev.begin(), dev.end());
        double maxd = 0, sd = 0;
        bool finite = true;
        for (size_t j = 0; j < ref.size(); j++) {
            if (!std::isfinite(outs[i][j])) finite = false;
            double d = fabs((double)outs[i][j] - ref[j]);
            maxd = std::max(maxd, d);
            sd += d * d;
        }
        double rel = refrms > 0 ? sqrt(sd / ref.size()) / refrms : 0;
        printf("%s\t%s\t%.4f\t%.4f\t%.4f\t%.3g\t%.3g\t%s\t%.4g\n", tag, engines[i]->name.c_str(), med, t[0],
               dev[dev.size() / 2], maxd, rel, finite ? "finite" : "NONFINITE", refrms);
    }
    engines.clear();
    for (auto f : factories) deleteDSPFactory(f);
    return 0;
}
