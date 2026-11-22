#include <faust/dsp/llvm-dsp.h>
#include <chrono>
#include <cstdio>
#include <string>
int main(int argc, const char** argv) {
    const char* args[] = {"-I", "/Users/letz/Developpements/faustlibraries", "-scal"};
    double best = 1e9;
    for (int r = 0; r < 3; r++) {
        auto t0 = std::chrono::steady_clock::now();
        std::string err;
        llvm_dsp_factory* f = createDSPFactoryFromFile(argv[1], 3, args, "", err, -1);
        dsp* d = f->createDSPInstance(); d->init(48000);
        double t = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        best = std::min(best, t);
        delete d; deleteDSPFactory(f);
    }
    printf("%.3f\n", best);
}
