#include "compute.h"
#include "parser.h"
#include "lexer.h"

static void collapse_env(varenv_t* env)
{
	if (!env) return;
	if (env->next) collapse_env(env->next);
	free(env);
}

#define COMRES_V(v) (compresult_t){ v, 0 }
#define COMRES_E(e) (compresult_t){ 0.0, e }

static compresult_t compute_variable(const ast_node_t* node, const varenv_t* env);
static compresult_t compute_dfunction(const ast_node_t* node, const varenv_t* env);

compresult_t compute_node(const struct ast_node* node, const varenv_t* env)
{
	if (!node) return COMRES_V(NAN);

	if (node->type == NODE_NUMBER)
		return COMRES_V(node->number);
	else if (node->type == NODE_PREVANSWER)
		return get_previous_answer();
	else if (node->type == NODE_VARIABLE)
		return compute_variable(node, env);
	else if (node->type == NODE_LFUNCTION)
		return node->lfunc->logic(node->left, env);
	else if (node->type == NODE_DFUNCTION)
		return compute_dfunction(node, env);

	compresult_t lres = COMRES_E(-1);
	if (node->left) lres = compute_node(node->left, env);
	if (lres.error) return COMRES_E(lres.error);
	if (isnan(lres.value)) return COMRES_V(NAN);

	if (node->type == NODE_NEGATE)
		return COMRES_V(-lres.value);
	else if (node->type == NODE_ABS)
		return COMRES_V(fabs(lres.value));
	else if (node->type == NODE_SFUNCTION)
		return COMRES_V(node->sfunc->logic(lres.value));
	
	compresult_t rres = COMRES_E(-1);
	if (node->right) rres = compute_node(node->right, env);
	if (rres.error) return COMRES_E(rres.error);
	if (isnan(rres.value)) COMRES_V(NAN);

	if (node->type == NODE_POW)
		return COMRES_V(pow(lres.value, rres.value));
	else if (node->type == NODE_MULTIPLY)
		return COMRES_V(lres.value * rres.value);
	else if (node->type == NODE_DIVIDE)
		return COMRES_V(lres.value / rres.value);
	else if (node->type == NODE_ADD)
		return COMRES_V(lres.value + rres.value);
	else if (node->type == NODE_SUBTRACT)
		return COMRES_V(lres.value - rres.value);

	collapse_env(env);
	return COMRES_E(ERROR_COMPUTE_UNDEFINED_OPERATION);
}

/* helpers */
static compresult_t compute_variable(const ast_node_t* node, const varenv_t* env)
{
	while (env)
	{
		if (ident_eq(env->variable, node->ident)) return env->result;
		env = env->next;
	}
	const ast_node_t* v = get_variable(node->ident);
	return v ? compute_node(v, env) : COMRES_E(ERROR_COMPUTE_UNDEFINED_VARIABLE);
}
static compresult_t compute_dfunction(const ast_node_t* node, const varenv_t* oldenv)
{
	const dfunc_t* dfunc = get_dfunc(node->ident);
	if (!dfunc) return COMRES_E(ERROR_COMPUTE_UNDEFINED_FUNCTION);

	varenv_t* env = NULL;
	const ast_node_t* args = node->left;
	const ast_node_t* pattern = dfunc->args;
	while (pattern)
	{
		if (!args)
		{
			collapse_env(env);
			return COMRES_E(ERROR_COMPUTE_TOO_FEW_ARGUMENTS);
		}

		varenv_t* newenv = calloc(1, sizeof(varenv_t));
		if (!newenv) PANIC("Undefined unallocation");

		newenv->variable = pattern->left->ident;
		newenv->result = compute_node(args->left, oldenv);
		newenv->next = env;
		env = newenv;

		if (newenv->result.error)
		{
			collapse_env(env);
			return newenv->result;
		}

		pattern = pattern->right;
		args = args->right;
	}

	return compute_node(dfunc->impl, env);
}

/* S-function implementation */
double sign(double x) { return x > 0 ? 1 : (x == 0 ? 0 : -1); }
double cot(double x) { return cos(x) / sin(x); }
double coth(double x) { return cosh(x) / sinh(x); }
double acot(double x) { return 1.570796326794896557998981734 - atan(x); }
double acoth(double x) { return atanh(1.0 / x); }

/* L-function implementation */
compresult_t llog(const struct ast_node* node, const struct varenv* env)
{
	compresult_t base = compute_node(node->left, env);
	if (!node->right) return COMRES_E(ERROR_COMPUTE_TOO_FEW_ARGUMENTS);
	compresult_t value = compute_node(node->right->left, env);
	if (base.error) return COMRES_E(base.error);
	if (value.error) return COMRES_E(value.error);

	return COMRES_V(log(value.value) / log(base.value));
}
compresult_t lmax(const struct ast_node* node, const struct varenv* env)
{
	double maximum = -INFINITY;
	while (node)
	{
		compresult_t cr = compute_node(node->left, env);
		if (cr.error) return COMRES_E(cr.error);
		else
		{
			if (cr.value > maximum) maximum = cr.value;
		}
		node = node->right;
	}
	return COMRES_V(maximum);
}
compresult_t lmin(const struct ast_node* node, const struct varenv* env)
{
	double minimum = INFINITY;
	while (node)
	{
		compresult_t cr = compute_node(node->left, env);
		if (cr.error) return COMRES_E(cr.error);
		else
		{
			if (cr.value < minimum) minimum = cr.value;
		}
		node = node->right;
	}
	return COMRES_V(minimum);
}
static long long gcd(long long a, long long b)
{
	while (b != 0) {
		long long t = a % b;
		a = b;
		b = t;
	}
	return a;
}
compresult_t lgcd(const struct ast_node* node, const struct varenv* env)
{
	long long g = 0;
	while (node)
	{
		compresult_t cr = compute_node(node->left, env);
		if (cr.error) return COMRES_E(cr.error);
		else
		{
			long long v = (long long)floor(cr.value);
			g = gcd(g, v);
		}
		node = node->right;
	}
	return COMRES_V((double)g);
}
static long long lcm(long long a, long long b)
{
	return a * b / gcd(a, b);
}
compresult_t llcm(const struct ast_node* node, const struct varenv* env)
{
	long long g = 1;
	while (node)
	{
		compresult_t cr = compute_node(node->left, env);
		if (cr.error) return COMRES_E(cr.error);
		else
		{
			long long v = (long long)floor(cr.value);
			g = lcm(g, v);
		}
		node = node->right;
	}
	return COMRES_V((double)g);
}
compresult_t lmod(const struct ast_node* node, const struct varenv* env)
{
	compresult_t dividend = compute_node(node->left, env);
	if (!node->right) return COMRES_E(ERROR_COMPUTE_TOO_FEW_ARGUMENTS);
	compresult_t divisor = compute_node(node->right->left, env);
	if (dividend.error) return COMRES_E(dividend.error);
	if (divisor.error) return COMRES_E(divisor.error);

	long long rem = (long long)floor(dividend.value) % (long long)floor(divisor.value);
	return COMRES_V((double)rem);
}
