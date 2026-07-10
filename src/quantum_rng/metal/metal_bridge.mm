#import <Metal/Metal.h>
#import <Foundation/Foundation.h>
#include "metal_bridge.h"
#include <stdio.h>
#include <string.h>
#include <mach/mach_time.h>

/**
 * @file metal_bridge.mm
 * @brief Objective-C++ implementation of Metal GPU acceleration
 * 
 * Implements zero-copy unified memory architecture for M2 Ultra.
 * Optimized for 76 GPU cores with 192GB unified memory.
 */

// ============================================================================
// INTERNAL STRUCTURES
// ============================================================================

struct metal_compute_ctx {
    id<MTLDevice> device;
    id<MTLCommandQueue> commandQueue;
    id<MTLLibrary> library;
    
    // Compiled compute pipelines (cached for performance)
    id<MTLComputePipelineState> hadamardPipeline;
    id<MTLComputePipelineState> hadamardAllPipeline;
    id<MTLComputePipelineState> oraclePipeline;
    id<MTLComputePipelineState> oracleMultiPipeline;
    id<MTLComputePipelineState> diffusionPipeline;
    id<MTLComputePipelineState> batchSearchPipeline;
    id<MTLComputePipelineState> batchHadamardPipeline;
    
    id<MTLComputePipelineState> probabilitiesPipeline;
    id<MTLComputePipelineState> normalizePipeline;
    id<MTLComputePipelineState> pauliXPipeline;
    id<MTLComputePipelineState> pauliZPipeline;
    
    // Performance monitoring
    int performance_monitoring;
    double last_execution_time;
    
    // Error tracking
    NSString* lastError;
};

struct metal_buffer {
    id<MTLBuffer> buffer;
    size_t size;
};

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

static double get_time_seconds() {
    static mach_timebase_info_data_t timebase_info;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        mach_timebase_info(&timebase_info);
    });
    
    uint64_t time = mach_absolute_time();
    return (double)time * timebase_info.numer / timebase_info.denom / 1e9;
}

static void set_error(metal_compute_ctx_t* ctx, NSString* error) {
    if (ctx) {
        ctx->lastError = error;
    }
}

// ============================================================================
// INITIALIZATION
// ============================================================================

metal_compute_ctx_t* metal_compute_init(void) {
    @autoreleasepool {
        metal_compute_ctx_t* ctx = (metal_compute_ctx_t*)calloc(1, sizeof(metal_compute_ctx_t));
        if (!ctx) return NULL;
        
        // Get Metal device (should be M2 Ultra)
        ctx->device = MTLCreateSystemDefaultDevice();
        if (!ctx->device) {
            fprintf(stderr, "Metal: Failed to create device\n");
            free(ctx);
            return NULL;
        }
        
        // Print device info
        NSString* deviceName = [ctx->device name];
        printf("Metal: Initialized device: %s\n", [deviceName UTF8String]);
        printf("Metal: Unified memory: %llu MB\n", 
               [ctx->device recommendedMaxWorkingSetSize] / (1024 * 1024));
        
        // Create command queue
        ctx->commandQueue = [ctx->device newCommandQueue];
        if (!ctx->commandQueue) {
            fprintf(stderr, "Metal: Failed to create command queue\n");
            free(ctx);
            return NULL;
        }
        
        // Load shader library
        NSError* error = nil;
        NSString* shaderPath = @"src/quantum_rng/metal/quantum_kernels.metal";
        
        // Try to compile from source
        NSString* shaderSource = [NSString stringWithContentsOfFile:shaderPath
                                                           encoding:NSUTF8StringEncoding
                                                              error:&error];
        
        if (shaderSource) {
            ctx->library = [ctx->device newLibraryWithSource:shaderSource
                                                     options:nil
                                                       error:&error];
        }
        
        if (!ctx->library) {
            // Try default library (pre-compiled)
            ctx->library = [ctx->device newDefaultLibrary];
        }
        
        if (!ctx->library) {
            fprintf(stderr, "Metal: Failed to load shader library: %s\n",
                    [[error localizedDescription] UTF8String]);
            free(ctx);
            return NULL;
        }
        
        // Load batch kernels
        NSError* batchError = nil;
        NSString* batchPath = @"src/quantum_rng/metal/quantum_kernels_batch.metal";
        NSString* batchSource = [NSString stringWithContentsOfFile:batchPath
                                                           encoding:NSUTF8StringEncoding
                                                              error:&batchError];
        
        if (batchSource) {
            id<MTLLibrary> batchLibrary = [ctx->device newLibraryWithSource:batchSource
                                                                    options:nil
                                                                      error:&batchError];
            if (batchLibrary) {
                id<MTLFunction> batchFunc = [batchLibrary newFunctionWithName:@"grover_batch_search"];
                if (batchFunc) {
                    ctx->batchSearchPipeline = [ctx->device newComputePipelineStateWithFunction:batchFunc
                                                                                           error:&batchError];
                    if (!ctx->batchSearchPipeline) {
                        fprintf(stderr, "Metal: Failed to compile batch search pipeline: %s\n",
                                [[batchError localizedDescription] UTF8String]);
                    } else {
                        printf("Metal: Batch search pipeline compiled successfully\n");
                        printf("  Max threads per threadgroup: %lu\n",
                               (unsigned long)[ctx->batchSearchPipeline maxTotalThreadsPerThreadgroup]);
                        printf("  Threadgroup memory: %lu bytes\n",
                               (unsigned long)[ctx->batchSearchPipeline threadExecutionWidth]);
                    }
                } else {
                    fprintf(stderr, "Metal: Failed to find grover_batch_search function\n");
                }
                
                id<MTLFunction> batchHadamard = [batchLibrary newFunctionWithName:@"batch_hadamard_init"];
                if (batchHadamard) {
                    ctx->batchHadamardPipeline = [ctx->device newComputePipelineStateWithFunction:batchHadamard
                                                                                            error:&batchError];
                    if (!ctx->batchHadamardPipeline) {
                        fprintf(stderr, "Metal: Failed to compile batch Hadamard pipeline: %s\n",
                                [[batchError localizedDescription] UTF8String]);
                    } else {
                        printf("Metal: Batch Hadamard pipeline compiled successfully\n");
                    }
                } else {
                    fprintf(stderr, "Metal: Failed to find batch_hadamard_init function\n");
                }
            } else if (batchError) {
                fprintf(stderr, "Metal: Failed to compile batch library: %s\n",
                        [[batchError localizedDescription] UTF8String]);
            }
        }
        
        // Compile compute pipelines
        NSArray* kernelNames = @[
            @"hadamard_transform",
            @"hadamard_all_qubits",
            @"oracle_single_target",
            @"sparse_oracle",
            @"grover_diffusion_fused",
            @"compute_probabilities",
            @"normalize_state",
            @"pauli_x",
            @"pauli_z"
        ];
        
        id<MTLComputePipelineState>* pipelines[] = {
            &ctx->hadamardPipeline,
            &ctx->hadamardAllPipeline,
            &ctx->oraclePipeline,
            &ctx->oracleMultiPipeline,
            &ctx->diffusionPipeline,
            &ctx->probabilitiesPipeline,
            &ctx->normalizePipeline,
            &ctx->pauliXPipeline,
            &ctx->pauliZPipeline
        };
        
        for (size_t i = 0; i < [kernelNames count]; i++) {
            id<MTLFunction> function = [ctx->library newFunctionWithName:kernelNames[i]];
            if (!function) {
                fprintf(stderr, "Metal: Failed to load function: %s\n",
                        [kernelNames[i] UTF8String]);
                continue;
            }
            
            *pipelines[i] = [ctx->device newComputePipelineStateWithFunction:function
                                                                        error:&error];
            if (!*pipelines[i]) {
                fprintf(stderr, "Metal: Failed to create pipeline for %s: %s\n",
                        [kernelNames[i] UTF8String],
                        [[error localizedDescription] UTF8String]);
            }
        }
        
        ctx->performance_monitoring = 0;
        ctx->last_execution_time = 0.0;
        
        // Detect GPU cores
        uint32_t num_cores = 0;
        metal_get_device_info(ctx, NULL, NULL, &num_cores);
        
        printf("Metal: Compute pipelines compiled successfully\n");
        printf("Metal: Ready for GPU acceleration (%u cores)\n", num_cores);
        
        return ctx;
    }
}

void metal_compute_free(metal_compute_ctx_t* ctx) {
    if (!ctx) return;
    
    @autoreleasepool {
        // ARC will handle cleanup of Metal objects
        free(ctx);
    }
}

int metal_is_available(void) {
    @autoreleasepool {
        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        return device != nil;
    }
}

void metal_get_device_info(
    metal_compute_ctx_t* ctx,
    char* name,
    uint32_t* max_threads,
    uint32_t* num_cores
) {
    if (!ctx || !ctx->device) return;
    
    @autoreleasepool {
        if (name) {
            NSString* deviceName = [ctx->device name];
            strncpy(name, [deviceName UTF8String], 255);
            name[255] = '\0';
        }
        
        if (max_threads) {
            *max_threads = (uint32_t)[ctx->device maxThreadsPerThreadgroup].width;
        }
        
        if (num_cores) {
            // Estimate GPU cores based on device capabilities
            // This is a heuristic since Metal doesn't expose core count directly
            
            NSString* deviceName = [ctx->device name];
            uint64_t memorySize = [ctx->device recommendedMaxWorkingSetSize];
            
            // M-series GPU core estimates based on device configuration
            if ([deviceName containsString:@"M2 Ultra"]) {
                // M2 Ultra: 60 or 76 cores depending on configuration
                *num_cores = (memorySize > 100ULL * 1024 * 1024 * 1024) ? 76 : 60;
            } else if ([deviceName containsString:@"M2 Max"]) {
                // M2 Max: 30 or 38 cores
                *num_cores = (memorySize > 50ULL * 1024 * 1024 * 1024) ? 38 : 30;
            } else if ([deviceName containsString:@"M2 Pro"]) {
                // M2 Pro: 16 or 19 cores
                *num_cores = 19;
            } else if ([deviceName containsString:@"M2"]) {
                // Base M2: 8 or 10 cores
                *num_cores = 10;
            } else if ([deviceName containsString:@"M1 Ultra"]) {
                // M1 Ultra: 48 or 64 cores
                *num_cores = (memorySize > 100ULL * 1024 * 1024 * 1024) ? 64 : 48;
            } else if ([deviceName containsString:@"M1 Max"]) {
                // M1 Max: 24 or 32 cores
                *num_cores = 32;
            } else if ([deviceName containsString:@"M1 Pro"]) {
                // M1 Pro: 14 or 16 cores
                *num_cores = 16;
            } else if ([deviceName containsString:@"M1"]) {
                // Base M1: 7 or 8 cores
                *num_cores = 8;
            } else {
                // Fallback: estimate from recommended working set
                // Rough heuristic: ~2GB RAM per GPU core
                *num_cores = (uint32_t)(memorySize / (2ULL * 1024 * 1024 * 1024));
                if (*num_cores < 4) *num_cores = 4;  // Minimum reasonable estimate
                if (*num_cores > 128) *num_cores = 128;  // Maximum reasonable estimate
            }
        }
    }
}

// ============================================================================
// MEMORY MANAGEMENT
// ============================================================================

metal_buffer_t* metal_buffer_create(metal_compute_ctx_t* ctx, size_t size) {
    if (!ctx || !ctx->device || size == 0) return NULL;
    
    @autoreleasepool {
        metal_buffer_t* buffer = (metal_buffer_t*)malloc(sizeof(metal_buffer_t));
        if (!buffer) return NULL;
        
        // Create buffer with shared storage (zero-copy)
        buffer->buffer = [ctx->device newBufferWithLength:size
                                                   options:MTLResourceStorageModeShared];
        
        if (!buffer->buffer) {
            free(buffer);
            return NULL;
        }
        
        buffer->size = size;
        return buffer;
    }
}

metal_buffer_t* metal_buffer_create_from_data(
    metal_compute_ctx_t* ctx,
    void* data,
    size_t size
) {
    if (!ctx || !ctx->device || !data || size == 0) return NULL;
    
    @autoreleasepool {
        metal_buffer_t* buffer = (metal_buffer_t*)malloc(sizeof(metal_buffer_t));
        if (!buffer) return NULL;
        
        // Create buffer with existing data (zero-copy when possible)
        buffer->buffer = [ctx->device newBufferWithBytesNoCopy:data
                                                         length:size
                                                        options:MTLResourceStorageModeShared
                                                    deallocator:nil];
        
        if (!buffer->buffer) {
            // Fallback: copy data
            buffer->buffer = [ctx->device newBufferWithBytes:data
                                                      length:size
                                                     options:MTLResourceStorageModeShared];
        }
        
        if (!buffer->buffer) {
            free(buffer);
            return NULL;
        }
        
        buffer->size = size;
        return buffer;
    }
}

void* metal_buffer_contents(metal_buffer_t* buffer) {
    if (!buffer || !buffer->buffer) return NULL;
    
    @autoreleasepool {
        return [buffer->buffer contents];
    }
}

void metal_buffer_free(metal_buffer_t* buffer) {
    if (!buffer) return;
    
    @autoreleasepool {
        // ARC will handle buffer cleanup
        free(buffer);
    }
}

// ============================================================================
// KERNEL DISPATCH HELPER
// ============================================================================

static int dispatch_kernel(
    metal_compute_ctx_t* ctx,
    id<MTLComputePipelineState> pipeline,
    metal_buffer_t** buffers,
    size_t num_buffers,
    uint32_t* constants,
    size_t num_constants,
    uint32_t grid_size
) {
    if (!ctx || !pipeline) return -1;
    
    @autoreleasepool {
        double start_time = ctx->performance_monitoring ? get_time_seconds() : 0.0;
        
        id<MTLCommandBuffer> commandBuffer = [ctx->commandQueue commandBuffer];
        id<MTLComputeCommandEncoder> encoder = [commandBuffer computeCommandEncoder];
        
        [encoder setComputePipelineState:pipeline];
        
        // Bind buffers
        for (size_t i = 0; i < num_buffers; i++) {
            if (buffers[i] && buffers[i]->buffer) {
                [encoder setBuffer:buffers[i]->buffer offset:0 atIndex:i];
            }
        }
        
        // Bind constants
        for (size_t i = 0; i < num_constants; i++) {
            [encoder setBytes:&constants[i] length:sizeof(uint32_t) atIndex:num_buffers + i];
        }
        
        // Calculate threadgroup size and count
        NSUInteger maxThreads = [pipeline maxTotalThreadsPerThreadgroup];
        NSUInteger threadgroupSize = MIN(1024, maxThreads);  // Optimal for M2 Ultra
        NSUInteger threadgroups = (grid_size + threadgroupSize - 1) / threadgroupSize;
        
        MTLSize threadsPerThreadgroup = MTLSizeMake(threadgroupSize, 1, 1);
        MTLSize threadgroupCount = MTLSizeMake(threadgroups, 1, 1);
        
        [encoder dispatchThreadgroups:threadgroupCount
                threadsPerThreadgroup:threadsPerThreadgroup];
        
        [encoder endEncoding];
        [commandBuffer commit];
        [commandBuffer waitUntilCompleted];
        
        if (ctx->performance_monitoring) {
            ctx->last_execution_time = get_time_seconds() - start_time;
        }
        
        // Check for errors
        if ([commandBuffer status] == MTLCommandBufferStatusError) {
            set_error(ctx, @"Command buffer execution failed");
            return -1;
        }
        
        return 0;
    }
}

// ============================================================================
// QUANTUM GATE OPERATIONS
// ============================================================================

int metal_hadamard(
    metal_compute_ctx_t* ctx,
    metal_buffer_t* amplitudes,
    uint32_t qubit_index,
    uint32_t state_dim
) {
    if (!ctx || !amplitudes) return -1;
    
    metal_buffer_t* buffers[] = {amplitudes};
    uint32_t constants[] = {qubit_index, state_dim};
    
    uint32_t stride = 1u << qubit_index;
    uint32_t num_pairs = state_dim / 2;
    
    return dispatch_kernel(ctx, ctx->hadamardPipeline,
                          buffers, 1,
                          constants, 2,
                          num_pairs);
}

int metal_hadamard_all(
    metal_compute_ctx_t* ctx,
    metal_buffer_t* amplitudes,
    uint32_t num_qubits,
    uint32_t state_dim
) {
    if (!ctx || !amplitudes) return -1;
    
    metal_buffer_t* buffers[] = {amplitudes};
    uint32_t constants[] = {num_qubits, state_dim};
    
    return dispatch_kernel(ctx, ctx->hadamardAllPipeline,
                          buffers, 1,
                          constants, 2,
                          state_dim);
}

int metal_oracle(
    metal_compute_ctx_t* ctx,
    metal_buffer_t* amplitudes,
    uint32_t target_state,
    uint32_t state_dim
) {
    if (!ctx || !amplitudes) return -1;
    
    metal_buffer_t* buffers[] = {amplitudes};
    uint32_t constants[] = {target_state};
    
    return dispatch_kernel(ctx, ctx->oraclePipeline,
                          buffers, 1,
                          constants, 1,
                          state_dim);
}

int metal_oracle_multi(
    metal_compute_ctx_t* ctx,
    metal_buffer_t* amplitudes,
    const uint32_t* marked_states,
    uint32_t num_marked,
    uint32_t state_dim
) {
    if (!ctx || !amplitudes || !marked_states) return -1;
    
    // Create buffer for marked states
    metal_buffer_t* marked_buffer = metal_buffer_create_from_data(
        ctx, (void*)marked_states, num_marked * sizeof(uint32_t));
    
    if (!marked_buffer) return -1;
    
    metal_buffer_t* buffers[] = {amplitudes, marked_buffer};
    uint32_t constants[] = {num_marked, state_dim};
    
    int result = dispatch_kernel(ctx, ctx->oracleMultiPipeline,
                                buffers, 2,
                                constants, 2,
                                state_dim);
    
    metal_buffer_free(marked_buffer);
    return result;
}

int metal_grover_diffusion(
    metal_compute_ctx_t* ctx,
    metal_buffer_t* amplitudes,
    uint32_t num_qubits,
    uint32_t state_dim
) {
    if (!ctx || !amplitudes) return -1;
    
    // Create scratch buffer for average computation
    metal_buffer_t* scratch = metal_buffer_create(ctx, 8);  // 2 floats
    if (!scratch) return -1;
    
    metal_buffer_t* buffers[] = {amplitudes, scratch};
    uint32_t constants[] = {num_qubits, state_dim};
    
    int result = dispatch_kernel(ctx, ctx->diffusionPipeline,
                                buffers, 2,
                                constants, 2,
                                state_dim);
    
    metal_buffer_free(scratch);
    return result;
}

int metal_pauli_x(
    metal_compute_ctx_t* ctx,
    metal_buffer_t* amplitudes,
    uint32_t qubit_index,
    uint32_t state_dim
) {
    if (!ctx || !amplitudes) return -1;
    
    metal_buffer_t* buffers[] = {amplitudes};
    uint32_t constants[] = {qubit_index, state_dim};
    
    uint32_t num_pairs = state_dim / 2;
    return dispatch_kernel(ctx, ctx->pauliXPipeline,
                          buffers, 1,
                          constants, 2,
                          num_pairs);
}

int metal_pauli_z(
    metal_compute_ctx_t* ctx,
    metal_buffer_t* amplitudes,
    uint32_t qubit_index,
    uint32_t state_dim
) {
    if (!ctx || !amplitudes) return -1;
    
    metal_buffer_t* buffers[] = {amplitudes};
    uint32_t constants[] = {qubit_index, state_dim};
    
    return dispatch_kernel(ctx, ctx->pauliZPipeline,
                          buffers, 1,
                          constants, 2,
                          state_dim);
}

// ============================================================================
// PROBABILITY & MEASUREMENT
// ============================================================================

int metal_compute_probabilities(
    metal_compute_ctx_t* ctx,
    metal_buffer_t* amplitudes,
    metal_buffer_t* probabilities,
    uint32_t state_dim
) {
    if (!ctx || !amplitudes || !probabilities) return -1;
    
    metal_buffer_t* buffers[] = {amplitudes, probabilities};
    uint32_t constants[] = {state_dim};
    
    return dispatch_kernel(ctx, ctx->probabilitiesPipeline,
                          buffers, 2,
                          constants, 1,
                          state_dim);
}

int metal_normalize(
    metal_compute_ctx_t* ctx,
    metal_buffer_t* amplitudes,
    float norm,
    uint32_t state_dim
) {
    if (!ctx || !amplitudes) return -1;
    
    metal_buffer_t* buffers[] = {amplitudes};
    uint32_t constants[] = {*(uint32_t*)&norm, state_dim};  // Cast float to uint32 for passing
    
    return dispatch_kernel(ctx, ctx->normalizePipeline,
                          buffers, 1,
                          constants, 2,
                          state_dim);
}

// ============================================================================
// BATCH OPERATIONS
// ============================================================================

int metal_grover_iteration(
    metal_compute_ctx_t* ctx,
    metal_buffer_t* amplitudes,
    uint32_t target_state,
    uint32_t num_qubits,
    uint32_t state_dim
) {
    // Oracle
    if (metal_oracle(ctx, amplitudes, target_state, state_dim) != 0) {
        return -1;
    }
    
    // Diffusion
    return metal_grover_diffusion(ctx, amplitudes, num_qubits, state_dim);
}

int metal_grover_search(
    metal_compute_ctx_t* ctx,
    metal_buffer_t* amplitudes,
    uint32_t target_state,
    uint32_t num_qubits,
    uint32_t state_dim,
    uint32_t num_iterations
) {
    // Initialize: Apply Hadamard to all qubits
    if (metal_hadamard_all(ctx, amplitudes, num_qubits, state_dim) != 0) {
        return -1;
    }
    
    // Run Grover iterations
    for (uint32_t i = 0; i < num_iterations; i++) {
        if (metal_grover_iteration(ctx, amplitudes, target_state, num_qubits, state_dim) != 0) {
            return -1;
        }
    }
    
    return 0;
}

// ============================================================================

// ============================================================================
// BATCH PROCESSING (THE BREAKTHROUGH!)
// ============================================================================

int metal_grover_batch_search(
    metal_compute_ctx_t* ctx,
    metal_buffer_t* batch_states,
    const uint32_t* targets,
    uint32_t* results,
    uint32_t num_searches,
    uint32_t num_qubits,
    uint32_t num_iterations
) {
    if (!ctx || !batch_states || !targets || !results) return -1;
    if (num_searches == 0 || num_qubits == 0) return -1;
    
    @autoreleasepool {
        // Load batch kernel if not already loaded
        if (!ctx->batchSearchPipeline) {
            // Try to load from batch kernel file
            NSError* error = nil;
            NSString* batchPath = @"src/quantum_rng/metal/quantum_kernels_batch.metal";
            NSString* batchSource = [NSString stringWithContentsOfFile:batchPath
                                                               encoding:NSUTF8StringEncoding
                                                                  error:&error];
            
            if (batchSource) {
                id<MTLLibrary> batchLibrary = [ctx->device newLibraryWithSource:batchSource
                                                                        options:nil
                                                                          error:&error];
                if (batchLibrary) {
                    id<MTLFunction> function = [batchLibrary newFunctionWithName:@"grover_batch_search"];
                    if (function) {
                        ctx->batchSearchPipeline = [ctx->device newComputePipelineStateWithFunction:function
                                                                                               error:&error];
                    }
                }
            }
            
            if (!ctx->batchSearchPipeline) {
                fprintf(stderr, "Metal: Failed to load batch search kernel\n");
                return -1;
            }
        }
        
        double start_time = ctx->performance_monitoring ? get_time_seconds() : 0.0;
        
        // Create Metal buffers for targets and results
        metal_buffer_t* targets_buf = metal_buffer_create_from_data(
            ctx, (void*)targets, num_searches * sizeof(uint32_t));
        metal_buffer_t* results_buf = metal_buffer_create(
            ctx, num_searches * sizeof(uint32_t));
        
        if (!targets_buf || !results_buf) {
            if (targets_buf) metal_buffer_free(targets_buf);
            if (results_buf) metal_buffer_free(results_buf);
            return -1;
        }
        
        // Dispatch batch kernel
        id<MTLCommandBuffer> commandBuffer = [ctx->commandQueue commandBuffer];
        id<MTLComputeCommandEncoder> encoder = [commandBuffer computeCommandEncoder];
        
        [encoder setComputePipelineState:ctx->batchSearchPipeline];
        [encoder setBuffer:batch_states->buffer offset:0 atIndex:0];
        [encoder setBuffer:targets_buf->buffer offset:0 atIndex:1];
        [encoder setBuffer:results_buf->buffer offset:0 atIndex:2];
        [encoder setBytes:&num_searches length:sizeof(uint32_t) atIndex:3];
        [encoder setBytes:&num_qubits length:sizeof(uint32_t) atIndex:4];
        [encoder setBytes:&num_iterations length:sizeof(uint32_t) atIndex:5];
        
        // CRITICAL: One threadgroup per search!
        // Each threadgroup has 1024 threads to process its quantum state
        MTLSize threadsPerThreadgroup = MTLSizeMake(1024, 1, 1);
        MTLSize threadgroupCount = MTLSizeMake(num_searches, 1, 1);
        
        [encoder dispatchThreadgroups:threadgroupCount
                threadsPerThreadgroup:threadsPerThreadgroup];
        
        [encoder endEncoding];
        [commandBuffer commit];
        [commandBuffer waitUntilCompleted];
        
        if (ctx->performance_monitoring) {
            ctx->last_execution_time = get_time_seconds() - start_time;
        }
        
        // Copy results back
        uint32_t* results_ptr = (uint32_t*)metal_buffer_contents(results_buf);
        memcpy(results, results_ptr, num_searches * sizeof(uint32_t));
        
        metal_buffer_free(targets_buf);
        metal_buffer_free(results_buf);
        
        return ([commandBuffer status] == MTLCommandBufferStatusCompleted) ? 0 : -1;
    }
}
// SYNCHRONIZATION & UTILITIES
// ============================================================================

void metal_wait_completion(metal_compute_ctx_t* ctx) {
    if (!ctx || !ctx->commandQueue) return;
    
    @autoreleasepool {
        // Create a barrier to wait for all pending operations
        id<MTLCommandBuffer> commandBuffer = [ctx->commandQueue commandBuffer];
        [commandBuffer commit];
        [commandBuffer waitUntilCompleted];
    }
}

double metal_get_last_execution_time(metal_compute_ctx_t* ctx) {
    return ctx ? ctx->last_execution_time : 0.0;
}

void metal_set_performance_monitoring(metal_compute_ctx_t* ctx, int enable) {
    if (ctx) {
        ctx->performance_monitoring = enable;
    }
}

// ============================================================================
// DIAGNOSTICS
// ============================================================================

void metal_print_device_info(metal_compute_ctx_t* ctx) {
    if (!ctx || !ctx->device) return;
    
    @autoreleasepool {
        printf("\n");
        printf("╔═══════════════════════════════════════════════════════════╗\n");
        printf("║     METAL GPU DEVICE INFORMATION                          ║\n");
        printf("╠═══════════════════════════════════════════════════════════╣\n");
        printf("║                                                           ║\n");
        
        NSString* name = [ctx->device name];
        printf("║  Device: %-48s ║\n", [name UTF8String]);
        printf("║  Unified Memory: %7llu MB                             ║\n",
               [ctx->device recommendedMaxWorkingSetSize] / (1024 * 1024));
        printf("║  Max Threadgroup Size: %7lu                          ║\n",
               (unsigned long)[ctx->device maxThreadsPerThreadgroup].width);
        printf("║  GPU Cores (M2 Ultra): 76                                ║\n");
        printf("║  Memory Bandwidth: ~800 GB/s                             ║\n");
        printf("║  Storage Mode: MTLResourceStorageModeShared (zero-copy)  ║\n");
        printf("║                                                           ║\n");
        printf("╚═══════════════════════════════════════════════════════════╝\n");
        printf("\n");
    }
}

const char* metal_get_error(metal_compute_ctx_t* ctx) {
    if (!ctx || !ctx->lastError) return "No error";
    
    @autoreleasepool {
        return [ctx->lastError UTF8String];
    }
}