/**
 * @file cuda_gpu_benchmark.c
 * @brief CUDA GPU vs CPU benchmark + correctness self-test for quantum RNG.
 *
 * Mirrors examples/quantum/metal_gpu_benchmark.c in spirit, but for the CUDA
 * backend (src/quantum_rng/cuda/). Self-contained: it depends only on
 * cuda_bridge.h and the C standard library, and carries its own simple CPU
 * reference implementation for verification and honest timing comparison.
 *
 * COMPILE (RTX 3090 = sm_86):
 *   nvcc -O3 -arch=sm_86 -Isrc -o cuda_test \
 *        src/quantum_rng/cuda/cuda_bridge.cu \
 *        examples/quantum/cuda_gpu_benchmark.c -lm
 *
 * RUN:
 *   ./cuda_test
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <complex.h>

#include "quantum_rng/cuda/cuda_bridge.h"

typedef double _Complex cpx;

static double now_seconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

// ============================================================================
// CPU REFERENCE (double precision) — Grover search, inversion about the mean
// ============================================================================

// Returns the argmax state index; writes elapsed seconds to *elapsed.
static uint32_t cpu_grover_search(int num_qubits, uint32_t target,
                                  int iterations, double* elapsed) {
    uint32_t N = 1u << num_qubits;
    cpx* amp = (cpx*)malloc((size_t)N * sizeof(cpx));
    if (!amp) { *elapsed = -1.0; return 0; }

    double t0 = now_seconds();

    // Uniform superposition
    double inv = 1.0 / sqrt((double)N);
    for (uint32_t i = 0; i < N; i++) amp[i] = inv;

    for (int it = 0; it < iterations; it++) {
        // Oracle: phase flip target
        amp[target] = -amp[target];

        // Diffusion: inversion about the mean
        cpx mean = 0.0;
        for (uint32_t i = 0; i < N; i++) mean += amp[i];
        mean /= (double)N;
        for (uint32_t i = 0; i < N; i++) amp[i] = 2.0 * mean - amp[i];
    }

    // Argmax of |amp|^2
    uint32_t best = 0;
    double best_p = -1.0;
    for (uint32_t i = 0; i < N; i++) {
        double p = creal(amp[i]) * creal(amp[i]) + cimag(amp[i]) * cimag(amp[i]);
        if (p > best_p) { best_p = p; best = i; }
    }

    *elapsed = now_seconds() - t0;
    free(amp);
    return best;
}

// ============================================================================
// CORRECTNESS TEST 1: Hadamard-all produces uniform superposition
// ============================================================================

static int test_hadamard_uniform(cuda_compute_ctx_t* ctx) {
    printf("\n[TEST 1] Hadamard-all on |0..0> -> uniform 1/sqrt(2^n)\n");
    int all_ok = 1;

    int qubit_sizes[] = {4, 8, 12, 16};
    for (int t = 0; t < 4; t++) {
        int n = qubit_sizes[t];
        uint32_t N = 1u << n;
        size_t bytes = (size_t)N * sizeof(cuda_complex_t);

        cuda_buffer_t* buf = cuda_buffer_create(ctx, bytes);
        if (!buf) { printf("  n=%2d: buffer alloc FAILED\n", n); return 0; }

        cuda_complex_t* amp = (cuda_complex_t*)cuda_buffer_contents(buf);
        for (uint32_t i = 0; i < N; i++) {
            amp[i].real = (i == 0) ? 1.0f : 0.0f;
            amp[i].imag = 0.0f;
        }

        cuda_hadamard_all(ctx, buf, n, N);
        cuda_wait_completion(ctx);

        float expected = (float)(1.0 / sqrt((double)N));
        double max_err = 0.0;
        for (uint32_t i = 0; i < N; i++) {
            double er = fabs((double)amp[i].real - (double)expected);
            double ei = fabs((double)amp[i].imag);
            if (er > max_err) max_err = er;
            if (ei > max_err) max_err = ei;
        }
        // Single-precision tolerance, scaled by number of gates applied.
        double tol = 1e-4;
        int ok = (max_err < tol);
        printf("  n=%2d  N=%-6u  expected=%.6f  max|err|=%.2e  %s\n",
               n, N, expected, max_err, ok ? "PASS" : "FAIL");
        if (!ok) all_ok = 0;

        cuda_buffer_free(buf);
    }
    return all_ok;
}

// ============================================================================
// CORRECTNESS TEST 2: Grover search finds a random target
// ============================================================================

static int test_grover_finds_target(cuda_compute_ctx_t* ctx) {
    printf("\n[TEST 2] Grover search finds a random target (GPU argmax == target)\n");
    int all_ok = 1;

    int qubit_sizes[] = {12, 14, 16};
    for (int t = 0; t < 3; t++) {
        int n = qubit_sizes[t];
        uint32_t N = 1u << n;
        uint32_t target = (uint32_t)(rand() % (int)N);
        int iterations = (int)(0.785398163 * sqrt((double)N)); // ~pi/4 * sqrt(N)

        size_t bytes = (size_t)N * sizeof(cuda_complex_t);
        cuda_buffer_t* buf = cuda_buffer_create(ctx, bytes);
        if (!buf) { printf("  n=%2d: buffer alloc FAILED\n", n); return 0; }

        cuda_grover_search(ctx, buf, target, n, N, iterations);
        cuda_wait_completion(ctx);

        // Argmax on host from resulting amplitudes.
        cuda_complex_t* amp = (cuda_complex_t*)cuda_buffer_contents(buf);
        uint32_t best = 0;
        float best_p = -1.0f, target_p = 0.0f, total_p = 0.0f;
        for (uint32_t i = 0; i < N; i++) {
            float p = amp[i].real * amp[i].real + amp[i].imag * amp[i].imag;
            total_p += p;
            if (i == target) target_p = p;
            if (p > best_p) { best_p = p; best = i; }
        }
        int ok = (best == target);
        printf("  n=%2d  target=%-6u  iters=%-4d  found=%-6u  P(target)=%.4f  %s\n",
               n, target, iterations, best, target_p / (total_p > 0 ? total_p : 1.0f),
               ok ? "PASS" : "FAIL");
        if (!ok) all_ok = 0;

        cuda_buffer_free(buf);
    }
    return all_ok;
}

// ============================================================================
// CORRECTNESS TEST 3: Batch search — one block per search finds each target
// ============================================================================

static int test_grover_batch(cuda_compute_ctx_t* ctx) {
    printf("\n[TEST 3] Batch Grover (one block/search) finds every target\n");
    int n = 12;
    uint32_t N = 1u << n;
    uint32_t num_searches = 32;
    int iterations = (int)(0.785398163 * sqrt((double)N));

    size_t bytes = (size_t)num_searches * N * sizeof(cuda_complex_t);
    cuda_buffer_t* buf = cuda_buffer_create(ctx, bytes);
    if (!buf) { printf("  batch buffer alloc FAILED\n"); return 0; }

    uint32_t* targets = (uint32_t*)malloc(num_searches * sizeof(uint32_t));
    uint32_t* results = (uint32_t*)malloc(num_searches * sizeof(uint32_t));
    for (uint32_t s = 0; s < num_searches; s++) targets[s] = (uint32_t)(rand() % (int)N);

    cuda_grover_batch_search(ctx, buf, targets, results, num_searches, n, iterations);
    cuda_wait_completion(ctx);

    uint32_t correct = 0;
    for (uint32_t s = 0; s < num_searches; s++) {
        if (results[s] == targets[s]) correct++;
    }
    int ok = (correct == num_searches);
    printf("  n=%d  searches=%u  iters=%d  correct=%u/%u  %s\n",
           n, num_searches, iterations, correct, num_searches, ok ? "PASS" : "FAIL");

    free(targets);
    free(results);
    cuda_buffer_free(buf);
    return ok;
}

// ============================================================================
// TIMING: honest GPU-vs-CPU Grover search
// ============================================================================

static void run_timing(cuda_compute_ctx_t* ctx) {
    printf("\n[TIMING] Grover search: measured GPU vs single-thread CPU reference\n");
    printf("%-8s  %-10s  %-14s  %-14s  %-10s\n",
           "Qubits", "Iters", "CPU (ms)", "GPU (ms)", "Speedup");
    printf("---------------------------------------------------------------------\n");

    int qubit_sizes[] = {12, 14, 16, 18, 20};
    int num_tests = (int)(sizeof(qubit_sizes) / sizeof(qubit_sizes[0]));

    for (int t = 0; t < num_tests; t++) {
        int n = qubit_sizes[t];
        uint32_t N = 1u << n;
        uint32_t target = 42u % N;
        int iterations = (int)(0.785398163 * sqrt((double)N));

        // CPU reference
        double cpu_elapsed = 0.0;
        uint32_t cpu_best = cpu_grover_search(n, target, iterations, &cpu_elapsed);

        // GPU (measured wall clock, including submission + sync)
        size_t bytes = (size_t)N * sizeof(cuda_complex_t);
        cuda_buffer_t* buf = cuda_buffer_create(ctx, bytes);
        if (!buf) { printf("%-8d  GPU alloc FAILED\n", n); continue; }

        double g0 = now_seconds();
        cuda_grover_search(ctx, buf, target, n, N, iterations);
        cuda_wait_completion(ctx);
        double gpu_elapsed = now_seconds() - g0;

        cuda_complex_t* amp = (cuda_complex_t*)cuda_buffer_contents(buf);
        uint32_t gpu_best = 0; float best_p = -1.0f;
        for (uint32_t i = 0; i < N; i++) {
            float p = amp[i].real * amp[i].real + amp[i].imag * amp[i].imag;
            if (p > best_p) { best_p = p; gpu_best = i; }
        }
        cuda_buffer_free(buf);

        const char* agree = (gpu_best == target && cpu_best == target) ? "" : "  (mismatch!)";
        double speedup = (gpu_elapsed > 0) ? cpu_elapsed / gpu_elapsed : 0.0;
        printf("%-8d  %-10d  %-14.3f  %-14.3f  %-8.2fx%s\n",
               n, iterations, cpu_elapsed * 1000.0, gpu_elapsed * 1000.0, speedup, agree);
    }

    printf("\nNote: GPU timing includes kernel launch + unified-memory sync overhead.\n");
    printf("Per-iteration this backend issues several small kernels; the honest\n");
    printf("advantage grows with qubit count (larger state vectors).\n");
}

// ============================================================================
// MAIN
// ============================================================================

int main(void) {
    printf("=====================================================================\n");
    printf("  CUDA GPU BENCHMARK + CORRECTNESS SELF-TEST (quantum_rng)\n");
    printf("=====================================================================\n");

    if (!cuda_is_available()) {
        fprintf(stderr, "ERROR: no CUDA device available on this system.\n");
        return 1;
    }

    cuda_compute_ctx_t* ctx = cuda_compute_init();
    if (!ctx) {
        fprintf(stderr, "ERROR: failed to initialize CUDA compute context.\n");
        return 1;
    }
    cuda_print_device_info(ctx);

    srand(12345u); // deterministic targets for reproducibility

    int ok1 = test_hadamard_uniform(ctx);
    int ok2 = test_grover_finds_target(ctx);
    int ok3 = test_grover_batch(ctx);

    run_timing(ctx);

    printf("\n=====================================================================\n");
    printf("  CORRECTNESS SUMMARY\n");
    printf("=====================================================================\n");
    printf("  Test 1 (Hadamard uniform):   %s\n", ok1 ? "PASS" : "FAIL");
    printf("  Test 2 (Grover finds target): %s\n", ok2 ? "PASS" : "FAIL");
    printf("  Test 3 (Batch Grover):        %s\n", ok3 ? "PASS" : "FAIL");

    int all_ok = ok1 && ok2 && ok3;
    printf("\n  OVERALL: %s\n", all_ok ? "ALL TESTS PASSED" : "SOME TESTS FAILED");

    cuda_compute_free(ctx);
    return all_ok ? 0 : 1;
}
