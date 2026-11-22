#include <faust/dsp/llvm-dsp.h>
#include <cstdio>
#include <string>
int main(int argc, const char** argv) {
    std::string err;
    const char* args[] = {"-I", "/Users/letz/Developpements/faustlibraries", "-scal"};
    llvm_dsp_factory* f = createDSPFactoryFromFile(argv[1], 3, args, "", err, -1);
    if (!f) { fprintf(stderr, "%s\n", err.c_str()); return 1; }
    writeDSPFactoryToIRFile(f, argv[2]);
    printf("%s\n", getDSPMachineTarget().c_str());
    return 0;
}
