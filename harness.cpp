// One benchmark binary per DSP: onda AOT objects, Faust C++ classes, the Faust LLVM JIT, Cmajor (generated C++
// and its LLVM JIT), YUP's YDSP JIT and a hand-written C++ port, measured by the same loop, on the same input, interleaved.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <new>
#include <string>
#include <vector>
#ifdef __APPLE__
#include <pthread.h>
#include <sys/qos.h>
#endif
#if defined(__linux__) && defined(__x86_64__)
#include <xmmintrin.h>
#endif

#include <faust/dsp/dsp.h>
#include <faust/dsp/llvm-dsp.h>
#include <faust/gui/UI.h>
#include <faust/gui/meta.h>

#include "ext_dsp.h"     // Cmajor and YDSP engines behind the Faust dsp interface
#include "onda_processor_abi.h"
#include "ydsp_engine.h"
#ifdef CMAJ_SRC
#include "cmaj_jit.h"
#endif
#ifndef BENCH_GENERATED_ENGINES
#include "onda_meta.h"
#endif

static const int BS = 256;  // also given to the Cmajor C++ generator (build.sh)
static const int SR = 48000;
static const int NCH = 16;  // input and output channels provided to every engine

#ifndef BENCH_GENERATED_ENGINES
extern "C" decltype(onda_processor_init) onda_init_strict, onda_init_fast;
extern "C" decltype(onda_process) onda_process_strict, onda_process_fast;
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
dsp* make_cmaj_cpp_strict();  // nullptr when the program has no Cmajor port (dsp/NAME.cmajor)
dsp* make_cmaj_cpp_fast();
dsp* make_cxx_strict();  // nullptr when the program has no hand-written C++ port (dsp/NAME.cpp)
dsp* make_cxx_fast();
#endif

#ifdef CMAJ_SRC
static dsp* make_cmaj_jit(int opt) {
    if (!*CMAJ_SRC) return nullptr;
    int nin = 0, nout = 0;
    void* e = cmajjit_create(CMAJ_SRC, opt, SR, BS, &nin, &nout);
    return e ? new c_api_dsp(e, nin, nout, cmajjit_reset, cmajjit_process, cmajjit_destroy) : nullptr;
}
#endif

static dsp* make_ydsp(int fastMath) {
    if (!*YDSP_SRC) return nullptr;
    int nin = 0, nout = 0;
    void* e = ydsp_create(YDSP_SRC, fastMath, SR, BS, &nin, &nout);
    return e ? new c_api_dsp(e, nin, nout, ydsp_reset, ydsp_process, ydsp_destroy) : nullptr;
}

struct InputPorts {
    float* audio[NCH];
    const void* const* onda;
};

struct OutputPorts {
    float* audio[NCH];
    void* onda[NCH];
};

struct Engine {
    std::string name;
    virtual ~Engine() {}
    virtual void init() = 0;
    virtual void compute(InputPorts& in, OutputPorts& out) = 0;
};

struct OndaEngine : Engine {
    onda_processor_init_fn fi;
    onda_processor_process_fn fp;
    std::align_val_t state_align, param_align;
    void* state;
    void* params;
    OndaEngine(const char* n, onda_processor_init_fn i, onda_processor_process_fn p,
               size_t state_bytes, size_t state_alignment, size_t param_alignment,
               const unsigned char* defaults, size_t param_bytes)
        : fi(i), fp(p), state_align{std::max<size_t>(16, state_alignment)},
          param_align{std::max<size_t>(16, param_alignment)},
          state(state_bytes ? ::operator new(state_bytes, state_align) : nullptr),
          params(param_bytes ? ::operator new(param_bytes, param_align) : nullptr) {
        name = n;
        if (params) memcpy(params, defaults, param_bytes);
    }
    ~OndaEngine() override {
        ::operator delete(params, param_align);
        ::operator delete(state, state_align);
    }
    void init() override {
        uint32_t s = fi(params, state, ONDA_PROCESSOR_INIT_FULL, nullptr, nullptr, nullptr, nullptr, nullptr);
        if (s != ONDA_PROCESSOR_EXECUTION_OK) { fprintf(stderr, "%s: init failed %u\n", name.c_str(), s); exit(1); }
    }
    void compute(InputPorts& in, OutputPorts& out) override {
        uint32_t s = fp(state, params, in.onda, out.onda, 0, BS, ONDA_PROCESSOR_FULL_BLOCK,
                        nullptr, nullptr, nullptr, nullptr, nullptr);
        if (s != ONDA_PROCESSOR_EXECUTION_OK) { fprintf(stderr, "%s: process failed %u\n", name.c_str(), s); exit(1); }
    }
};

struct FaustEngine : Engine {
    std::unique_ptr<dsp> d;
    FaustEngine(const char* n, dsp* p) : d(p) { name = n; }
    void init() override { d->init(SR); }
    void compute(InputPorts& in, OutputPorts& out) override { d->compute(BS, in.audio, out.audio); }
};

#ifdef BENCH_GENERATED_ENGINES
#include "engines.inc"
#endif

static double now_ns() {
    return (double)std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

int main(int argc, char** argv) {
#ifdef __APPLE__
    pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
#endif
#if defined(__linux__) && defined(__x86_64__)
    constexpr unsigned ftz_daz = (1u << 15) | (1u << 6);
    const char* flush = std::getenv("BENCH_FLUSH_DENORMALS");
    _mm_setcsr((_mm_getcsr() & ~ftz_daz) | (flush && *flush == '1' ? ftz_daz : 0));
#endif
    if (argc < 3 || argc > 6) {
        fprintf(stderr, "usage: %s file.dsp name [rounds] [round-ms] [ir-dir]\n", argv[0]);
        return 1;
    }
    const char* dspfile = argv[1];
    const char* tag = argv[2];
    int rounds = argc > 3 ? atoi(argv[3]) : 21;
    int round_ms = argc > 4 ? atoi(argv[4]) : 40;
    if (rounds < 1 || round_ms < 1) { fprintf(stderr, "rounds and round-ms must be positive\n"); return 1; }

    std::vector<std::unique_ptr<Engine>> engines;
    // A compiler that rejected the program yields no dsp.
    auto add_faust = [&](const char* n, dsp* p) {
        if (!p) return;
        if (p->getNumInputs() != input_count || p->getNumOutputs() != output_count) {
            fprintf(stderr, "%s: %s has incompatible audio ports\n", tag, n);
            delete p;
            return;
        }
        engines.emplace_back(new FaustEngine(n, p));
    };
#ifdef BENCH_GENERATED_ENGINES
    add_engines(engines, add_faust);
#else
    unsigned char defaults[std::max(1, ONDA_PARAM_SIZE)] = {};
    onda_write_defaults(defaults);
    engines.emplace_back(new OndaEngine("onda", onda_init_strict, onda_process_strict,
        ONDA_STATE_SIZE, ONDA_STATE_ALIGN, ONDA_PARAM_ALIGN, defaults, ONDA_PARAM_SIZE));
    engines.emplace_back(new OndaEngine("onda-fm", onda_init_fast, onda_process_fast,
        ONDA_STATE_SIZE, ONDA_STATE_ALIGN, ONDA_PARAM_ALIGN, defaults, ONDA_PARAM_SIZE));
    add_faust("cpp-scal", make_faust_scal_strict());
    add_faust("cpp-scal-fm", make_faust_scal_fast());
    add_faust("cpp-vec", make_faust_vec_strict());
    add_faust("cpp-vec-fm", make_faust_vec_fast());
    add_faust("cpp-vec-acc", make_faust_vecacc_fast());
    // faust of the master-dev-certification-7 branch: old C++ backend, and the option set elected by fcautotool
    add_faust("ocpp", make_faust_ocpp_strict());
    add_faust("ocpp-fm", make_faust_ocpp_fast());
    add_faust("best-fm", make_faust_best_fast());
    // the same two with SR fixed at 48 kHz at compile time (sr48/platform.lib), as onda does
    add_faust("ocpp48", make_faust_ocpp48_strict());
    add_faust("best48-fm", make_faust_best48_fast());
    // Cmajor: C++ from `cmaj generate` (clang strict and -ffast-math), and its own LLVM JIT at -O3 and -O4 (fast math)
    add_faust("cmaj-cpp", make_cmaj_cpp_strict());
    add_faust("cmaj-cpp-fm", make_cmaj_cpp_fast());
    add_faust("cmaj-jit", make_cmaj_jit(3));
    add_faust("cmaj-jit-fm", make_cmaj_jit(4));
    // hand-written C++ (dsp/NAME.cpp), clang strict and -ffast-math
    add_faust("cxx", make_cxx_strict());
    add_faust("cxx-fm", make_cxx_fast());
#endif
    if (input_count > NCH || output_count < 1 || output_count > NCH) {
        fprintf(stderr, "%s: unsupported audio port count\n", tag);
        return 1;
    }
    // YUP's YDSP JIT (asmjit), strict and with its default fastMath
    add_faust("ydsp", make_ydsp(0));
    add_faust("ydsp-fm", make_ydsp(1));
    std::vector<llvm_dsp_factory*> factories;
    const char* llvm_variants[][6] = {{"llvm-scal", "-I", FAUSTLIB, "-scal", nullptr, nullptr},
                                      {"llvm-vec", "-I", FAUSTLIB, "-vec", "-vs", "32"},
                                      // SR fixed at 48 kHz: sr48/ holds the libraries with platform.lib's SR = 48000.0
                                      {"llvm48", "-I", SR48DIR, "-scal", nullptr, nullptr},
#ifdef BENCH_GENERATED_ENGINES
                                      {"llvm48-vec", "-I", SR48DIR, "-vec", "-vs", "32"},
#endif
    };
    const char* faust_opt = std::getenv("FAUST_OPT");
    bool llvm_no_fast = faust_opt && strcmp(faust_opt, "FAUST_LLVM_NO_FM") == 0;
    for (auto& v : llvm_variants) {
        std::string err;
        int n = v[4] ? 5 : 3;
        llvm_dsp_factory* f = createDSPFactoryFromFile(dspfile, n, (const char**)&v[1], "", err, -1);
        if (!f) { fprintf(stderr, "%s: %s: %s\n", tag, v[0], err.c_str()); continue; }
        factories.push_back(f);
        std::string label = std::string(v[0]) + (llvm_no_fast ? "-no-fm" : "");
        if (argc > 5 && *argv[5] && !writeDSPFactoryToIRFile(f, std::string(argv[5]) + "/" + label + ".ll")) {
            fprintf(stderr, "%s: %s IR export failed\n", tag, label.c_str());
            return 1;
        }
        add_faust(label.c_str(), f->createDSPInstance());
    }

    // Input: 64 blocks of deterministic white noise, cycled.
    const int NB = 64;
    std::vector<float> noise((size_t)NB * BS * NCH);
    uint32_t seed = 22222;
    for (auto& x : noise) { seed = seed * 1664525u + 1013904223u; x = ((int32_t)seed) * (0.5f / 2147483648.0f); }
    float obuf[NCH][BS] = {};
    auto run_blocks = [&](Engine& e, int nblocks, int start) {
        for (int b = 0; b < nblocks; b++) {
            int k = (start + b) % NB;
            InputPorts in;
            OutputPorts out;
            const void* onda_in[NCH];
            for (int c = 0; c < NCH; c++) {
                onda_in[c] = in.audio[c] = &noise[((size_t)k * NCH + c) * BS];
                out.onda[c] = out.audio[c] = obuf[c];
            }
            in.onda = input_count ? onda_in : nullptr;
            e.compute(in, out);
        }
    };

    // 1. Output equivalence: 4 s rendered from a fresh instance, against cpp-scal (strict).
    const int NCMP = 4 * SR / BS;
    auto reference = std::find_if(engines.begin(), engines.end(), [](const auto& e) { return e->name == "cpp-scal"; });
    if (reference == engines.end()) { fprintf(stderr, "%s: missing cpp-scal reference\n", tag); return 1; }
    auto render = [&](Engine& e, int b) {
        for (int c = 0; c < output_count; c++)
            std::fill_n(obuf[c], BS, std::numeric_limits<float>::quiet_NaN());
        run_blocks(e, 1, b);
    };
    std::vector<float> ref;
    ref.reserve((size_t)NCMP * BS * output_count);
    double energy = 0;
    (*reference)->init();
    for (int b = 0; b < NCMP; b++) {
        render(**reference, b);
        for (int c = 0; c < output_count; c++) {
            for (float sample : obuf[c]) {
                if (!std::isfinite(sample)) { fprintf(stderr, "%s: non-finite reference output\n", tag); return 1; }
                energy += double(sample) * sample;
            }
            ref.insert(ref.end(), obuf[c], obuf[c] + BS);
        }
    }
    auto relative_rms = [](double error, double energy) {
        if (energy > 0) return sqrt(error / energy);
        return error == 0 ? 0.0 : std::numeric_limits<double>::infinity();
    };
    struct Error { double max_abs = 0, rel_rms = 0; bool finite = true; };
    std::vector<Error> errors(engines.size());
    double refrms = sqrt(energy / ref.size());
    for (size_t i = 0; i < engines.size(); i++) {
        Engine& e = *engines[i];
        e.init();
        auto& err = errors[i];
        double squared = 0;
        size_t j = 0;
        for (int b = 0; b < NCMP; b++) {
            render(e, b);
            for (int c = 0; c < output_count; c++) {
                for (float sample : obuf[c]) {
                    double d = fabs(double(sample) - ref[j++]);
                    if (!std::isfinite(sample)) err.finite = false;
                    err.max_abs = std::max(err.max_abs, d);
                    squared += d * d;
                }
            }
        }
        err.rel_rms = relative_rms(squared, energy);
        if (!err.finite) err.rel_rms = std::numeric_limits<double>::infinity();
    }

    // 2. Timing: warm-up, then rounds interleaved across engines (shuffled order each round).
    for (auto& e : engines) { e->init(); run_blocks(*e, 2000, 0); }
    // calibrate blocks per round to ~40 ms for the slowest engine
    double worst = 0;
    for (auto& e : engines) {
        double t0 = now_ns(); run_blocks(*e, 200, 0); double t = (now_ns() - t0) / 200;
        worst = std::max(worst, t);
    }
    int K = std::max(50, (int)(round_ms * 1e6 / worst));
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
        const auto& err = errors[i];
        printf("%s\t%s\t%.5f\t%.5f\t%.5f\t%.7g\t%.7g\t%s\t%.7g", tag, engines[i]->name.c_str(), med, t[0],
               dev[dev.size() / 2], err.max_abs, err.rel_rms, err.finite ? "finite" : "NONFINITE", refrms);
        putchar('\n');
    }
    engines.clear();
    for (auto f : factories) deleteDSPFactory(f);
    return 0;
}
