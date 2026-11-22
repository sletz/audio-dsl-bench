// YDSP side of the benchmark: compiles a .ydsp patch with the YUP JIT and runs it block by block.
#include <yup_dsp_jit/yup_dsp_jit.h>

#include <array>
#include <cstdio>
#include <cstdlib>

#include "ydsp_engine.h"

namespace
{
constexpr int maxChannels = 16;

struct YdspEngine
{
    yup::YdspAudioGraph graph;
    int numInputs = 0;
    int numOutputs = 0;
};
} // namespace

extern "C" void* ydsp_create (const char* path, int fastMath, double sampleRate, int blockSize, int* numInputs, int* numOutputs)
{
    const yup::File file (yup::String::fromUTF8 (path));
    yup::YdspCompileOptions options;
    options.fastMath = fastMath != 0;
    options.emitOptimizationReport = getenv ("YDSP_REPORT") != nullptr;

    yup::YdspCompiler compiler;
    auto result = compiler.compile (file.loadFileAsString(), options, file.getParentDirectory().getFullPathName());
    if (! result.wasOk())
    {
        const auto& diagnostics = compiler.getDiagnostics();
        fprintf (stderr, "ydsp: %s: %s\n", path,
                 (diagnostics.hasErrors() ? diagnostics.toString() : result.getErrorMessage()).toRawUTF8());
        return nullptr;
    }
    if (options.emitOptimizationReport)
    {
        const auto& r = compiler.getOptimizationReport();
        fprintf (stderr, "ydsp report %s (fastMath %d): vector width %d, contraction %d, code %d bytes, compile %.1f ms\n",
                 file.getFileName().toRawUTF8(), fastMath, r.vectorWidth, (int) r.contractionEnabled,
                 (int) r.generatedCodeSize, r.compileTimeMilliseconds);
        for (auto& t : r.rejectedTransforms)
            fprintf (stderr, "  rejected: %s\n", t.toRawUTF8());
    }

    auto* engine = new YdspEngine { std::move (result.getReference()) };
    engine->numInputs = engine->graph.getInputStreamCount();
    engine->numOutputs = engine->graph.getOutputStreamCount();
    if (engine->numInputs > maxChannels || engine->numOutputs > maxChannels)
    {
        fprintf (stderr, "ydsp: %s: too many streams\n", path);
        delete engine;
        return nullptr;
    }
    auto prepared = engine->graph.prepare (sampleRate, blockSize);
    if (prepared.failed())
    {
        fprintf (stderr, "ydsp: %s: prepare failed: %s\n", path, prepared.getErrorMessage().toRawUTF8());
        delete engine;
        return nullptr;
    }
    *numInputs = engine->numInputs;
    *numOutputs = engine->numOutputs;
    return engine;
}

extern "C" void ydsp_reset (void* e)
{
    static_cast<YdspEngine*> (e)->graph.reset();
}

extern "C" void ydsp_process (void* e, float* const* in, float* const* out, int numSamples)
{
    auto* engine = static_cast<YdspEngine*> (e);
    std::array<yup::YdspInputBuffer, maxChannels> inputs;
    std::array<yup::YdspOutputBuffer, maxChannels> outputs;
    for (int c = 0; c < engine->numInputs; ++c)
        inputs[(size_t) c] = yup::Span<const float> (in[c], (size_t) numSamples);
    for (int c = 0; c < engine->numOutputs; ++c)
        outputs[(size_t) c] = yup::Span<float> (out[c], (size_t) numSamples);

    yup::YdspProcessRequest request;
    request.inputs = yup::Span<const yup::YdspInputBuffer> (inputs.data(), (size_t) engine->numInputs);
    request.outputs = yup::Span<yup::YdspOutputBuffer> (outputs.data(), (size_t) engine->numOutputs);
    request.numSamples = numSamples;
    auto r = engine->graph.process (request);
    if (r != yup::YdspProcessResult::ok)
    {
        fprintf (stderr, "ydsp: process failed (%d)\n", (int) r);
        abort();
    }
}

extern "C" void ydsp_destroy (void* e)
{
    delete static_cast<YdspEngine*> (e);
}
