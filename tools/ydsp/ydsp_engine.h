// Plain C interface to a YUP YDSP patch compiled by the YDSP JIT (tools/ydsp/ydsp_engine.cpp).
#pragma once
#ifdef __cplusplus
extern "C" {
#endif
// Compiles the .ydsp file and prepares the graph; returns nullptr (and prints the diagnostics) on failure.
void* ydsp_create (const char* path, int fastMath, double sampleRate, int blockSize, int* numInputs, int* numOutputs);
void ydsp_reset (void* engine);
// in/out: at least numInputs/numOutputs channel pointers of numSamples floats
void ydsp_process (void* engine, float* const* in, float* const* out, int numSamples);
void ydsp_destroy (void* engine);
#ifdef __cplusplus
}
#endif
