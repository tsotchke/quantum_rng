#include "simd_ops.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

// Platform detection
#ifdef __x86_64__
    #include <cpuid.h>
    #ifdef __SSE2__
        #include <emmintrin.h>  // SSE2
    #endif
    #ifdef __AVX2__
        #include <immintrin.h>  // AVX2
    #endif
#endif

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
    #include <arm_neon.h>
    #define NEON_AVAILABLE 1
#else
    #define NEON_AVAILABLE 0
#endif

/**
 * @file simd_ops.c
 * @brief SIMD-optimized operations for quantum RNG
 * 
 * Provides vectorized implementations with automatic fallback to scalar code.
 * Expected speedups:
 * - SSE2: 4-8x for complex operations
 * - AVX2: 8-16x for large operations
 * - ARM NEON: 4-8x on ARM processors
 */

// ============================================================================
// CPU FEATURE DETECTION
// ============================================================================

simd_capabilities_t simd_detect_capabilities(void) {
    simd_capabilities_t caps = {0};
    
#ifdef __x86_64__
    unsigned int eax, ebx, ecx, edx;
    
    // Basic features (CPUID function 1)
    if (__get_cpuid(1, &eax, &ebx, &ecx, &edx)) {
        caps.has_sse2 = (edx & (1 << 26)) != 0;
        caps.has_sse3 = (ecx & (1 << 0)) != 0;
        caps.has_ssse3 = (ecx & (1 << 9)) != 0;
        caps.has_sse4_1 = (ecx & (1 << 19)) != 0;
        caps.has_avx = (ecx & (1 << 28)) != 0;
        caps.has_fma = (ecx & (1 << 12)) != 0;
    }
    
    // Extended features (CPUID function 7)
    if (__get_cpuid_count(7, 0, &eax, &ebx, &ecx, &edx)) {
        caps.has_avx2 = (ebx & (1 << 5)) != 0;
    }
#elif defined(__aarch64__)
    // ARM NEON is mandatory on AArch64
    caps.has_sse2 = 1;  // Map to NEON capability
#endif
    
    return caps;
}

const char* simd_capabilities_string(const simd_capabilities_t *caps) {
    if (!caps) return "Unknown";
    
    static char buffer[256];
    buffer[0] = '\0';
    
#ifdef __x86_64__
    if (caps->has_avx2) strcat(buffer, "AVX2 ");
    if (caps->has_avx) strcat(buffer, "AVX ");
    if (caps->has_fma) strcat(buffer, "FMA ");
    if (caps->has_sse4_1) strcat(buffer, "SSE4.1 ");
    if (caps->has_ssse3) strcat(buffer, "SSSE3 ");
    if (caps->has_sse3) strcat(buffer, "SSE3 ");
    if (caps->has_sse2) strcat(buffer, "SSE2 ");
#elif defined(__aarch64__)
    if (caps->has_sse2) strcat(buffer, "ARM_NEON ");  // sse2 flag used for NEON
#endif
    
    if (buffer[0] == '\0') {
        return "Scalar only";
    }
    
    return buffer;
}

// ============================================================================
// VECTORIZED OPERATIONS
// ============================================================================

double simd_sum_squared_magnitudes(const complex_t *amplitudes, size_t n) {
    if (!amplitudes || n == 0) return 0.0;
    
#if defined(__AVX2__) && defined(__x86_64__) && !defined(__aarch64__)
    // AVX2 implementation (4 doubles at once)
    __m256d sum_vec = _mm256_setzero_pd();
    
    size_t i = 0;
    // Process 2 complex numbers (4 doubles) at a time
    for (; i + 1 < n; i += 2) {
        __m256d data = _mm256_loadu_pd((const double*)&amplitudes[i]);
        __m256d sq = _mm256_mul_pd(data, data);
        sum_vec = _mm256_add_pd(sum_vec, sq);
    }
    
    // Horizontal sum of 4 doubles
    __m128d sum_high = _mm256_extractf128_pd(sum_vec, 1);
    __m128d sum_low = _mm256_castpd256_pd128(sum_vec);
    __m128d sum128 = _mm_add_pd(sum_low, sum_high);
    __m128d sum_final = _mm_hadd_pd(sum128, sum128);
    
    double sum = _mm_cvtsd_f64(sum_final);
    
    // Handle remaining elements
    for (; i < n; i++) {
        double re = creal(amplitudes[i]);
        double im = cimag(amplitudes[i]);
        sum += re * re + im * im;
    }
    
    return sum;
    
#elif defined(__SSE2__) && defined(__x86_64__) && !defined(__aarch64__)
    // SSE2 implementation (2 doubles at once)
    __m128d sum_vec = _mm_setzero_pd();
    
    size_t i = 0;
    // Process 1 complex number (2 doubles) at a time
    for (; i < n; i++) {
        __m128d data = _mm_loadu_pd((const double*)&amplitudes[i]);
        __m128d sq = _mm_mul_pd(data, data);
        sum_vec = _mm_add_pd(sum_vec, sq);
    }
    
    // Horizontal sum
    __m128d sum_final = _mm_hadd_pd(sum_vec, sum_vec);
    return _mm_cvtsd_f64(sum_final);
    
#elif NEON_AVAILABLE
    // ARM NEON implementation (2 doubles per complex number)
    float64x2_t sum_vec = vdupq_n_f64(0.0);
    
    for (size_t i = 0; i < n; i++) {
        float64x2_t data = vld1q_f64((const double*)&amplitudes[i]);
        float64x2_t sq = vmulq_f64(data, data);
        sum_vec = vaddq_f64(sum_vec, sq);
    }
    
    // Extract and sum the two lanes
    return vgetq_lane_f64(sum_vec, 0) + vgetq_lane_f64(sum_vec, 1);
    
#else
    // Scalar fallback
    double sum = 0.0;
    for (size_t i = 0; i < n; i++) {
        double re = creal(amplitudes[i]);
        double im = cimag(amplitudes[i]);
        sum += re * re + im * im;
    }
    return sum;
#endif
}

void simd_normalize_amplitudes(complex_t *amplitudes, size_t n, double norm) {
    if (!amplitudes || n == 0 || norm == 0.0) return;
    
#if defined(__AVX2__) && defined(__x86_64__) && !defined(__aarch64__)
    // AVX2: Process 4 doubles (2 complex) at a time
    __m256d norm_vec = _mm256_set1_pd(norm);
    
    size_t i = 0;
    for (; i + 1 < n; i += 2) {
        __m256d data = _mm256_loadu_pd((double*)&amplitudes[i]);
        __m256d normalized = _mm256_div_pd(data, norm_vec);
        _mm256_storeu_pd((double*)&amplitudes[i], normalized);
    }
    
    // Handle remaining elements
    for (; i < n; i++) {
        amplitudes[i] /= norm;
    }
    
#elif defined(__SSE2__) && defined(__x86_64__) && !defined(__aarch64__)
    // SSE2: Process 2 doubles (1 complex) at a time
    __m128d norm_vec = _mm_set1_pd(norm);
    
    for (size_t i = 0; i < n; i++) {
        __m128d data = _mm_loadu_pd((double*)&amplitudes[i]);
        __m128d normalized = _mm_div_pd(data, norm_vec);
        _mm_storeu_pd((double*)&amplitudes[i], normalized);
    }
    
#elif NEON_AVAILABLE
    // ARM NEON implementation (2 doubles per complex number)
    float64x2_t norm_vec = vdupq_n_f64(norm);
    
    for (size_t i = 0; i < n; i++) {
        float64x2_t data = vld1q_f64((double*)&amplitudes[i]);
        float64x2_t normalized = vdivq_f64(data, norm_vec);
        vst1q_f64((double*)&amplitudes[i], normalized);
    }
    
#else
    // Scalar fallback
    for (size_t i = 0; i < n; i++) {
        amplitudes[i] /= norm;
    }
#endif
}

void simd_matrix2x2_vec_multiply(
    const complex_t matrix[4],
    const complex_t input[2],
    complex_t output[2]
) {
    if (!matrix || !input || !output) return;
    
#if defined(__SSE2__) && defined(__x86_64__) && !defined(__aarch64__)
    // Load matrix elements
    __m128d m00 = _mm_loadu_pd((const double*)&matrix[0]);  // a + bi
    __m128d m01 = _mm_loadu_pd((const double*)&matrix[1]);  // c + di
    __m128d m10 = _mm_loadu_pd((const double*)&matrix[2]);  // e + fi
    __m128d m11 = _mm_loadu_pd((const double*)&matrix[3]);  // g + hi
    
    // Load input
    __m128d v0 = _mm_loadu_pd((const double*)&input[0]);
    __m128d v1 = _mm_loadu_pd((const double*)&input[1]);
    
    // Complex multiplication: (a+bi)(c+di) = (ac-bd) + (ad+bc)i
    // For matrix-vector: out[0] = m00*v0 + m01*v1
    
    // m00 * v0
    __m128d v0_flip = _mm_shuffle_pd(v0, v0, 1);  // Swap real/imag
    __m128d m00_real = _mm_shuffle_pd(m00, m00, 0);  // Broadcast real
    __m128d m00_imag = _mm_shuffle_pd(m00, m00, 3);  // Broadcast imag
    
    __m128d prod00_real = _mm_mul_pd(m00_real, v0);
    __m128d prod00_imag = _mm_mul_pd(m00_imag, v0_flip);
    
    __m128d result0 = _mm_addsub_pd(prod00_real, prod00_imag);
    
    // m01 * v1 (similar)
    __m128d v1_flip = _mm_shuffle_pd(v1, v1, 1);
    __m128d m01_real = _mm_shuffle_pd(m01, m01, 0);
    __m128d m01_imag = _mm_shuffle_pd(m01, m01, 3);
    
    __m128d prod01_real = _mm_mul_pd(m01_real, v1);
    __m128d prod01_imag = _mm_mul_pd(m01_imag, v1_flip);
    
    result0 = _mm_add_pd(result0, _mm_addsub_pd(prod01_real, prod01_imag));
    
    _mm_storeu_pd((double*)&output[0], result0);
    
    // out[1] = m10*v0 + m11*v1
    __m128d m10_real = _mm_shuffle_pd(m10, m10, 0);
    __m128d m10_imag = _mm_shuffle_pd(m10, m10, 3);
    
    __m128d prod10_real = _mm_mul_pd(m10_real, v0);
    __m128d prod10_imag = _mm_mul_pd(m10_imag, v0_flip);
    
    __m128d result1 = _mm_addsub_pd(prod10_real, prod10_imag);
    
    __m128d m11_real = _mm_shuffle_pd(m11, m11, 0);
    __m128d m11_imag = _mm_shuffle_pd(m11, m11, 3);
    
    __m128d prod11_real = _mm_mul_pd(m11_real, v1);
    __m128d prod11_imag = _mm_mul_pd(m11_imag, v1_flip);
    
    result1 = _mm_add_pd(result1, _mm_addsub_pd(prod11_real, prod11_imag));
    
    _mm_storeu_pd((double*)&output[1], result1);
    
#else
    // Scalar fallback - standard complex multiplication
    output[0] = matrix[0] * input[0] + matrix[1] * input[1];
    output[1] = matrix[2] * input[0] + matrix[3] * input[1];
#endif
}

complex_t simd_complex_multiply(complex_t z1, complex_t z2) {
#if defined(__SSE2__) && defined(__x86_64__) && !defined(__aarch64__)
    __m128d a = _mm_loadu_pd((const double*)&z1);
    __m128d b = _mm_loadu_pd((const double*)&z2);
    
    // (a+bi)(c+di) = (ac-bd) + (ad+bc)i
    __m128d b_flip = _mm_shuffle_pd(b, b, 1);  // di, c
    __m128d a_real = _mm_shuffle_pd(a, a, 0);  // a, a
    __m128d a_imag = _mm_shuffle_pd(a, a, 3);  // b, b
    
    __m128d prod_real = _mm_mul_pd(a_real, b);      // ac, ad
    __m128d prod_imag = _mm_mul_pd(a_imag, b_flip); // bd, bc
    
    __m128d result = _mm_addsub_pd(prod_real, prod_imag);  // ac-bd, ad+bc
    
    complex_t output;
    _mm_storeu_pd((double*)&output, result);
    return output;
    
#else
    return z1 * z2;
#endif
}

void simd_compute_probabilities(
    const complex_t *amplitudes,
    double *probabilities,
    size_t n
) {
    if (!amplitudes || !probabilities || n == 0) return;
    
#if defined(__AVX2__) && defined(__x86_64__) && !defined(__aarch64__)
    // AVX2: Process 2 complex (4 doubles) at once
    size_t i = 0;
    for (; i + 1 < n; i += 2) {
        __m256d data = _mm256_loadu_pd((const double*)&amplitudes[i]);
        __m256d sq = _mm256_mul_pd(data, data);
        
        // Horizontal add within each complex number
        __m256d sum = _mm256_hadd_pd(sq, sq);
        
        // Extract probabilities
        double probs[4];
        _mm256_storeu_pd(probs, sum);
        probabilities[i] = probs[0];
        probabilities[i+1] = probs[2];
    }
    
    // Handle remaining
    for (; i < n; i++) {
        double re = creal(amplitudes[i]);
        double im = cimag(amplitudes[i]);
        probabilities[i] = re * re + im * im;
    }
    
#elif defined(__SSE2__) && defined(__x86_64__) && !defined(__aarch64__)
    // SSE2: Process 1 complex (2 doubles) at once
    for (size_t i = 0; i < n; i++) {
        __m128d data = _mm_loadu_pd((const double*)&amplitudes[i]);
        __m128d sq = _mm_mul_pd(data, data);
        __m128d sum = _mm_hadd_pd(sq, sq);
        probabilities[i] = _mm_cvtsd_f64(sum);
    }
    
#else
    // Scalar fallback
    for (size_t i = 0; i < n; i++) {
        double re = creal(amplitudes[i]);
        double im = cimag(amplitudes[i]);
        probabilities[i] = re * re + im * im;
    }
#endif
}

// ============================================================================
// ENTROPY MIXING OPERATIONS
// ============================================================================

void simd_xor_bytes(uint8_t *dest, const uint8_t *src, size_t n) {
    if (!dest || !src || n == 0) return;
    
#if defined(__AVX2__) && defined(__x86_64__) && !defined(__aarch64__)
    // AVX2: Process 32 bytes at once
    size_t i = 0;
    for (; i + 31 < n; i += 32) {
        __m256i d = _mm256_loadu_si256((const __m256i*)(dest + i));
        __m256i s = _mm256_loadu_si256((const __m256i*)(src + i));
        __m256i result = _mm256_xor_si256(d, s);
        _mm256_storeu_si256((__m256i*)(dest + i), result);
    }
    
    // Handle remaining bytes
    for (; i < n; i++) {
        dest[i] ^= src[i];
    }
    
#elif defined(__SSE2__) && defined(__x86_64__) && !defined(__aarch64__)
    // SSE2: Process 16 bytes at once
    size_t i = 0;
    for (; i + 15 < n; i += 16) {
        __m128i d = _mm_loadu_si128((const __m128i*)(dest + i));
        __m128i s = _mm_loadu_si128((const __m128i*)(src + i));
        __m128i result = _mm_xor_si128(d, s);
        _mm_storeu_si128((__m128i*)(dest + i), result);
    }
    
    // Handle remaining bytes
    for (; i < n; i++) {
        dest[i] ^= src[i];
    }
    
#else
    // Scalar fallback
    for (size_t i = 0; i < n; i++) {
        dest[i] ^= src[i];
    }
#endif
}

void simd_mix_entropy(
    const uint8_t *state,
    const uint8_t *entropy,
    uint8_t *output,
    size_t size
) {
    if (!state || !entropy || !output || size == 0) return;
    
    // Copy state to output, then XOR with entropy
    memcpy(output, state, size);
    simd_xor_bytes(output, entropy, size);
    
#if defined(__AVX2__) && !defined(__aarch64__)
    // Additional mixing with bit rotation (AVX2)
    size_t i = 0;
    for (; i + 31 < size; i += 32) {
        __m256i data = _mm256_loadu_si256((const __m256i*)(output + i));
        
        // Rotate left by 3 bits
        __m256i rotated = _mm256_or_si256(
            _mm256_slli_epi32(data, 3),
            _mm256_srli_epi32(data, 29)
        );
        
        // XOR with rotation
        __m256i mixed = _mm256_xor_si256(data, rotated);
        _mm256_storeu_si256((__m256i*)(output + i), mixed);
    }
    
    // Handle remaining with scalar
    for (; i < size; i++) {
        output[i] ^= (output[i] << 3) | (output[i] >> 5);
    }
    
#elif defined(__SSE2__) && defined(__x86_64__) && !defined(__aarch64__)
    // SSE2 version
    size_t i = 0;
    for (; i + 15 < size; i += 16) {
        __m128i data = _mm_loadu_si128((const __m128i*)(output + i));
        __m128i rotated = _mm_or_si128(
            _mm_slli_epi32(data, 3),
            _mm_srli_epi32(data, 29)
        );
        __m128i mixed = _mm_xor_si128(data, rotated);
        _mm_storeu_si128((__m128i*)(output + i), mixed);
    }
    
    for (; i < size; i++) {
        output[i] ^= (output[i] << 3) | (output[i] >> 5);
    }
    
#else
    // Scalar mixing with rotation
    for (size_t i = 0; i < size; i++) {
        output[i] ^= (output[i] << 3) | (output[i] >> 5);
    }
#endif
}

// ============================================================================
// QUANTUM GATE PRIMITIVES (SIMD-OPTIMIZED)
// ============================================================================

void simd_complex_swap(complex_t *amp0, complex_t *amp1, size_t n) {
    if (!amp0 || !amp1 || n == 0) return;
    
#if NEON_AVAILABLE
    // ARM NEON: Process 1 complex number (2 doubles) at a time
    for (size_t i = 0; i < n; i++) {
        float64x2_t a = vld1q_f64((double*)&amp0[i]);
        float64x2_t b = vld1q_f64((double*)&amp1[i]);
        
        vst1q_f64((double*)&amp0[i], b);
        vst1q_f64((double*)&amp1[i], a);
    }
    
#elif defined(__SSE2__) && defined(__x86_64__)
    // SSE2: Process 1 complex number (2 doubles) at a time
    for (size_t i = 0; i < n; i++) {
        __m128d a = _mm_loadu_pd((double*)&amp0[i]);
        __m128d b = _mm_loadu_pd((double*)&amp1[i]);
        
        _mm_storeu_pd((double*)&amp0[i], b);
        _mm_storeu_pd((double*)&amp1[i], a);
    }
    
#else
    // Scalar fallback
    for (size_t i = 0; i < n; i++) {
        complex_t temp = amp0[i];
        amp0[i] = amp1[i];
        amp1[i] = temp;
    }
#endif
}

void simd_multiply_by_i(complex_t *amplitudes, size_t n, int negate) {
    if (!amplitudes || n == 0) return;
    
#if NEON_AVAILABLE
    // ARM NEON: Multiply by ±i = swap real/imag and negate real
    // (a + bi) * i = -b + ai
    // (a + bi) * (-i) = b - ai
    
    for (size_t i = 0; i < n; i++) {
        float64x2_t data = vld1q_f64((double*)&amplitudes[i]);
        
        // Swap real and imaginary parts: [re, im] -> [im, re]
        float64x2_t swapped = vextq_f64(data, data, 1);
        
        // Negate first lane (was real, now im position)
        float64x2_t neg_mask = {-1.0, 1.0};
        if (negate) {
            neg_mask = (float64x2_t){1.0, -1.0};
        }
        
        float64x2_t result = vmulq_f64(swapped, neg_mask);
        vst1q_f64((double*)&amplitudes[i], result);
    }
    
#elif defined(__SSE2__) && defined(__x86_64__)
    // SSE2: Similar approach
    for (size_t i = 0; i < n; i++) {
        __m128d data = _mm_loadu_pd((double*)&amplitudes[i]);
        
        // Swap real and imaginary
        __m128d swapped = _mm_shuffle_pd(data, data, 1);
        
        // Negate first element
        __m128d neg_mask = negate ?
            _mm_set_pd(-1.0, 1.0) :
            _mm_set_pd(1.0, -1.0);
        
        __m128d result = _mm_mul_pd(swapped, neg_mask);
        _mm_storeu_pd((double*)&amplitudes[i], result);
    }
    
#else
    // Scalar fallback
    complex_t factor = negate ? -I : I;
    for (size_t i = 0; i < n; i++) {
        amplitudes[i] *= factor;
    }
#endif
}

void simd_negate(complex_t *amplitudes, size_t n) {
    if (!amplitudes || n == 0) return;
    
#if NEON_AVAILABLE
    // ARM NEON: Negate both real and imaginary parts
    float64x2_t neg_one = vdupq_n_f64(-1.0);
    
    for (size_t i = 0; i < n; i++) {
        float64x2_t data = vld1q_f64((double*)&amplitudes[i]);
        float64x2_t result = vmulq_f64(data, neg_one);
        vst1q_f64((double*)&amplitudes[i], result);
    }
    
#elif defined(__SSE2__) && defined(__x86_64__)
    // SSE2: Negate both components
    __m128d neg_one = _mm_set1_pd(-1.0);
    
    for (size_t i = 0; i < n; i++) {
        __m128d data = _mm_loadu_pd((double*)&amplitudes[i]);
        __m128d result = _mm_mul_pd(data, neg_one);
        _mm_storeu_pd((double*)&amplitudes[i], result);
    }
    
#else
    // Scalar fallback
    for (size_t i = 0; i < n; i++) {
        amplitudes[i] = -amplitudes[i];
    }
#endif
}

void simd_apply_phase(complex_t *amplitudes, complex_t phase, size_t n) {
    if (!amplitudes || n == 0) return;
    
#if NEON_AVAILABLE
    // ARM NEON: Complex multiplication by phase
    // (a+bi)(c+di) = (ac-bd) + (ad+bc)i
    
    double phase_re = creal(phase);
    double phase_im = cimag(phase);
    
    float64x2_t phase_real_vec = vdupq_n_f64(phase_re);  // [c, c]
    float64x2_t phase_imag_vec = vdupq_n_f64(phase_im);  // [d, d]
    
    for (size_t i = 0; i < n; i++) {
        float64x2_t amp = vld1q_f64((double*)&amplitudes[i]);  // [a, b]
        
        // Compute ac and ad
        float64x2_t prod_real = vmulq_f64(amp, phase_real_vec);  // [ac, bc]
        
        // Swap amplitude components for second product: [b, a]
        float64x2_t amp_swap = vextq_f64(amp, amp, 1);
        
        // Compute bd and ad
        float64x2_t prod_imag = vmulq_f64(amp_swap, phase_imag_vec);  // [bd, ad]
        
        // Result: [ac-bd, bc+ad] = [ac-bd, ad+bc]
        // Need to negate the bd term in first position
        float64x2_t neg_mask = {-1.0, 1.0};
        prod_imag = vmulq_f64(prod_imag, neg_mask);  // [-bd, ad]
        
        float64x2_t result = vaddq_f64(prod_real, prod_imag);  // [ac-bd, bc+ad]
        vst1q_f64((double*)&amplitudes[i], result);
    }
    
#elif defined(__SSE2__) && defined(__x86_64__)
    // SSE2: Use existing simd_complex_multiply logic
    for (size_t i = 0; i < n; i++) {
        amplitudes[i] = simd_complex_multiply(amplitudes[i], phase);
    }
    
#else
    // Scalar fallback
    for (size_t i = 0; i < n; i++) {
        amplitudes[i] *= phase;
    }
#endif
}

// ============================================================================
// QUANTUM MEASUREMENT OPTIMIZATIONS (CRITICAL FOR GROVER)
// ============================================================================

uint64_t simd_cumulative_probability_search(
    const complex_t *amplitudes,
    size_t n,
    double random_threshold
) {
    if (!amplitudes || n == 0) return 0;
    
    double cumulative = 0.0;
    
#if defined(__ARM_NEON) || defined(__aarch64__)
    // ARM NEON: Process 2 complex numbers (4 doubles) at once
    size_t i = 0;
    
    for (; i + 1 < n; i += 2) {
        // Load 2 complex numbers
        float64x2_t amp0 = vld1q_f64((const double*)&amplitudes[i]);
        float64x2_t amp1 = vld1q_f64((const double*)&amplitudes[i+1]);
        
        // Compute |amp|^2 for both
        float64x2_t mag0_sq = vmulq_f64(amp0, amp0);
        float64x2_t mag1_sq = vmulq_f64(amp1, amp1);
        
        // Horizontal add to get probabilities
        double prob0 = vgetq_lane_f64(mag0_sq, 0) + vgetq_lane_f64(mag0_sq, 1);
        double prob1 = vgetq_lane_f64(mag1_sq, 0) + vgetq_lane_f64(mag1_sq, 1);
        
        cumulative += prob0;
        if (random_threshold < cumulative) return i;
        
        cumulative += prob1;
        if (random_threshold < cumulative) return i + 1;
    }
    
    // Handle remainder
    for (; i < n; i++) {
        double re = creal(amplitudes[i]);
        double im = cimag(amplitudes[i]);
        cumulative += re * re + im * im;
        if (random_threshold < cumulative) return i;
    }
    
#elif defined(__AVX2__) && defined(__x86_64__)
    // AVX2: Process 2 complex numbers at once
    size_t i = 0;
    
    for (; i + 1 < n; i += 2) {
        __m256d data = _mm256_loadu_pd((const double*)&amplitudes[i]);
        __m256d sq = _mm256_mul_pd(data, data);
        __m256d sum = _mm256_hadd_pd(sq, sq);
        
        double probs[4];
        _mm256_storeu_pd(probs, sum);
        
        cumulative += probs[0];
        if (random_threshold < cumulative) return i;
        
        cumulative += probs[2];
        if (random_threshold < cumulative) return i + 1;
    }
    
    // Handle remainder
    for (; i < n; i++) {
        double re = creal(amplitudes[i]);
        double im = cimag(amplitudes[i]);
        cumulative += re * re + im * im;
        if (random_threshold < cumulative) return i;
    }
    
#else
    // Scalar fallback
    for (size_t i = 0; i < n; i++) {
        double re = creal(amplitudes[i]);
        double im = cimag(amplitudes[i]);
        cumulative += re * re + im * im;
        if (random_threshold < cumulative) return i;
    }
#endif
    
    return n - 1;  // Fallback
}

uint64_t simd_fast_measurement_sample(
    const complex_t *amplitudes,
    size_t n,
    double random_threshold
) {
    // For now, use the cumulative search (already optimized)
    // Could be further optimized with probability pre-computation if needed
    return simd_cumulative_probability_search(amplitudes, n, random_threshold);
}