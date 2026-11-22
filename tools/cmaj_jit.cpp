// Cmajor JIT side of the benchmark: loads a .cmajor file into the LLVM engine of libCmajPerformer.
// Stream endpoints are matched to the harness channels in declaration order (in1, in2... / out1, out2...).
#define CMAJOR_DLL 1
#include "cmajor/API/cmaj_Engine.h"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <vector>

#include "cmaj_jit.h"

#ifndef CMAJ_DYLIB
#define CMAJ_DYLIB "/usr/local/lib/libCmajPerformer.dylib"
#endif

namespace
{
struct CmajJit
{
    cmaj::Engine engine;
    cmaj::Performer performer;
    std::vector<cmaj::EndpointHandle> inputs, outputs;
};
} // namespace

extern "C" void* cmajjit_create (const char* path, int optLevel, double sampleRate, int blockSize, int* numInputs, int* numOutputs)
{
    static bool loaded = cmaj::Library::initialise (CMAJ_DYLIB);
    if (! loaded) { fprintf (stderr, "cmaj-jit: cannot load %s\n", CMAJ_DYLIB); return nullptr; }

    std::ifstream f (path);
    if (! f) { fprintf (stderr, "cmaj-jit: cannot read %s\n", path); return nullptr; }
    std::stringstream source;
    source << f.rdbuf();

    cmaj::DiagnosticMessageList messages;
    cmaj::Program program;
    if (! program.parse (messages, path, source.str()))
    {
        fprintf (stderr, "cmaj-jit: %s\n", messages.toString().c_str());
        return nullptr;
    }
    auto* jit = new CmajJit();
    jit->engine = cmaj::Engine::create ("llvm");
    if (! jit->engine) { fprintf (stderr, "cmaj-jit: no llvm engine\n"); delete jit; return nullptr; }
    auto settings = jit->engine.getBuildSettings();
    settings.setFrequency (sampleRate)
        .setMaxFrequency (sampleRate)
        .setMaxBlockSize ((uint32_t) blockSize)
        .setMaxStateSize (256 * 1024 * 1024)
        .setOptimisationLevel (optLevel);
    jit->engine.setBuildSettings (settings);

    if (! jit->engine.load (messages, program, {}, {}))
    {
        fprintf (stderr, "cmaj-jit: %s\n", messages.toString().c_str());
        delete jit;
        return nullptr;
    }
    for (auto& e : jit->engine.getInputEndpoints())
        if (e.isStream()) jit->inputs.push_back (jit->engine.getEndpointHandle (e.endpointID.toString().c_str()));
    for (auto& e : jit->engine.getOutputEndpoints())
        if (e.isStream()) jit->outputs.push_back (jit->engine.getEndpointHandle (e.endpointID.toString().c_str()));

    if (! jit->engine.link (messages))
    {
        fprintf (stderr, "cmaj-jit: %s\n", messages.toString().c_str());
        delete jit;
        return nullptr;
    }
    jit->performer = jit->engine.createPerformer();
    if (! jit->performer) { fprintf (stderr, "cmaj-jit: no performer\n"); delete jit; return nullptr; }
    *numInputs = (int) jit->inputs.size();
    *numOutputs = (int) jit->outputs.size();
    return jit;
}

extern "C" void cmajjit_reset (void* e)
{
    static_cast<CmajJit*> (e)->performer.reset();
}

extern "C" void cmajjit_process (void* e, float* const* in, float* const* out, int numSamples)
{
    auto* jit = static_cast<CmajJit*> (e);
    auto& p = jit->performer;
    p.setBlockSize ((uint32_t) numSamples);
    for (size_t c = 0; c < jit->inputs.size(); ++c)
        p.setInputFrames (jit->inputs[c], in[c], (uint32_t) numSamples);
    p.advance();
    for (size_t c = 0; c < jit->outputs.size(); ++c)
        p.copyOutputFrames (jit->outputs[c], out[c], (uint32_t) numSamples);
}

extern "C" void cmajjit_destroy (void* e)
{
    delete static_cast<CmajJit*> (e);
}
