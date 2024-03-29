#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "iter.h"

/* 
 * Solve the SPD system with the iterative conjugate gradient method
 * store the result in vector x and also return the number of iterations
 */ 
int conj_grad(double **A, cs *C, double *x, double *b, double *M, int dimension, double itol, int maxiter, bool SPARSE) {
	double *Ax = (double *)malloc(dimension * sizeof(double));
	/* Residual vector r */
	double *r  = (double *)malloc(dimension * sizeof(double));
	/* Alocate z vector: solution of preconditioner */
	double *z  = (double *)malloc(dimension * sizeof(double));
	double *p  = (double *)malloc(dimension * sizeof(double));
	double *q  = (double *)malloc(dimension * sizeof(double));
	double r_norm, b_norm, rho, rho1, alpha, beta;
	int iter = 0;

	if (!SPARSE) {
		/* Compute A*x and store it to Ax */
		mat_vec_mul(Ax, A, x, dimension);
	}
	else {
		/* Compute A*x and store it ot Ax , C is CCF of A */
		cs_mat_vec_mul(Ax, C, x);
	}
	
	/* Compute r = b - Ax */
	sub_vector(r, b, Ax, dimension);

	/* Initialize norm2 of b and r vectors */
	r_norm = norm2(r, dimension);
	b_norm = norm2(b, dimension);
	/* Set b_norm = 1 in case it's zero to avoid seg fault */
	b_norm = b_norm == 0.0 ? 1.0 : b_norm;

	while (iter < maxiter && (r_norm / b_norm) > itol) {
		iter++;
		/* Solution of the preconditioner Mz = r */
		precond_solve(z, M, r, dimension);
		/* rho = r*z */
		rho = dot_product(r, z, dimension);
		if (iter == 1) {
			/* Set p = z */
			memcpy(p, z, dimension * sizeof(double));
		}
		else {
			beta = rho / rho1;
			/* p = z + beta*p */
			axpy(p, beta, p, z, dimension);
		}
		rho1 = rho;
		if (SPARSE) {
			/* q = A*p, C is CCF of A */
			cs_mat_vec_mul(q, C, p);
		}
		else {
			/* q = A*p */
			mat_vec_mul(q, A, p, dimension);
		}
		/* a = rho / p*q */
		alpha = rho / dot_product(p, q, dimension);
		/* x = x + alpha*p */
		axpy(x,  alpha, p, x, dimension);
		/* r = r - alpha*q */
		axpy(r, -alpha, q, r, dimension);
		r_norm = norm2(r, dimension);
	}

	/* Free all the memory we allocated */
	free(Ax);
	free(r);
	free(z);
	free(p);
	free(q);

	return iter;
}

/* 
 * Solve the complex SPD system with the iterative conjugate gradient method
 * store the result in vector x and also return the number of iterations
 */ 
int complex_conj_grad(gsl_matrix_complex *A, cs_ci *C, gsl_vector_complex *x, gsl_vector_complex *b,
 					  gsl_vector_complex *M, int dimension, double itol, int maxiter, bool SPARSE) {
	gsl_vector_complex *Ax = init_gsl_complex_vector(dimension);
	/* Residual vector r */
	gsl_vector_complex *r = init_gsl_complex_vector(dimension);
	/* Alocate z vector: solution of preconditioner */
	gsl_vector_complex *z = init_gsl_complex_vector(dimension);
	gsl_vector_complex *p = init_gsl_complex_vector(dimension);
	gsl_vector_complex *q = init_gsl_complex_vector(dimension);
	double r_norm, b_norm;
	gsl_complex rho, rho1, alpha, beta;
	int iter = 0;

	if (SPARSE) {
		complex_cs_mat_vec_mul(Ax, C, x);
	}
	else {
		/* Compute A*x and store it to Ax, with Hermitian of A conjugate+transpose */
		gsl_blas_zgemv(CblasNoTrans, GSL_COMPLEX_ONE, A, x, GSL_COMPLEX_ZERO, Ax);
	}
	
	/* Compute r = b - Ax */
	/* Computes the y = ax + b, we want r = -Ax + b */
	complex_axpy(r, GSL_COMPLEX_NEGONE, Ax, b, dimension);

	/* Initialize norm2 of b and r vectors */
	r_norm = complex_norm2(r, dimension);
	b_norm = complex_norm2(b, dimension);
	/* Set b_norm = 1 in case it's zero to avoid seg fault */
	b_norm = b_norm == 0.0 ? 1.0 : b_norm;

	while (iter < maxiter && (r_norm / b_norm) > itol) {
		iter++;
		/* Solution of the preconditioner Mz = r */
		complex_precond_solve(z, M, r, dimension);
		/* rho = r*z */
		rho = complex_dot_product(r, z, dimension);
		if (iter == 1) {
			/* Set p = z */
			gsl_vector_complex_memcpy(p, z);
		}
		else {
			/* beta = rho / rho1 */
			beta = gsl_complex_div(rho, rho1);
			/* p = z + beta*p */
			complex_axpy(p, beta, p, z, dimension);
		}
		rho1 = rho;
		if (SPARSE) {
			/* q = A*p, C is CCF of A */
			complex_cs_mat_vec_mul(q, C, p);
		}
		else {
			/* q = A*p */
			gsl_blas_zgemv(CblasNoTrans, GSL_COMPLEX_ONE, A, p, GSL_COMPLEX_ZERO, q);
		}
		/* a = rho / p*q */
		alpha = gsl_complex_div(rho, complex_dot_product(p, q, dimension));
		/* x = x + alpha*p */
		complex_axpy(x, alpha, p, x, dimension);
		/* r = r - alpha*q */
		complex_axpy(r, __gsl_complex_neg(alpha), q, r, dimension);
		r_norm = complex_norm2(r, dimension);
	}

	/* Free all the memory we allocated */
	gsl_vector_complex_free(Ax);
	gsl_vector_complex_free(r);
	gsl_vector_complex_free(z);
	gsl_vector_complex_free(p);
	gsl_vector_complex_free(q);

	return iter;
}

/* 
 * Solve the system with the iterative bi-conjugate gradient method
 * store the result in vector x and also return the number of iterations
 * or FAILURE in case it fails
 */ 
int bi_conj_grad(double **A, cs *C, double *x, double *b, double *M, int dimension, double itol, int maxiter, bool SPARSE) {
	/* Set maxiter to our threshold in case the provided one is small for Bi-CG */
	maxiter = MAX(maxiter, MAX_ITER_THRESHOLD);
	/* Vector to store A*x */
	double *Ax = (double *)malloc(dimension * sizeof(double));
	/* Residual vector r */
	double *r  = (double *)malloc(dimension * sizeof(double));
	/* Alocate z vector: solution of preconditioner */
	double *z  = (double *)malloc(dimension * sizeof(double));
	double *p  = (double *)malloc(dimension * sizeof(double));
	double *q  = (double *)malloc(dimension * sizeof(double));
	/* Residual vector r_tilde */
	double *r_tilde = (double *)malloc(dimension * sizeof(double));
	/* Alocate z_tilde vector: solution of preconditioner */
	double *z_tilde = (double *)malloc(dimension * sizeof(double));
	double *p_tilde = (double *)malloc(dimension * sizeof(double));
	double *q_tilde = (double *)malloc(dimension * sizeof(double));
	double r_norm, b_norm, rho, rho1, alpha, beta, omega;
	int iter = 0;

	if (SPARSE) {
		/* Compute A*x and store it ot Ax , C is CCF of A */
		cs_mat_vec_mul(Ax, C, x);
	}
	else {
		/* Compute A*x and store it to Ax */
		mat_vec_mul(Ax, A, x, dimension);
	}

	/* Compute r = b - Ax */
	sub_vector(r, b, Ax, dimension);

	/* Compute r_tilde = b - Ax = r */
	memcpy(r_tilde, r, dimension * sizeof(double));

	/* Initialize norm2 of b and r vectors */
	r_norm = norm2(r, dimension);
	b_norm = norm2(b, dimension);
	/* Set b_norm = 1 in case it's zero to avoid seg fault */
	b_norm = b_norm == 0.0 ? 1.0 : b_norm;
	
	while (iter < maxiter && (r_norm / b_norm) > itol) {
		iter++;
		/* Solution of the preconditioner Mz = r */
		precond_solve(z, M, r, dimension);
		/* Solution of the preconditioner M'z_tilde = r_tilde, *
		 * M' = M because is a diagonal matrix                 */
		precond_solve(z_tilde, M, r_tilde, dimension);
		/* rho = r_tilde*z */
		rho = dot_product(r_tilde, z, dimension);
		/* Check for Algorithm Failure */
		if (fabs(rho) < EPSILON) {
			return -1;
		}
		if (iter == 1) {
			/* Set p = z */
			memcpy(p, z, dimension * sizeof(double));
			/* Set p_tilde = z_tilde */
			memcpy(p_tilde, z_tilde, dimension * sizeof(double));
		}
		else {
			beta = rho / rho1;
			/* p = z + beta*p */
			axpy(p, beta, p, z, dimension);
			/* p_tilde = z_tilde + beta*p_tilde */
			axpy(p_tilde, beta, p_tilde, z_tilde, dimension);
		}
		rho1 = rho;
		if (SPARSE) {
			/* q = A*p, C is CCF of A */
			cs_mat_vec_mul(q, C, p);
			/* q_tilde = A'*p_tilde, A' = A, C is CCF of A */
			cs_mat_vec_mul_trans(q_tilde, C, p_tilde);
		}
		else {
			/* q = A*p */
			mat_vec_mul(q, A, p, dimension);
			/* q_tilde = A'*p_tilde, A' = A */
			mat_vec_mul_trans(q_tilde, A, p_tilde, dimension);
		}
		/* omega = p_tilde * q */
		omega = dot_product(p_tilde, q, dimension);
		/* Check for Algorithm Failure */
		if (fabs(omega) < EPSILON) {
			return -1;
		}
		alpha = rho / omega;
		/* x = x + alpha*p */
		axpy(x,  alpha, p, x, dimension);
		/* r = r - alpha*q */
		axpy(r, -alpha, q, r, dimension);
		/* r_tilde = r_tilde - alpha*q_tilde */
		axpy(r_tilde, -alpha, q_tilde, r_tilde, dimension);
		r_norm = norm2(r, dimension);
	}

	/* Free all the memory we allocated */
	free(Ax);
	free(r);
	free(z);
	free(p);
	free(q);
	free(r_tilde);
	free(z_tilde);
	free(p_tilde);
	free(q_tilde);

	return iter;
}

/* 
 * Solve the complex system with the iterative bi-conjugate gradient method
 * store the result in vector x and also return the number of iterations
 * or FAILURE in case it fails
 */ 
int complex_bi_conj_grad(gsl_matrix_complex *A, cs_ci *C,  gsl_vector_complex *x, gsl_vector_complex *b,
 						 gsl_vector_complex *M, gsl_vector_complex *M_conj, int dimension, double itol,
						 int maxiter, bool SPARSE) {
	/* Set maxiter to our threshold in case the provided one is small for Bi-CG */
	maxiter = MAX(maxiter, MAX_ITER_THRESHOLD);
	/* Vector to store A*x */
	gsl_vector_complex *Ax = init_gsl_complex_vector(dimension);
	/* Residual vector r */
	gsl_vector_complex *r = init_gsl_complex_vector(dimension);
	/* Alocate z vector: solution of preconditioner */
	gsl_vector_complex *z = init_gsl_complex_vector(dimension);
	gsl_vector_complex *p = init_gsl_complex_vector(dimension);
	gsl_vector_complex *q = init_gsl_complex_vector(dimension);
	/* Residual vector r_tilde */
	gsl_vector_complex *r_tilde = init_gsl_complex_vector(dimension);
	/* Alocate z_tilde vector: solution of preconditioner */
	gsl_vector_complex *z_tilde = init_gsl_complex_vector(dimension);
	gsl_vector_complex *p_tilde = init_gsl_complex_vector(dimension);
	gsl_vector_complex *q_tilde = init_gsl_complex_vector(dimension);
	double r_norm, b_norm;
	gsl_complex rho, rho1, alpha, beta, omega;
	int iter = 0;

	if (SPARSE) {
		/* Compute A*x and store it ot Ax , C is CCF of A */
		complex_cs_mat_vec_mul(Ax, C, x);
	}
	else {
		/* Compute A*x and store it to Ax */
		gsl_blas_zgemv(CblasNoTrans, GSL_COMPLEX_ONE, A, x, GSL_COMPLEX_ZERO, Ax);
	}
	
	/* Compute r = b - Ax */
	/* Computes the y = ax + b, we want r = -Ax + b */
	complex_axpy(r, GSL_COMPLEX_NEGONE, Ax, b, dimension);

	/* Compute r_tilde = b - Ax = r */
	gsl_vector_complex_memcpy(r_tilde, r);

	/* Initialize norm2 of b and r vectors */
	r_norm = complex_norm2(r, dimension);
	b_norm = complex_norm2(b, dimension);
	/* Set b_norm = 1 in case it's zero to avoid seg fault */
	b_norm = b_norm == 0.0 ? 1.0 : b_norm;

	while (iter < maxiter && (r_norm / b_norm) > itol) {
		iter++;
		/* Solution of the preconditioner Mz = r */
		complex_precond_solve(z, M, r, dimension);
		/* Solution of the preconditioner M'z_tilde = r_tilde, *
		 * M_conj = M* because is a diagonal matrix                 */
		complex_precond_solve(z_tilde, M_conj, r_tilde, dimension);
		/* rho = r_tilde*z */
		rho = complex_dot_product(r_tilde, z, dimension);
		/* Check for Algorithm Failure */
		if (complex_abs(rho) < EPSILON) {
			return -1;
		}
		if (iter == 1) {
			/* Set p = z */
			gsl_vector_complex_memcpy(p, z);
			/* Set p_tilde = z_tilde */
			gsl_vector_complex_memcpy(p_tilde, z_tilde);
		}
		else {
			/* beta = rho / rho1 */
			beta = gsl_complex_div(rho, rho1);
			/* p = z + beta*p */
			complex_axpy(p, beta, p, z, dimension);
			/* p_tilde = z_tilde + beta_conj*p_tilde */
			complex_axpy(p_tilde, __gsl_complex_conj(beta), p_tilde, z_tilde, dimension);
		}
		rho1 = rho;
		if (SPARSE) {
			/* q = A*p, C is CCF of A */
			complex_cs_mat_vec_mul(q, C, p);
			/* q_tilde = A^H*p_tilde, A^H = A*', C is CCF of A */
			complex_cs_mat_vec_mul_herm(q_tilde, C, p_tilde);
		}
		else {
			/* q = A*p */
			gsl_blas_zgemv(CblasNoTrans, GSL_COMPLEX_ONE, A, p, GSL_COMPLEX_ZERO, q);
			/* q_tilde = A^H*p_tilde */
			gsl_blas_zgemv(CblasConjTrans, GSL_COMPLEX_ONE, A, p_tilde, GSL_COMPLEX_ZERO, q_tilde);
		}
		/* omega = p_tilde * q */
		omega = complex_dot_product(p_tilde, q, dimension);
		
		/* Check for Algorithm Failure */
		if (complex_abs(omega) < EPSILON) {
			return -1;
		}
		/* alpha = rho / omega */
		alpha = gsl_complex_div(rho, omega);
		/* x = x + alpha*p */
		complex_axpy(x, alpha, p, x, dimension);
		/* r = r - alpha*q */
		complex_axpy(r, __gsl_complex_neg(alpha), q, r, dimension);
		/* r_tilde = r_tilde - alpha_conj*q_tilde */
		complex_axpy(r_tilde, __gsl_complex_neg(__gsl_complex_conj(alpha)), q_tilde, r_tilde, dimension);
		r_norm = complex_norm2(r, dimension);
	}

	/* Free all the memory we allocated */
	gsl_vector_complex_free(Ax);
	gsl_vector_complex_free(r);
	gsl_vector_complex_free(z);
	gsl_vector_complex_free(p);
	gsl_vector_complex_free(q);
	gsl_vector_complex_free(r_tilde);
	gsl_vector_complex_free(z_tilde);
	gsl_vector_complex_free(p_tilde);
	gsl_vector_complex_free(q_tilde);

	return iter;
}