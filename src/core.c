#include "cical.h"

#include <stdlib.h>
#include <string.h>

#include "lexer.h"
#include "parser.h"
#include "compute.h"


#define countof(arr) (sizeof(arr) / sizeof(arr[0]))

static const alpha_name_t ALPHA_NAMES[] = {
	{ "alpha" }, { "beta" }, { "gamma" }, { "mu" }, { "pi" }, { "rho" }, { "tau" }, { "phi" }
};
const alpha_name_t* get_alpha_name(const char* str)
{
	for (int i = 0; i < countof(ALPHA_NAMES); i++)
	{
		if (!strncmp(str, ALPHA_NAMES[i].str, strlen(ALPHA_NAMES[i].str)))
			return &ALPHA_NAMES[i];
	}
	return NULL;
}


/* --------------------- S- & L- FUNCTIONS --------------------- */

static sfunc_t SFUNCS[] = {
	{ "sqrt" }, { "cbrt" }, { "ln" }, { "lg" }, { "exp" }, { "erf" },

	{ "arcsinh" },	{ "arccosh" },	{ "arctanh" },	{ "arccoth" }, 
	{ "arcsin" },	{ "arccos" },	{ "arctan" },	{ "arccot" },
	{ "sinh" },		{ "cosh" },		{ "tanh" },		{ "coth" },
	{ "sin" },		{ "cos" },		{ "tan" },		{ "cot" },
	
	{ "floor" }, { "ceil" }, { "sign" }
};
const sfunc_t* get_sfunc(const char* str)
{
	for (int i = 0; i < countof(SFUNCS); i++)
	{
		if (!strncmp(str, SFUNCS[i].name, strlen(SFUNCS[i].name)))
			return &SFUNCS[i];
	}
	return NULL;
}
static void declare_sfuncs()
{
	SFUNCS[0].logic = sqrt;
	SFUNCS[1].logic = cbrt;
	SFUNCS[2].logic = log;
	SFUNCS[3].logic = log10;
	SFUNCS[4].logic = exp;
	SFUNCS[5].logic = erf;

	SFUNCS[6].logic = asinh;
	SFUNCS[7].logic = acosh;
	SFUNCS[8].logic = atanh;
	SFUNCS[9].logic = acoth;

	SFUNCS[10].logic = asin;
	SFUNCS[11].logic = acos;
	SFUNCS[12].logic = atan;
	SFUNCS[13].logic = acot;

	SFUNCS[14].logic = sinh;
	SFUNCS[15].logic = cosh;
	SFUNCS[16].logic = tanh;
	SFUNCS[17].logic = coth;

	SFUNCS[18].logic = sin;
	SFUNCS[19].logic = cos;
	SFUNCS[20].logic = tan;
	SFUNCS[21].logic = cot;

	SFUNCS[22].logic = floor;
	SFUNCS[23].logic = ceil;
	SFUNCS[24].logic = sign;
}

static lfunc_t LFUNCS[] = {
	{ "log" },
	{ "max" }, { "min" },
	{ "lcm" }, { "gcd" },
	{ "mod" },
};
const lfunc_t* get_lfunc(const char* str)
{
	for (int i = 0; i < countof(LFUNCS); i++)
	{
		if (!strncmp(str, LFUNCS[i].name, strlen(LFUNCS[i].name)))
			return &LFUNCS[i];
	}
	return NULL;
}
static void declare_lfuncs()
{
	LFUNCS[0].logic = llog;
	LFUNCS[1].logic = lmax;
	LFUNCS[2].logic = lmin;
	LFUNCS[3].logic = llcm;
	LFUNCS[4].logic = lgcd;
	LFUNCS[5].logic = lmod;
}


/* --------------------- MISC --------------------- */

int ident_eq(identifier_t a, identifier_t b)
{
	return a.type == b.type && a.alpha == b.alpha && a.symbol == b.symbol && a.subscript == b.subscript;
}
void annihilate_tree(const struct ast_node* node)
{
	if (!node) return;
	if (node->left) annihilate_tree(node->left);
	if (node->right) annihilate_tree(node->right);
	free(node);
}


/* --------------------- VARIABLES --------------------- */

static struct
{
	identifier_t ident;
	const ast_node_t* value;
	int mut;
} VARSET[1024] = { 0 };

static void insert_new_variable(identifier_t ident, const ast_node_t* value, int mut)
{
	int i = 0;
	for (; i < countof(VARSET); i++)
	{
		if ((ident_eq(VARSET[i].ident, ident) || VARSET[i].ident.type == 0) && VARSET[i].mut) break;
	}

	if (i == countof(VARSET)) PANIC("Varset ended");

	annihilate_tree(VARSET[i].value);
	VARSET[i].ident = ident;
	VARSET[i].value = value;
	VARSET[i].mut = mut;
}
const struct ast_node* get_variable(identifier_t ident)
{
	for (int i = 0; i < countof(VARSET); i++)
	{
		if (ident_eq(VARSET[i].ident, ident)) return VARSET[i].value;
	}
	return NULL;
}
int remove_variables()
{
	int count = 0;
	for (int i = 0; i < countof(VARSET); i++)
	{
		if (VARSET[i].mut)
		{
			if (VARSET[i].value) count++;
			annihilate_tree(VARSET[i].value);
			VARSET[i].value = NULL;
			VARSET[i].ident = (identifier_t){ 0 };
		}
	}
	return count;
}


/* --------------------- FUNCTIONS --------------------- */

static dfunc_t FUNCSET[1024] = { 0 };
const dfunc_t* get_dfunc(identifier_t ident)
{
	for (int i = 0; i < countof(FUNCSET); i++)
	{
		if (ident_eq(FUNCSET[i].ident, ident)) return &FUNCSET[i];
	}
	return NULL;
}
static int insert_new_dfunc(identifier_t ident, const ast_node_t* args, const ast_node_t* impl)
{
	int i = 0;
	for (; i < countof(FUNCSET); i++)
	{
		if (ident_eq(FUNCSET[i].ident, ident) || FUNCSET[i].ident.type == 0) break;
	}

	if (i == countof(FUNCSET)) PANIC("Funcset ended");

	ast_node_t* temp = args;
	while (temp)
	{
		if (temp->left->type != NODE_VARIABLE) return 1;
		temp = temp->right;
	}

	FUNCSET[i].ident = ident;
	annihilate_tree(FUNCSET[i].args);
	annihilate_tree(FUNCSET[i].impl);
	FUNCSET[i].args = args;
	FUNCSET[i].impl = impl;
	return 0;
}
int remove_dfuncs()
{
	int count = 0;
	for (int i = 0; i < countof(FUNCSET); i++)
	{
		if (FUNCSET[i].impl) count++;
		FUNCSET[i].ident = (identifier_t) { 0 };
		annihilate_tree(FUNCSET[i].args);
		annihilate_tree(FUNCSET[i].impl);
		FUNCSET[i].args = NULL;
		FUNCSET[i].impl = NULL;
	}
	return count;
}


/* --------------------- PRELOAD --------------------- */

static void preload_consts(identifier_t ident, double value)
{
	ast_node_t* node = calloc(1, sizeof(ast_node_t));
	if (!node) PANIC("Undefined unallocation");
	node->type = NODE_NUMBER;
	node->number = value;
	insert_new_variable(ident, node, 0);
}
void preload_defaults()
{
	for (int i = 0; i < countof(VARSET); i++) VARSET[i].mut = 1;

	preload_consts((identifier_t) { .type = IDENTIFIER_SYMBOL, .symbol = 'e', .subscript = -1 },
		2.71828182845904523536028747135);
	preload_consts((identifier_t) { .type = IDENTIFIER_ALPHA, .alpha = get_alpha_name("pi"), .subscript = -1 },
		3.14159265358979323846264338328);
	preload_consts((identifier_t) { .type = IDENTIFIER_ALPHA, .alpha = get_alpha_name("tau"), .subscript = -1 },
		6.283185307179586476925286766559);
	preload_consts((identifier_t) { .type = IDENTIFIER_ALPHA, .alpha = get_alpha_name("phi"), .subscript = -1 },
		1.61803398874989484820458683436);
	preload_consts((identifier_t) { .type = IDENTIFIER_ALPHA, .alpha = get_alpha_name("gamma"), .subscript = -1 },
		-0.57721566490153286060651209008);

	declare_sfuncs();
	declare_lfuncs();
}


/* --------------------- MAIN --------------------- */

int execute(const struct ast_node* root, struct compresult* cr)
{
	if (root->type == NODE_ASSIGNMENT)
	{
		if (root->left->type == NODE_VARIABLE)
		{
			insert_new_variable(root->left->ident, root->right, 1);
			return 0;
		}
		else if (root->left->type == NODE_DFUNCTION)
		{
			if (insert_new_dfunc(root->left->ident, root->left->left, root->right))
			{
				cr->error = ERROR_COMPUTE_EXPECTED_ONLY_VARIABLES;
				return 1;
			}
			return 0;
		}
		else
		{
			cr->error = ERROR_COMPUTE_EXPECTED_REGULAR_DECLARATION;
			return 1;
		}
	}
	else
	{
		*cr = compute_node(root, 0);
		return 1;
	}
}
