#include "cical.h"

#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>


// LITERALS

static const literal_t LITERALS[] = {
	// consts 0 - 7
	{ "pi", LITERAL_CONST },
	{ "e", LITERAL_CONST },
	{ "tau", LITERAL_CONST },
	{ "phi", LITERAL_CONST },
	{ "gamma", LITERAL_CONST },
	{ "rho", LITERAL_CONST },
	{ "alpha", LITERAL_CONST },
	{ "beta", LITERAL_CONST },

	// basic 8 - 13
	{ "sqrt", LITERAL_FUNCTION, 1 },
	{ "cbrt", LITERAL_FUNCTION, 1 },
	{ "nrt", LITERAL_FUNCTION, 2 },
	{ "log", LITERAL_FUNCTION, 2 },
	{ "ln", LITERAL_FUNCTION, 1 },
	{ "exp", LITERAL_FUNCTION, 1 },

	// trigonometry 14 - 22
	{ "sin", LITERAL_FUNCTION, 1 },
	{ "cos", LITERAL_FUNCTION, 1 },
	{ "tan", LITERAL_FUNCTION, 1 },
	{ "cot", LITERAL_FUNCTION, 1 },
	{ "sinh", LITERAL_FUNCTION, 1 },
	{ "cosh", LITERAL_FUNCTION, 1 },
	{ "tanh", LITERAL_FUNCTION, 1 },
	{ "coth", LITERAL_FUNCTION, 1 },

	{ "max", LITERAL_FUNCTION, -1 },
	{ "min", LITERAL_FUNCTION, -1 }
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


int does_function_exist(identifier_t id)
{
	return 0;
}

// VARIABLES

#define VARSET_SIZE 1024
struct
{
	identifier_t var;
	ast_node_t* value;
} VARSET[VARSET_SIZE] = { 0 };

static ast_node_t* lookfor_variable(identifier_t id)
{
	for (int i = 0; i < VARSET_SIZE; i++)
	{
		identifier_t v = VARSET[i].var;
		if (v.type == id.type && v.sym == id.sym && v.litid == id.litid && v.subscript == id.subscript)
			return VARSET[i].value;
	}
	return NULL;
}
static void insert_variable(identifier_t id, const ast_node_t* node)
{
	int i = 0;
	for (; i < VARSET_SIZE; i++)
	{
		identifier_t v = VARSET[i].var;
		if (
			(v.type == id.type && v.sym == id.sym && v.litid == id.litid && v.subscript == id.subscript) ||
			(v.type == 0)
		) break;
	}

	if (VARSET[i].value) annihilate_tree(VARSET[i].value);
	VARSET[i].var = id;
	VARSET[i].value = node;
}


// COMPUTATION

#define COMRES(v, e) (compresult_t) { .value = v, .error = e }
#define COMRES_V(v) COMRES(v, 0)
#define COMRES_E(e) COMRES(0.0, e)

static compresult_t compute_node(const ast_node_t* node);
static compresult_t compute_function(const ast_node_t* funcnode);


static compresult_t compute_function(const ast_node_t* funcnode)
{
	return COMRES_E(-1);
}
static compresult_t compute_node(const ast_node_t* node)
{
	if (node->type == NODE_NUMBER)
		return COMRES_V(node->number);
	else if (node->type == NODE_VARIABLE)
	{
		ast_node_t* v = lookfor_variable(node->ident);
		if (v) return compute_node(v);
		else return COMRES_E(ERROR_COMPUTE_UNDEFINED_VARIABLE);
	}

	compresult_t lr = COMRES_E(-1);
	if (node->left) lr = compute_node(node->left);
	if (lr.error) return COMRES_E(lr.error);
	if (isnan(lr.value)) return COMRES_E(ERROR_COMPUTE_UNEXPECTED_NAN);

	if (node->type == NODE_UNARY_MINUS)
		return COMRES_V(-lr.value);
	else if (node->type == NODE_ABS)
		return COMRES_V(lr.value < 0 ? -lr.value : lr.value);
	else if (node->type == NODE_FUNC_CALL)
		return compute_function(node);

	compresult_t rr = COMRES_E(-1);
	if (node->right) rr = compute_node(node->right);
	if (rr.error) return COMRES_E(rr.error);
	if (isnan(rr.value)) return COMRES_E(ERROR_COMPUTE_UNEXPECTED_NAN);

	if (node->type == NODE_POW)
		return COMRES_V(pow(lr.value, rr.value));
	else if (node->type == NODE_MULTIPLY)
		return COMRES_V(lr.value * rr.value);
	else if (node->type == NODE_DIVIDE)
		return COMRES_V(lr.value / rr.value);
	else if (node->type == NODE_ADD)
		return COMRES_V(lr.value + rr.value);
	else if (node->type == NODE_SUBTRACT)
		return COMRES_V(lr.value - rr.value);
}

int execute_sequence(const ast_node_t* node, compresult_t* cr)
{
	if (node->type == NODE_ASSIGNMENT)
	{
		ast_node_t* var = node->left, * expr = node->right;
		if (!var || !expr) return 1;
		insert_variable(var->ident, expr);
		return 0;
	}
	else
	{
		*cr = compute_node(node);
		return 1;
	}
}


// DEFAULTS

static int preload_const(int idx, double value, int litid, int subscript)
{
	ast_node_t* cnst = calloc(1, sizeof(ast_node_t));
	if (!cnst) return 1;
	cnst->type = NODE_NUMBER;
	cnst->number = value;
	VARSET[idx].var = (identifier_t){ .type = IDENTIFIER_LITERAL, .litid = litid, .subscript = subscript };
	VARSET[idx].value = cnst;
}
void preload_defaults()
{
	preload_const(0, 3.141592653589793238462643383279, 0, -1); // pi
	preload_const(1, 2.718281828459045235360287471352, 1, -1); // e
	preload_const(2, 1.618033988749894902525738871191, 3, -1); // phi
	preload_const(3, 0.577215664901532860606512090082, 4, -1); // gamma
}
