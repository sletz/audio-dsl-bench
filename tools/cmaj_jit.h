// Plain C interface to a Cmajor program run by Cmajor's own LLVM JIT (libCmajPerformer), tools/cmaj_jit.cpp.
#pragma once
extern "C" {
// Compiles the .cmajor file (optLevel 3 = strict, 4 = fast-math); returns nullptr (and prints the errors) on failure.
void* cmajjit_create (const char* path, int optLevel, double sampleRate, int blockSize, int* numInputs, int* numOutputs);
void cmajjit_reset (void* engine);
void cmajjit_process (void* engine, float* const* in, float* const* out, int numSamples);
void cmajjit_destroy (void* engine);
}
