#include "matrix_math.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define JACOBI_TOLERANCE 1e-12
#define SMALL_NUMBER 1e-15

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

static inline double complex_abs_squared(complex_t z) {
    double r = creal(z);
    double i = cimag(z);
    return r * r + i * i;
}

static inline void givens_rotation(
    complex_t *matrix,
    size_t n,
    size_t p,
    size_t q,
    double *c,
    double *s
) {
    // Compute Givens rotation to zero out matrix[p][q]
    complex_t app = matrix[p * n + p];
    complex_t aqq = matrix[q * n + q];
    complex_t apq = matrix[p * n + q];
    
    double theta;
    if (fabs(creal(apq)) < SMALL_NUMBER && fabs(cimag(apq)) < SMALL_NUMBER) {
        *c = 1.0;
        *s = 0.0;
        return;
    }
    
    double tau = (creal(aqq) - creal(app)) / (2.0 * creal(apq));
    double t;
    
    if (tau >= 0) {
        t = 1.0 / (tau + sqrt(1.0 + tau * tau));
    } else {
        t = -1.0 / (-tau + sqrt(1.0 + tau * tau));
    }
    
    *c = 1.0 / sqrt(1.0 + t * t);
    *s = t * (*c);
}

// ============================================================================
// EIGENVALUE DECOMPOSITION
// ============================================================================

int hermitian_eigen_decomposition(
    const complex_t *matrix,
    size_t n,
    double *eigenvalues,
    complex_t *eigenvectors,
    int max_iterations,
    double tolerance
) {
    if (!matrix || !eigenvalues || !eigenvectors) return -1;
    if (n == 0) return -1;
    
    if (max_iterations <= 0) {
        max_iterations = 50 * n * n;
    }
    if (tolerance <= 0) {
        tolerance = JACOBI_TOLERANCE;
    }
    
    // Verify matrix is Hermitian
    if (!matrix_is_hermitian(matrix, n, tolerance)) {
        return -1;
    }
    
    // Copy matrix to working array (will be diagonalized)
    complex_t *A = (complex_t *)malloc(n * n * sizeof(complex_t));
    if (!A) return -1;
    memcpy(A, matrix, n * n * sizeof(complex_t));
    
    // Initialize eigenvectors to identity
    memset(eigenvectors, 0, n * n * sizeof(complex_t));
    for (size_t i = 0; i < n; i++) {
        eigenvectors[i * n + i] = 1.0;
    }
    
    // Jacobi rotation algorithm
    int iteration = 0;
    int converged = 0;
    
    while (iteration < max_iterations && !converged) {
        // Find largest off-diagonal element
        double max_off_diag = 0.0;
        size_t max_p = 0, max_q = 1;
        
        for (size_t p = 0; p < n - 1; p++) {
            for (size_t q = p + 1; q < n; q++) {
                double abs_sq = complex_abs_squared(A[p * n + q]);
                if (abs_sq > max_off_diag) {
                    max_off_diag = abs_sq;
                    max_p = p;
                    max_q = q;
                }
            }
        }
        
        // Check convergence
        if (sqrt(max_off_diag) < tolerance) {
            converged = 1;
            break;
        }
        
        // Apply Givens rotation
        double c, s;
        givens_rotation(A, n, max_p, max_q, &c, &s);
        
        // Update matrix A
        for (size_t i = 0; i < n; i++) {
            if (i != max_p && i != max_q) {
                complex_t aip = A[i * n + max_p];
                complex_t aiq = A[i * n + max_q];
                
                A[i * n + max_p] = c * aip - s * aiq;
                A[i * n + max_q] = s * aip + c * aiq;
                A[max_p * n + i] = conj(A[i * n + max_p]);
                A[max_q * n + i] = conj(A[i * n + max_q]);
            }
        }
        
        // Update diagonal elements
        complex_t app = A[max_p * n + max_p];
        complex_t aqq = A[max_q * n + max_q];
        complex_t apq = A[max_p * n + max_q];
        
        A[max_p * n + max_p] = c * c * app - 2.0 * c * s * apq + s * s * aqq;
        A[max_q * n + max_q] = s * s * app + 2.0 * c * s * apq + c * c * aqq;
        A[max_p * n + max_q] = 0.0;
        A[max_q * n + max_p] = 0.0;
        
        // Update eigenvectors
        for (size_t i = 0; i < n; i++) {
            complex_t vip = eigenvectors[i * n + max_p];
            complex_t viq = eigenvectors[i * n + max_q];
            
            eigenvectors[i * n + max_p] = c * vip - s * viq;
            eigenvectors[i * n + max_q] = s * vip + c * viq;
        }
        
        iteration++;
    }
    
    if (!converged) {
        free(A);
        return -1;  // Failed to converge
    }
    
    // Extract eigenvalues (diagonal elements)
    for (size_t i = 0; i < n; i++) {
        eigenvalues[i] = creal(A[i * n + i]);
    }
    
    // Sort eigenvalues in descending order with eigenvectors
    for (size_t i = 0; i < n - 1; i++) {
        for (size_t j = i + 1; j < n; j++) {
            if (eigenvalues[j] > eigenvalues[i]) {
                // Swap eigenvalues
                double temp_val = eigenvalues[i];
                eigenvalues[i] = eigenvalues[j];
                eigenvalues[j] = temp_val;
                
                // Swap eigenvector columns
                for (size_t k = 0; k < n; k++) {
                    complex_t temp = eigenvectors[k * n + i];
                    eigenvectors[k * n + i] = eigenvectors[k * n + j];
                    eigenvectors[k * n + j] = temp;
                }
            }
        }
    }
    
    free(A);
    return 0;
}

// ============================================================================
// MATRIX OPERATIONS
// ============================================================================

void matrix_multiply(
    const complex_t *a,
    const complex_t *b,
    complex_t *c,
    size_t m,
    size_t k,
    size_t n
) {
    if (!a || !b || !c) return;
    
    // C[m×n] = A[m×k] × B[k×n]
    for (size_t i = 0; i < m; i++) {
        for (size_t j = 0; j < n; j++) {
            complex_t sum = 0.0;
            for (size_t l = 0; l < k; l++) {
                sum += a[i * k + l] * b[l * n + j];
            }
            c[i * n + j] = sum;
        }
    }
}

complex_t matrix_trace(const complex_t *matrix, size_t n) {
    if (!matrix) return 0.0;
    
    complex_t trace = 0.0;
    for (size_t i = 0; i < n; i++) {
        trace += matrix[i * n + i];
    }
    return trace;
}

int matrix_is_hermitian(const complex_t *matrix, size_t n, double tolerance) {
    if (!matrix) return 0;
    
    for (size_t i = 0; i < n; i++) {
        // Diagonal elements must be real
        if (fabs(cimag(matrix[i * n + i])) > tolerance) {
            return 0;
        }
        
        for (size_t j = i + 1; j < n; j++) {
            complex_t aij = matrix[i * n + j];
            complex_t aji = matrix[j * n + i];
            
            // Check if A[i,j] = conj(A[j,i])
            double diff_real = fabs(creal(aij) - creal(aji));
            double diff_imag = fabs(cimag(aij) + cimag(aji));
            
            if (diff_real > tolerance || diff_imag > tolerance) {
                return 0;
            }
        }
    }
    
    return 1;
}

void matrix_conjugate_transpose(
    const complex_t *matrix,
    complex_t *result,
    size_t m,
    size_t n
) {
    if (!matrix || !result) return;
    
    // Result[n×m] = Matrix[m×n]†
    for (size_t i = 0; i < m; i++) {
        for (size_t j = 0; j < n; j++) {
            result[j * m + i] = conj(matrix[i * n + j]);
        }
    }
}

double matrix_frobenius_norm(const complex_t *matrix, size_t m, size_t n) {
    if (!matrix) return 0.0;
    
    double sum = 0.0;
    for (size_t i = 0; i < m * n; i++) {
        sum += complex_abs_squared(matrix[i]);
    }
    return sqrt(sum);
}