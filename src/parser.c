#include "cical.h"

#define _CRT_SECURE_NO_WARNINGS
#include <string.h>
#include <stdio.h>
#include <stdlib.h>


void annihilate_tree(ast_node_t* node)
{
	if (node->left) annihilate_tree(node->left);
	if (node->right) annihilate_tree(node->right);
	free(node);
}

typedef struct
{
	int remaining;		// how much tokens AFTER current is valid
	token_t* current;
	struct ast_node* node;
} context_t;

// parse ->
// +: skip
// 0: failed
// -: error

static int parse_number(context_t ctx);
static int parse_variable(context_t ctx);
static int parse_primary(context_t ctx);
static int parse_super(context_t ctx);
static int parse_factor(context_t ctx);
static int parse_term(context_t ctx);
static int parse_expression(context_t ctx);
static int parse_function(context_t ctx);
static int parse_statement(context_t ctx);


static int parse_number(context_t ctx)
{
	if (ctx.current[0].type != TOKEN_NUMBER) return 0;

	int skip = 1;

	char buf[TOKEN_DATA_SIZE * 2 + 1 + 1] = { 0 };
	int off = sprintf(buf, "%s", ctx.current->data);

	if (ctx.remaining >= 2 && ctx.current[1].type == TOKEN_DOT)
	{
		if (ctx.current[2].type != TOKEN_NUMBER) return ERROR_PARSER_EXPECTED_NUMBER_AFTER_DOT;
		buf[off] = '.';
		off += sprintf(buf + off + 1, "%s", ctx.current[2].data);
		skip = 3;
	}

	ctx.node->type = NODE_NUMBER;
	ctx.node->number = atof(buf);
	return skip;
}

static int parse_variable(context_t ctx)
{
	if (ctx.current[0].type == TOKEN_ALPHA)
	{
		int skip = 1;

		ctx.node->type = NODE_VARIABLE;
		ctx.node->variable.sym = ctx.current[0].data[0];
		if (ctx.remaining >= 1 && ctx.current[1].type == TOKEN_NUMBER)
		{
			ctx.node->variable.ident = atoi(ctx.current[1].data);
			skip = 2;
		}
		else ctx.node->variable.ident = -1;

		return skip;
	}
	else if (ctx.current[0].type == TOKEN_LITERAL && get_literal(ctx.current[0].data[0]).type == LITERAL_CONST)
	{
		ctx.node->type = NODE_CONST;
		ctx.node->variable.sym = 0xff;
		ctx.node->variable.ident = ctx.current[0].data[0];
		return 1;
	}
	else return 0;
}

static int parse_primary(context_t ctx)
{
	int skip = 0;
	if ((skip = parse_number(ctx))) return skip;
	else if ((skip = parse_variable(ctx))) return skip;
	else if ((skip = parse_function(ctx))) return skip;
	else if (ctx.current[0].type == TOKEN_BRACKET && ctx.current[0].data[0] == '(')
	{
		context_t exprctx = { .current = ctx.current + 1, .remaining = ctx.remaining - 1, .node = ctx.node };
		skip = parse_expression(exprctx);
		if (skip <= 0) return skip;
		if (ctx.current[skip + 1].type == TOKEN_BRACKET && ctx.current[skip + 1].data[0] == ')') return skip + 2;
		else return ERROR_PARSER_UNCLOSED_BRACKET;
	}
	return 0;
}

static int parse_super(context_t ctx)
{
	ast_node_t* tempnode = calloc(1, sizeof(ast_node_t));
	if (!tempnode) return -1;
	context_t tempctx = ctx;
	tempctx.node = tempnode;

	int skip = parse_primary(tempctx);

	if (skip <= 0 || skip >= ctx.remaining)
	{
		*ctx.node = *tempnode;
		free(tempnode);
		return skip;
	}

	token_t* cur = ctx.current + skip;
	if (cur->type == TOKEN_OPERATION && cur->data[0] == '^')
	{
		ast_node_t* expnode = calloc(1, sizeof(ast_node_t));
		if (!expnode) return -1;

		context_t expctx = { .current = cur + 1, .remaining = ctx.remaining - skip - 1, .node = expnode };
		int expskip = parse_factor(expctx);

		if (expskip <= 0)
		{
			annihilate_tree(tempnode);
			annihilate_tree(expnode);
			return expskip;
		}
		skip += expskip + 1;

		ctx.node->type = NODE_POW;
		ctx.node->left = tempnode;
		ctx.node->right = expnode;
		return skip;
	}

	*ctx.node = *tempnode;
	free(tempnode);
	return skip;
}

static int parse_factor(context_t ctx)
{
	if (ctx.current[0].type == TOKEN_OPERATION && ctx.current[0].data[0] == '-')
	{
		ast_node_t* newnode = calloc(1, sizeof(ast_node_t));
		if (!newnode) return -1;
		ctx.node->type = NODE_UNARY_MINUS;
		ctx.node->left = newnode;

		context_t newctx = { .current = ctx.current + 1, .remaining = ctx.remaining - 1, .node = newnode };
		int skip = parse_super(newctx);
		return skip <= 0 ? skip : (skip + 1);
	}
	return parse_super(ctx);
}

static int parse_term(context_t ctx)
{
	ast_node_t* firstnode = calloc(1, sizeof(ast_node_t));
	if (!firstnode) return -1;
	context_t firstctx = ctx;
	firstctx.node = firstnode;

	int skip = parse_factor(firstctx);

	if (skip <= 0 || skip > ctx.remaining)
	{
		*ctx.node = *firstnode;
		free(firstnode);
		return skip;
	}

	ast_node_t* tempnode = firstnode;

	while (skip <= ctx.remaining)
	{
		token_t* cur = ctx.current + skip;
		int mult = 0;
		ast_node_t* factornode = calloc(1, sizeof(ast_node_t));
		if (!factornode) return -1;

		int tempskip = 0;

		if (cur->type == TOKEN_OPERATION && (cur->data[0] == '*' || cur->data[0] == '/'))
		{
			context_t factorctx = { .current = cur + 1, .remaining = ctx.remaining - skip - 1, .node = factornode };
			tempskip = parse_factor(factorctx);
			if (tempskip <= 0)
			{
				annihilate_tree(tempnode);
				annihilate_tree(factornode);
				return tempskip;
			}

			skip += tempskip + 1;
			mult = cur->data[0] == '*';
		}
		else if ((tempskip = parse_super((context_t) {
			.current = cur,
			.node = factornode,
			.remaining = ctx.remaining - skip
		})) > 0)
		{
			skip += tempskip;
			mult = 1;
		}
		else
		{
			annihilate_tree(factornode);
			break;
		}

		ast_node_t* newtempnode = calloc(1, sizeof(ast_node_t));
		if (!newtempnode) return -1;
		newtempnode->type = mult ? NODE_MULTIPLY : NODE_DIVIDE;
		newtempnode->left = tempnode;
		newtempnode->right = factornode;
		tempnode = newtempnode;
	}

	*ctx.node = *tempnode;
	free(tempnode);
	return skip;
}

static int parse_expression(context_t ctx)
{
	ast_node_t* firstnode = calloc(1, sizeof(ast_node_t));
	if (!firstnode) return -1;
	context_t firstctx = ctx;
	firstctx.node = firstnode;

	int skip = parse_term(firstctx);

	if (skip <= 0 || skip > ctx.remaining)
	{
		*ctx.node = *firstnode;
		free(firstnode);
		return skip;
	}

	ast_node_t* tempnode = firstnode;

	while (skip <= ctx.remaining)
	{
		token_t* cur = ctx.current + skip;
		int add = 0;
		ast_node_t* termnode = calloc(1, sizeof(ast_node_t));
		if (!termnode) return -1;

		if (cur->type == TOKEN_OPERATION && (cur->data[0] == '+' || cur->data[0] == '-'))
		{
			context_t termctx = { .current = cur + 1, .remaining = ctx.remaining - skip - 1, .node = termnode };
			int termskip = parse_term(termctx);
			if (termskip <= 0)
			{
				annihilate_tree(tempnode);
				annihilate_tree(termnode);
				return termskip;
			}

			skip += termskip + 1;
			add = cur->data[0] == '+';
		}
		else
		{
			annihilate_tree(termnode);
			break;
		}

		ast_node_t* newtempnode = calloc(1, sizeof(ast_node_t));
		if (!newtempnode) return -1;
		newtempnode->type = add ? NODE_ADD : NODE_SUBTRACT;
		newtempnode->left = tempnode;
		newtempnode->right = termnode;
		tempnode = newtempnode;
	}

	*ctx.node = *tempnode;
	free(tempnode);
	return skip;
}

static int parse_function(context_t ctx)
{
	if (ctx.current[0].type != TOKEN_LITERAL) return 0;
	if (ctx.remaining < 1 || ctx.current[1].type == TOKEN_OPERATION) return ERROR_PARSER_EXPECTED_ARGUMENT;

	ctx.node->type = NODE_FUNCCALL;
	ctx.node->funcidx = ctx.current->data[0];

	ast_node_t* argnode = calloc(1, sizeof(ast_node_t));
	context_t primctx = { .current = ctx.current + 1, .remaining = ctx.remaining - 1, .node = argnode };
	int skip = parse_primary(primctx);
	if (skip <= 0)
	{
		annihilate_tree(argnode);
		return skip;
	}
	else
	{
		ctx.node->left = argnode;
		return skip + 1;
	}
}

static int parse_statement(context_t ctx)
{
	ast_node_t* varnode = calloc(1, sizeof(ast_node_t));
	if (!varnode) return -1;
	context_t varctx = ctx;
	varctx.node = varnode;

	int varskip = parse_variable(varctx);
	int skip = varskip;

	if (varskip > 0 && ctx.current[varskip].type == TOKEN_EQUAL)
	{
		ast_node_t* exprnode = calloc(1, sizeof(ast_node_t));
		if (!exprnode) return -1;
		context_t exprctx = { .current = ctx.current + varskip + 1, .remaining = ctx.remaining - varskip - 1, .node = exprnode };
		int exprskip = parse_expression(exprctx);
		if (exprskip <= 0)
		{
			annihilate_tree(varnode);
			annihilate_tree(exprnode);
			return exprskip;
		}
		skip += exprskip + 1;

		ctx.node->type = NODE_ASSIGNMENT;
		ctx.node->left = varnode;
		ctx.node->right = exprnode;
	}
	else
	{
		annihilate_tree(varnode);
		return parse_expression(ctx);
	}

	return skip;
}

int parse_content(const token_t* tokens, int count, ast_node_t* root)
{
	return parse_statement((context_t) { .current = tokens, .remaining = count - 1, .node = root });
}


/*
static void debug_print_tree(ast_node_t* node, int indent)
{
	for (int i = 0; i < indent; i++)
		putchar('\t');
	
	switch (node->type)
	{
	case NODE_NUMBER:
		printf("[NUM]: %.6f\n", node->number); break;
	case NODE_VARIABLE:
		if (node->variable.ident != -1)
			printf("[VAR]: %c%d\n", node->variable.sym, node->variable.ident);
		else
			printf("[VAR]: %c\n", node->variable.sym);
		break;
	case NODE_UNARY_MINUS:
		printf("[-]:\n");
		debug_print_tree(node->left, indent + 1);
		break;
	case NODE_POW:
		printf("[^]:\n");
		debug_print_tree(node->left, indent + 1);
		debug_print_tree(node->right, indent + 1);
		break;
	case NODE_MULTIPLY:
		printf("[*]:\n");
		debug_print_tree(node->left, indent + 1);
		debug_print_tree(node->right, indent + 1);
		break;
	case NODE_DIVIDE:
		printf("[/]:\n");
		debug_print_tree(node->left, indent + 1);
		debug_print_tree(node->right, indent + 1);
		break;
	case NODE_ADD:
		printf("[+]:\n");
		debug_print_tree(node->left, indent + 1);
		debug_print_tree(node->right, indent + 1);
		break;
	case NODE_SUBTRACT:
		printf("[-]:\n");
		debug_print_tree(node->left, indent + 1);
		debug_print_tree(node->right, indent + 1);
		break;
	case NODE_FUNCCALL:
		printf("<%s>:\n", get_literal(node->funcidx).name);
		debug_print_tree(node->left, indent + 1);
		break;
	case NODE_CONST:
		printf("[C]: %s\n", get_literal(node->variable.ident).name);
		break;
	case NODE_ASSIGNMENT:
		if (node->left->variable.ident != -1)
			printf("[VAR]: %c%d = \n", node->left->variable.sym, node->left->variable.ident);
		else
			printf("[VAR]: %c = \n", node->left->variable.sym);
		debug_print_tree(node->right, indent);
		break;
	}
}
*/
