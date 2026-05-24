#include "cical.h"

#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>


// LITERALS

static const literal_t LITERALS[] = {
	{ "sqrt", LITERAL_FUNCTION, 0 },
	{ "mod", LITERAL_FUNCTION, 1 },
	{ "log", LITERAL_FUNCTION, 1 },
	{ "ln", LITERAL_FUNCTION, 1 },

	{ "sin", LITERAL_FUNCTION, 0 },
	{ "cos", LITERAL_FUNCTION, 0 },
	{ "tan", LITERAL_FUNCTION, 0 },
	{ "cot", LITERAL_FUNCTION, 0 },

	{ "pi", LITERAL_CONST },
	{ "e", LITERAL_CONST },
	{ "tau", LITERAL_CONST },
	{ "phi", LITERAL_CONST },
	{ "gamma", LITERAL_CONST },
	{ "rho", LITERAL_CONST },
	{ "alpha", LITERAL_CONST },
	{ "beta", LITERAL_CONST },
};

literal_t get_literal(int idx)
{
	return LITERALS[idx];
}

int find_literal(const char* str)
{
	for (int i = 0; i < __crt_countof(LITERALS); i++)
	{
		if (!strncmp(str, LITERALS[i].name, strlen(LITERALS[i].name))) return i;
	}
	return -1;
}

// VARIABLES

#define VARSET_SIZE 1024
struct
{
	variable_t var;
	ast_node_t* value;
} VARSET[VARSET_SIZE] = { 0 };

static void new_variable(variable_t var, const ast_node_t* value)
{
	int idx = 0;
	for (; idx < VARSET_SIZE; idx++)
	{
		if (VARSET[idx].var.sym == var.sym && VARSET[idx].var.ident == var.ident) break;
		else if (!VARSET[idx].value) break;
	}
	if (idx == VARSET_SIZE) return;

	if (VARSET[idx].value) annihilate_tree(VARSET[idx].value);
	
	VARSET[idx].var.sym = var.sym;
	VARSET[idx].var.ident = var.ident;
	VARSET[idx].value = value;
}
static ast_node_t* lookfor_variable(variable_t var)
{
	for (int i = 0; i < VARSET_SIZE; i++)
	{
		if (VARSET[i].var.sym == var.sym && VARSET[i].var.ident == var.ident)
			return VARSET[i].value;
	}
	return NULL;
}


// COMPUTATION

#define COMRES(v, e) (compresult_t) { .value = v, .error = e }
#define COMRES_V(v) COMRES(v, 0)
#define COMRES_E(e) COMRES(0.0, e)

static compresult_t compute_function(int funcidx, double x)
{
	switch (funcidx)
	{
	case 0: return x < 0 ? COMRES_E(ERROR_COMPUTE_ILLEGAL_DOMAIN) : COMRES_V(sqrt(x));
	case 2: return x < 0 ? COMRES_E(ERROR_COMPUTE_ILLEGAL_DOMAIN) : COMRES_V(log10(x));
	case 3: return x < 0 ? COMRES_E(ERROR_COMPUTE_ILLEGAL_DOMAIN) : COMRES_V(log(x));

	case 4: return COMRES_V(sin(x));
	case 5: return COMRES_V(cos(x));
	case 6: return COMRES_V(tan(x));
	default: return COMRES_E(ERROR_COMPUTE_UNDEFINED_OPERATION);
	}
}
static compresult_t compute_const(int constid)
{
	switch (constid)
	{
	case 8: return COMRES_V(3.1415926535897932384626433832795028841971693993751058209749445923078164062862089986280348253421170679);
	case 9: return COMRES_V(2.7182818284590452353602874713526624977572470936999595749669676277240766303535475945713821785251664274);
	case 10: return COMRES_V(6.2831853071795864769252867665590057683943387987502116419498891846156328125724179972560696506842341359);
	case 11: return COMRES_V(1.6180339887498948482045868343656381177203091798057628621354486227052604628189024497072072041893911374);
	case 12: return COMRES_V(0.5772156649015328606065120900824024310421593359399235988057672348848677267776646709369470632917467498);
	default: return COMRES_E(ERROR_COMPUTE_UNDEFINED_VARIABLE);
	}
}

static compresult_t compute_node(const ast_node_t* node)
{
	if (node->type == NODE_NUMBER)
		return COMRES_V(node->number);
	else if (node->type == NODE_VARIABLE)
	{
		ast_node_t* variable = lookfor_variable(node->variable);
		if (variable) return compute_node(variable);
		else return COMRES_E(ERROR_COMPUTE_UNDEFINED_VARIABLE);
	}
	else if (node->type == NODE_CONST)
		return compute_const(node->variable.ident);

	compresult_t lres = COMRES(0.0, -1);
	if (node->left) lres = compute_node(node->left);
	if (lres.error) return lres;

	if (node->type == NODE_UNARY_MINUS)
		return COMRES_V(-lres.value);
	else if (node->type == NODE_FUNCCALL)
		return compute_function(node->funcidx, lres.value);

	compresult_t rres = COMRES(0.0, -1);
	if (node->right) rres = compute_node(node->right);
	if (rres.error) return rres;

	if (node->type == NODE_POW)
		return COMRES_V(pow(lres.value, rres.value));
	else if (node->type == NODE_MULTIPLY)
		return COMRES_V(lres.value * rres.value);
	else if (node->type == NODE_DIVIDE)
		return rres.value == 0 ? COMRES_E(ERROR_COMPUTE_DIVISION_BY_ZERO) : COMRES_V(lres.value / rres.value);
	else if (node->type == NODE_ADD)
		return COMRES_V(lres.value + rres.value);
	else if (node->type == NODE_SUBTRACT)
		return COMRES_V(lres.value - rres.value);
	else
		return COMRES_E(ERROR_COMPUTE_UNDEFINED_OPERATION);
}

int execute_sequence(const ast_node_t* node, compresult_t* cr)
{
	if (node->type == NODE_ASSIGNMENT)
	{
		ast_node_t* var = node->left, * expr = node->right;
		if (!var || !expr) return 1;
		new_variable(var->variable, expr);
		return 0;
	}
	else
	{
		*cr = compute_node(node);
		return 1;
	}
}
