// Measures, on this core, the latency (cycles) of the instructions found in audio sample loops, with
// chains of dependent instructions, and the throughput (instructions per cycle) of each instruction class,
// with independent chains. The clock comes from a chain of dependent integer additions (one cycle each).
// Prints "name value" lines, read by tools/mca_bound.py.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static double now(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec + (double)t.tv_nsec * 1e-9;
}

#define N 10000000ull
#define R4(s) s "\n" s "\n" s "\n" s "\n"
#define R12(a, b, c, d, e, f, g, h, i, j, k, l) a "\n" b "\n" c "\n" d "\n" e "\n" f "\n" g "\n" h "\n" i "\n" j "\n" k "\n" l "\n"
static float mem[64] __attribute__((aligned(64)));
static double secs_per_cycle, fastest_cycle = 1e9;
static double l_add(void);
// the clock ramps up and varies: each measurement is divided by an integer-add chain timed right after it
static double cycles(double (*f)(void))
{
    double t = f();
    double c = l_add();
    fastest_cycle = c < fastest_cycle ? c : fastest_cycle;
    return t / c;
}

// seconds per repetition of an asm block holding `per` instructions of interest
#define BENCH(name, block, per)                                                                        \
    static double name(void)                                                                           \
    {                                                                                                  \
        float f = 1.0f; uint64_t x = 1; double best = 1e9;                                             \
        for (int k = 0; k < 5; k++) {                                                                  \
            double t0 = now();                                                                         \
            for (uint64_t i = 0; i < N; i++)                                                           \
                __asm__ volatile(block : "+w"(f), "+r"(x) : "r"(mem)                                   \
                                 : "x9", "x10", "x11", "x12", "x13", "x14", "x15", "x16", "x17", "x19", "x20", \
                                   "x21", "x22", "v1", "v20", "v21", "v22", "v23", "v24", "v25", "v26", "v27", \
                                   "v28", "v29", "v30", "v31", "memory");                              \
            double t = (now() - t0) / ((double)N * (per)); best = t < best ? t : best;                 \
        }                                                                                              \
        return best;                                                                                   \
    }

// latency chains
BENCH(l_add, R4(R4("add %1, %1, #1")), 16)
BENCH(l_fadd, R4(R4("fadd %s0, %s0, %s0")), 16)
BENCH(l_fmul, R4(R4("fmul %s0, %s0, %s0")), 16)
BENCH(l_fmadd, R4(R4("fmadd %s0, %s0, %s0, %s0")), 16)
BENCH(l_fdiv, R4(R4("fdiv %s0, %s0, %s0")), 16)
BENCH(l_fsqrt, R4(R4("fsqrt %s0, %s0")), 16)
BENCH(l_fmaxnm, R4(R4("fmaxnm %s0, %s0, %s0")), 16)
BENCH(l_frint, R4(R4("frintm %s0, %s0")), 16)
BENCH(l_fcsel, R4(R4("fcmp %s0, #0.0\n fcsel %s0, %s0, %s0, gt")), 16)   // pair
BENCH(l_cvt, R4(R4("fcvtzs w9, %s0\n scvtf %s0, w9")), 16)              // pair
BENCH(l_fmovx, R4(R4("fmov w9, %s0\n fmov %s0, w9")), 16)               // pair
BENCH(l_fcvt, R4(R4("fcvt d1, %s0\n fcvt %s0, d1")), 16)                // pair
BENCH(l_mul, R4(R4("mul %1, %1, %1")), 16)
BENCH(l_madd, R4(R4("madd %1, %1, %1, %1")), 16)
BENCH(l_stld, R4(R4("str %s0, [%2]\n ldr %s0, [%2]")), 16)              // store and reload of the same address
BENCH(l_vfadd, R4(R4("fadd %0.4s, %0.4s, %0.4s")), 16)
BENCH(l_vfmla, R4(R4("fmla %0.4s, %0.4s, %0.4s")), 16)
BENCH(l_vext, R4(R4("ext %0.16b, %0.16b, %0.16b, #4")), 16)
BENCH(l_vdup, R4(R4("dup %0.4s, %0.s[1]")), 16)
// throughput: 12 independent chains
#define F12(op) R12(op " s20, s20, s20", op " s21, s21, s21", op " s22, s22, s22", op " s23, s23, s23", \
                    op " s24, s24, s24", op " s25, s25, s25", op " s26, s26, s26", op " s27, s27, s27", \
                    op " s28, s28, s28", op " s29, s29, s29", op " s30, s30, s30", op " s31, s31, s31")
BENCH(t_fp, R4(F12("fadd")), 48)
BENCH(t_fdiv, R4(F12("fdiv")), 48)
BENCH(t_int, R4(R12("add x10, x10, #1", "add x11, x11, #1", "add x12, x12, #1", "add x13, x13, #1",
                    "add x14, x14, #1", "add x15, x15, #1", "add x16, x16, #1", "add x17, x17, #1",
                    "add x19, x19, #1", "add x20, x20, #1", "add x21, x21, #1", "add x22, x22, #1")), 48)
BENCH(t_ld, R4(R12("ldr s20, [%2]", "ldr s21, [%2, #4]", "ldr s22, [%2, #8]", "ldr s23, [%2, #12]",
                   "ldr s24, [%2, #16]", "ldr s25, [%2, #20]", "ldr s26, [%2, #24]", "ldr s27, [%2, #28]",
                   "ldr s28, [%2, #32]", "ldr s29, [%2, #36]", "ldr s30, [%2, #40]", "ldr s31, [%2, #44]")), 48)
BENCH(t_st, R4(R12("str s20, [%2]", "str s21, [%2, #4]", "str s22, [%2, #8]", "str s23, [%2, #12]",
                   "str s24, [%2, #16]", "str s25, [%2, #20]", "str s26, [%2, #24]", "str s27, [%2, #28]",
                   "str s28, [%2, #32]", "str s29, [%2, #36]", "str s30, [%2, #40]", "str s31, [%2, #44]")), 48)
// all classes at once, each below its own limit (per 24: 7 FP on distinct registers, 12 integer, 5 loads):
// the issue width
#define MIX24 R12("fadd s20, s20, s20", "add x10, x10, #1", "ldr s26, [%2]", "add x11, x11, #1", \
                  "fadd s21, s21, s21", "add x12, x12, #1", "ldr s27, [%2, #4]", "fadd s22, s22, s22", \
                  "add x13, x13, #1", "add x9, x9, #1", "fadd s23, s23, s23", "add x14, x14, #1") \
              R12("fadd s24, s24, s24", "add x15, x15, #1", "ldr s28, [%2, #8]", "add x16, x16, #1", \
                  "fadd s25, s25, s25", "add x17, x17, #1", "ldr s29, [%2, #12]", "fadd s31, s31, s31", \
                  "add x19, x19, #1", "ldr s30, [%2, #16]", "add x21, x21, #1", "add x20, x20, #1")
BENCH(t_mix, R4(MIX24), 96)

int main(void)
{
    for (int k = 0; k < 20; k++) secs_per_cycle = l_add();   // warm up to the steady clock
    printf("ghz %.4f\n", 1e-9 / secs_per_cycle);
    struct { const char* n; double (*f)(void); int pair; } L[] = {
        {"fadd", l_fadd, 0}, {"fmul", l_fmul, 0}, {"fmadd", l_fmadd, 0}, {"fdiv", l_fdiv, 0}, {"fsqrt", l_fsqrt, 0},
        {"fmaxnm", l_fmaxnm, 0}, {"frint", l_frint, 0}, {"fcmp_fcsel", l_fcsel, 1}, {"fcvtzs_scvtf", l_cvt, 1},
        {"fmov_cross", l_fmovx, 1}, {"fcvt_pair", l_fcvt, 1}, {"mul", l_mul, 0}, {"madd", l_madd, 0},
        {"store_reload", l_stld, 1}, {"vfadd", l_vfadd, 0}, {"vfmla", l_vfmla, 0}, {"vext", l_vext, 0},
        {"vdup", l_vdup, 0}};
    for (unsigned i = 0; i < sizeof L / sizeof L[0]; i++)
        printf("lat_%s %.2f\n", L[i].n, cycles(L[i].f));
    // L1 load latency: pointer chase over a random cycle of 512 pointers (4 KB), so that no value predictor helps
    {
        static void* ring[512];
        int perm[512];
        for (int i = 0; i < 512; i++) perm[i] = i;
        srand(1);
        for (int i = 511; i > 0; i--) { int j = rand() % (i + 1), t = perm[i]; perm[i] = perm[j]; perm[j] = t; }
        for (int i = 0; i < 512; i++) ring[perm[i]] = &ring[perm[(i + 1) % 512]];
        void* q = ring[perm[0]]; double best = 1e9;
        for (int k = 0; k < 5; k++) {
            double t0 = now();
            for (uint64_t i = 0; i < N; i++) __asm__ volatile(R4(R4("ldr %0, [%0]")) : "+r"(q));
            double t = (now() - t0) / ((double)N * 16); best = t < best ? t : best;
        }
        printf("lat_ldr %.2f\n", best / l_add());
    }
    struct { const char* n; double (*f)(void); } T[] = {
        {"fp", t_fp}, {"fdiv", t_fdiv}, {"int", t_int}, {"ld", t_ld}, {"st", t_st}, {"mix", t_mix}};
    for (unsigned i = 0; i < sizeof T / sizeof T[0]; i++)
        printf("ipc_%s %.2f\n", T[i].n, 1.0 / cycles(T[i].f));
    printf("ghz_max %.4f\n", 1e-9 / fastest_cycle);   // the highest clock seen during the run
    return 0;
}
