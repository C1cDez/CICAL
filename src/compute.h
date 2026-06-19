#pragma once

#include "cical.h"

typedef struct compresult
{
	double value;
	int error;
} compresult_t;

struct ast_node;

typedef struct varenv
{
	identifier_t variable;
	compresult_t result;
	struct varenv* next;
} varenv_t;

compresult_t compute_node(const struct ast_node* node, const varenv_t* env);

/* S-functions */
double sign(double x);
double cot(double x);
double coth(double x);
double acot(double x);
double acoth(double x);

/* L-functions */
compresult_t llog(const struct ast_node* node, struct varenv* env);
compresult_t lmax(const struct ast_node* node, struct varenv* env);
compresult_t lmin(const struct ast_node* node, struct varenv* env);
compresult_t lgcd(const struct ast_node* node, struct varenv* env);
compresult_t llcm(const struct ast_node* node, struct varenv* env);
compresult_t lmod(const struct ast_node* node, struct varenv* env);
