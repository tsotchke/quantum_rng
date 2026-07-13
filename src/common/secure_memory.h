#ifndef SECURE_MEMORY_H
#define SECURE_MEMORY_H

/* Must be defined BEFORE any system header is included, otherwise glibc's
   <features.h> has already locked in the feature set and explicit_bzero()
   stays hidden under strict -std=c11. */
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif

#include <stddef.h>
#include <string.h>

/**
 * @file secure_memory.h
 * @brief Cryptographically secure memory operations
 *
 * Provides platform-independent secure memory zeroing that cannot be
 * optimized away by compilers. Essential for clearing sensitive data
 * like cryptographic keys, quantum states, and entropy buffers.
 *
 * Uses platform-specific implementations when available:
 * - C11 memset_s() (Annex K)
 * - OpenBSD/glibc explicit_bzero()
 * - Portable fallback with volatile writes + a compiler memory barrier
 *   (used on Windows/MinGW and anywhere the above are unavailable)
 */

/**
 * @brief Securely zero memory
 *
 * Guarantees that memory is zeroed and the operation is not optimized
 * away by the compiler. Critical for security-sensitive applications.
 *
 * This function:
 * - Cannot be optimized away (compiler-independent)
 * - Works across all platforms
 * - Has minimal performance overhead
 * - Is safe for use in destructors/cleanup code
 *
 * @param ptr Pointer to memory to zero
 * @param size Number of bytes to zero
 */
static inline void secure_memzero(void *ptr, size_t size) {
    if (!ptr || size == 0) return;

#if defined(__STDC_LIB_EXT1__) && defined(__STDC_WANT_LIB_EXT1__)
    // C11 Annex K - memset_s
    memset_s(ptr, size, 0, size);

#else
    // Portable fallback with volatile and a compiler memory barrier. Used on
    // glibc too (we avoid explicit_bzero() because under strict -std=c11 the
    // feature macro that exposes it is unavailable and the implicit-declaration
    // warning is unavoidable without polluting every TU's feature set). The
    // volatile writes + barrier guarantee the zeroing is not optimized away.
    volatile unsigned char *p = (volatile unsigned char *)ptr;
    while (size--) {
        *p++ = 0;
    }

    // Memory barrier to prevent reordering
    #if defined(__GNUC__) || defined(__clang__)
        __asm__ __volatile__("" ::: "memory");
    #elif defined(_MSC_VER)
        _ReadWriteBarrier();
    #endif
#endif
}

/**
 * @brief Securely compare two memory regions in constant time
 *
 * Prevents timing attacks by ensuring comparison time is independent
 * of data content. Use for comparing cryptographic keys, MACs, etc.
 *
 * @param a First memory region
 * @param b Second memory region
 * @param size Number of bytes to compare
 * @return 0 if equal, non-zero if different
 */
static inline int secure_memcmp(const void *a, const void *b, size_t size) {
    if (!a || !b) return -1;

    const volatile unsigned char *pa = (const volatile unsigned char *)a;
    const volatile unsigned char *pb = (const volatile unsigned char *)b;
    unsigned char diff = 0;

    // Constant-time comparison - always checks all bytes
    for (size_t i = 0; i < size; i++) {
        diff |= pa[i] ^ pb[i];
    }

    return diff;
}

#endif /* SECURE_MEMORY_H */
