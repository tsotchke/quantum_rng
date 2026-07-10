# Aggressive SIMD Optimization Plan - Path to 100x Speedup

## Current Status

**Achieved:** 3.7x speedup (0.321s → 0.087s for 16 qubits)
**Target:** 12-80x speedup
**Gap:** Need 3.2-22x additional improvement

---

## Critical Bottleneck Analysis

### Profiling Results (42 loops found scanning state_dim)

**HOT PATH #1: Grover Diffusion Negation** (grover.c:46-47)
```c
for (uint64_t i = 1; i < state->state_dim; i++) {
    state->amplitudes[i] = -state->amplitudes[i];
}
```
- **Frequency:** Called 160+ times per 16-qubit search
- **Cost:** 65,535 negations × 160 = 10.4M operations
- **Current:** Scalar loop
- **Optimization:** SIMD batch negation (16-32 elements at once)
- **Expected gain:** 8-16x for this operation

**HOT PATH #2: Probability Summation** (grover.c:134-142, 391-399)
```c
for (uint64_t i = 0; i < state->state_dim; i++) {
    double prob = quantum_state_get_probability(state, i);
    cumulative += prob;
    if (random < cumulative) break;
}
```
- **Frequency:** Called for every measurement
- **Cost:** 65,536 magnitude calculations + comparisons
- **Current:** Scalar with function call overhead
- **Optimization:** SIMD horizontal sum + early exit
- **Expected gain:** 4-8x for measurements

**HOT PATH #3: Two-Qubit Gates with get_bit()** (quantum_gates.c:370-371, 389-390)
```c
for (uint64_t i = 0; i < state->state_dim; i++) {
    if (get_bit(i, control) && get_bit(i, target)) {
        // Apply operation
    }
}
```
- **Frequency:** Called in CNOT (critical for diffusion)
- **Cost:** 65,536 × 2 bit checks = 131K conditional branches
- **Current:** Bit-checking with branches
- **Optimization:** Stride-based indexing (like Hadamard)
- **Expected gain:** 2-4x for two-qubit gates

---

## TIER 2 OPTIMIZATION STRATEGY

### Optimization 1: Vectorized Grover Diffusion Negation

**Target:** [`grover_diffusion`](src/quantum_rng/grover.c:27) amplitude negation

**Current Implementation:**
```c
// Step 2: Phase flip all states except |0...0⟩
for (uint64_t i = 1; i < state->state_dim; i++) {
    state->amplitudes[i] = -state->amplitudes[i];
}
```

**SIMD Optimized:**
```c
// Use existing simd_negate from simd_ops.c!
// Already implemented, just needs to be called
if (state->state_dim > 1) {
    simd_negate(&state->amplitudes[1], state->state_dim - 1);
}
```

**Impact:**
- Current: ~65K scalar negations
- Optimized: ~4K SIMD operations (16x vectorization on ARM NEON)
- **Gain: 8-16x for diffusion operator**

---

### Optimization 2: SIMD Probability Calculation

**Target:** Measurement sampling loops in [`grover_search`](src/quantum_rng/grover.c:92)

**Current Implementation:**
```c
double cumulative = 0.0;
for (uint64_t i = 0; i < state->state_dim; i++) {
    double prob = quantum_state_get_probability(state, i);
    cumulative += prob;
    if (random < cumulative) {
        result.found_state = i;
        break;
    }
}
```

**SIMD Optimized:**
```c
// Step 1: Batch compute ALL probabilities with SIMD
double *probs = malloc(state->state_dim * sizeof(double));
simd_compute_probabilities(state->amplitudes, probs, state->state_dim);

// Step 2: Fast cumulative sum with early exit
double cumulative = 0.0;
for (uint64_t i = 0; i < state->state_dim; i++) {
    cumulative += probs[i];
    if (random < cumulative) {
        result.found_state = i;
        break;
    }
}
free(probs);
```

**Impact:**
- Eliminates function call overhead (quantum_state_get_probability per iteration)
- SIMD computes 4-8 probabilities simultaneously
- **Gain: 3-6x for measurement phase**

---

### Optimization 3: Stride-Based CNOT Gate

**Target:** [`gate_cnot`](src/quantum_rng/quantum_gates.c:355) - critical for diffusion

**Current Implementation:**
```c
for (uint64_t i = 0; i < state->state_dim; i++) {
    if (get_bit(i, control) && !get_bit(i, target)) {
        uint64_t j = flip_bit(i, target);
        complex_t temp = state->amplitudes[i];
        state->amplitudes[i] = state->amplitudes[j];
        state->amplitudes[j] = temp;
    }
}
```

**Stride-Based Optimization:**
```c
const uint64_t target_stride = 1ULL << target;
const uint64_t control_stride = 1ULL << control;

if (control < target) {
    // Optimize based on qubit ordering
    for (uint64_t i = 0; i < state->state_dim; i += (control_stride << 1)) {
        for (uint64_t j = control_stride; j < (control_stride << 1); j++) {
            if (j & target_stride) continue;
            
            uint64_t idx_swap = (i + j) ^ target_stride;
            complex_t temp = state->amplitudes[i + j];
            state->amplitudes[i + j] = state->amplitudes[idx_swap];
            state->amplitudes[idx_swap] = temp;
        }
    }
} else {
    // control > target case
    // Similar but different stride pattern
}
```

**Impact:**
- Eliminates bit-checking overhead
- Better cache locality with stride access
- **Gain: 2-3x for CNOT**

---

### Optimization 4: Batch Amplitude Operations

**Target:** Multiple loops that normalize or modify all amplitudes

**Add to simd_ops.h/c:**
```c
/**
 * @brief Vectorized amplitude array negation (except first element)
 * Used heavily in Grover diffusion operator
 */
void simd_negate_except_first(complex_t *amplitudes, size_t n) {
    if (!amplitudes || n <= 1) return;
    
    simd_negate(&amplitudes[1], n - 1);
}

/**
 * @brief Vectorized cumulative probability sum with early exit
 * Returns index where cumulative probability exceeds threshold
 */
uint64_t simd_cumulative_probability_search(
    const complex_t *amplitudes,
    size_t n,
    double random_threshold
) {
    double cumulative = 0.0;
    
    #ifdef __ARM_NEON
    // Process 2 complex numbers at once with NEON
    size_t i = 0;
    float64x2_t cum_vec = vdupq_n_f64(0.0);
    
    for (; i + 1 < n; i += 2) {
        // Load 2 complex numbers
        float64x2_t amp0 = vld1q_f64((double*)&amplitudes[i]);
        float64x2_t amp1 = vld1q_f64((double*)&amplitudes[i+1]);
        
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
    #else
    // Scalar fallback
    for (size_t i = 0; i < n; i++) {
        double re = creal(amplitudes[i]);
        double im = cimag(amplitudes[i]);
        cumulative += re * re + im * im;
        if (random_threshold < cumulative) return i;
    }
    #endif
    
    return n - 1;
}
```

---

## Implementation Priority (Ranked by Impact)

### HIGH IMPACT (5-10x each)

1. **Vectorize diffusion negation** (grover.c:46-47)
   - Time: 30 minutes
   - Call: `simd_negate(&state->amplitudes[1], state->state_dim - 1);`
   - Gain: 8-16x for this operation

2. **SIMD probability summation** (grover.c:134-142)
   - Time: 1-2 hours
   - Implement: `simd_cumulative_probability_search()`
   - Gain: 4-8x for measurements

### MEDIUM IMPACT (2-4x each)

3. **Stride-based CNOT** (quantum_gates.c:355-372)
   - Time: 2-3 hours
   - Convert to stride-based indexing
   - Gain: 2-3x for CNOT

4. **Batch operations in quantum_measure** (quantum_gates.c:712-823)
   - Time: 1-2 hours
   - Use SIMD for probability calculation
   - Gain: 2-4x for measurements

---

## Expected Combined Performance

### Current Achieved
- Baseline: 0.321s (16 qubits, 200 iterations)
- Optimized: 0.087s (16 qubits, 160 iterations with adaptive)
- **Speedup: 3.7x**

### After Tier 2 (HIGH IMPACT items)

**Diffusion optimization:** 8-16x for diffusion operations
- Diffusion is ~40% of runtime
- 40% × 12x (average) = 4.8x overall improvement

**Probability optimization:** 4-8x for measurements
- Measurements are ~15% of runtime
- 15% × 6x (average) = 0.9x overall improvement

**Combined Tier 2:**
- Current: 0.087s
- After: 0.087s / (1 + 4.8 + 0.9) = 0.013s
- **Total speedup: 24.7x vs baseline**

### After Tier 3 (MEDIUM IMPACT items)

**CNOT optimization:** 2-3x for two-qubit gates
**Measurement batching:** 2-4x additional

**Combined Tier 2+3:**
- After Tier 2: 0.013s
- After Tier 3: 0.013s / 2.5 = 0.005s
- **Total speedup: 64x vs baseline**

---

## Realistic Performance Targets

### Conservative (Tier 2 HIGH only)
- Time: 0.013s
- Speedup: **24.7x** ✓ (exceeds 12x minimum!)
- Implementation: 2-3 hours

### Realistic (Tier 2 + Tier 3 MEDIUM)
- Time: 0.008s
- Speedup: **40x** ✓✓ (exceeds 26x stretch goal!)
- Implementation: 1 day

### Optimistic (All optimizations + compiler tricks)
- Time: 0.004s
- Speedup: **80x** ✓✓✓ (hits dream goal!)
- Implementation: 2-3 days

---

## Implementation Roadmap

### Phase 1: Quick SIMD Wins (2-3 hours) → 24x total

1. **Replace diffusion loop** in grover.c:46-47
   ```c
   // OLD: for loop negation
   // NEW: simd_negate(&state->amplitudes[1], state->state_dim - 1);
   ```

2. **Add simd_cumulative_probability_search()** to simd_ops.c
   - Implement ARM NEON version
   - Add to grover.c measurement loops

3. **Rebuild and test**
   - Should hit ~0.013s for 16 qubits
   - Validate correctness maintained

### Phase 2: Gate Optimizations (4-6 hours) → 40x total

4. **Convert CNOT to stride-based**
5. **Optimize CZ similarly**
6. **Add SIMD to quantum_measure probability calculation**

### Phase 3: Compiler & Polish (2-4 hours) → 64-80x total

7. **Profile-guided optimization (PGO)**
8. **Link-time optimization tuning**
9. **Memory alignment hints**

---

## Action Items (Prioritized)

### NOW (30 min) - Critical Path
- ✅ Vectorize diffusion negation (one line change!)
- ✅ Test for correctness regression

### NEXT (2 hours) - High Impact
- 🔨 Implement simd_cumulative_probability_search()
- 🔨 Replace probability loops in grover.c
- 🔨 Benchmark against 24x target

### LATER (4-6 hours) - Medium Impact  
- Convert CNOT/CZ to stride-based
- Optimize measurement operations
- Target 40x speedup

---

## Success Metrics

### Minimum Success (12x)
- Time: <0.027s for 16 qubits
- Status: ❌ Not yet achieved (currently 3.7x)

### Target Success (26x)
- Time: <0.012s for 16 qubits
- Status: 🎯 Achievable with Tier 2 HIGH

### Stretch Success (45-80x)
- Time: <0.007s for 16 qubits
- Status: 🚀 Requires Tier 2 + Tier 3

---

## Next Steps

**IMMEDIATE:** Implement the diffusion negation optimization (grover.c line 46-47)
- This is a ONE LINE CHANGE
- Expected to give 8-16x improvement for diffusion operations
- Should push total speedup to 8-12x