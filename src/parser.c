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

#define NEW_NODE() calloc(1, sizeof(ast_node_t))

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
static int parse_identifier(context_t ctx);
static int parse_variable(context_t ctx);
static int parse_primary(context_t ctx);
static int parse_power(context_t ctx);
static int parse_factor(context_t ctx);
static int parse_term(context_t ctx);
static int parse_expression(context_t ctx);
static int parse_func_params(context_t ctx);
static int parse_func_decl(context_t ctx);
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

static int parse_identifier(context_t ctx)
{
	if (ctx.current[0].type == TOKEN_ALPHA)
	{
		ctx.node->ident.type = IDENTIFIER_SYMBOLIC;
		ctx.node->ident.sym = ctx.current[0].data[0];
	}
	else if (ctx.current[0].type == TOKEN_LITERAL)
	{
		ctx.node->ident.type = IDENTIFIER_LITERAL;
		ctx.node->ident.litid = ctx.current[0].data[0];
	}
	else return 0;

	if (ctx.remaining >= 1 && ctx.current[1].type == TOKEN_NUMBER)
	{
		ctx.node->ident.subscript = atoi(ctx.current[1].data);
		return 2;
	}
	else
	{
		ctx.node->ident.subscript = -1;
		return 1;
	}
}
static int parse_variable(context_t ctx)
{
	ctx.node->type = NODE_VARIABLE;
	int skip = parse_identifier(ctx);
	if (ctx.current[0].type == TOKEN_LITERAL && get_literal(ctx.node->ident.litid).type != LITERAL_CONST)
		return 0;
	else return skip;
}

static int parse_func_params(context_t ctx)
{
	int skip = -1;
	ast_node_t* tempnode = ctx.node;
	do
	{
		skip++;

		ast_node_t* exprnode = NEW_NODE();
		if (!exprnode) return -1;
		context_t exprctx = { .current = ctx.current + skip, .remaining = ctx.remaining - skip, .node = exprnode };
		int exprskip = parse_expression(exprctx);

		if (exprskip <= 0)
		{
			if (ctx.node->right) annihilate_tree(ctx.node->right);
			return exprskip;
		}

		tempnode->type = NODE_FUNC_PARAM_JOINT;
		tempnode->left = exprnode;
		ast_node_t* newtempnode = NEW_NODE();
		if (!newtempnode) return -1;
		tempnode->right = newtempnode;
		tempnode = newtempnode;

		skip += exprskip;
	} while (skip <= ctx.remaining && ctx.current[skip].type == TOKEN_COMMA);

	ast_node_t* temp = ctx.node;
	while (temp->right->type == NODE_FUNC_PARAM_JOINT)
		temp = temp->right;

	free(temp->right);
	temp->right = NULL;

	return skip;
}
static int parse_function(context_t ctx)
{
	ctx.node->type = NODE_FUNC_CALL;

	if (ctx.current[0].type == TOKEN_LITERAL)
	{
		literal_t lit = get_literal(ctx.current[0].data[0]);
		if (lit.type == LITERAL_FUNCTION && lit.argcount == 1)	// std-ufl
		{
			ast_node_t* argnode = NEW_NODE();
			context_t argctx = { .current = ctx.current + 1, .remaining = ctx.remaining - 1, .node = argnode };
			int skip = parse_primary(argctx);

			if (skip < 0)
			{
				annihilate_tree(argnode);
				return skip;
			}
			else if (skip > 0)
			{
				ast_node_t* jointnode = NEW_NODE();
				if (!jointnode) return -1;
				jointnode->type = NODE_FUNC_PARAM_JOINT;
				jointnode->left = argnode;
				jointnode->right = NULL;

				ctx.node->ident.type = IDENTIFIER_LITERAL;
				ctx.node->ident.litid = ctx.current[0].data[0];
				ctx.node->ident.subscript = -1;
				ctx.node->left = jointnode;
				return skip + 1;
			}
			else
				annihilate_tree(argnode);
		}
	}

	int skip = parse_identifier(ctx);   // func-ident
	if (skip <= 0) return skip;

	if (!does_function_exist(ctx.node->ident))
		return 0;
	if (ctx.current[skip].type != TOKEN_BRACKET || ctx.current[skip].data[0] != '(')
		return 0;

	skip++;

	ast_node_t* paramsnode = NEW_NODE();
	context_t paramsctx = { .current = ctx.current + skip, .remaining = ctx.remaining - skip, .node = paramsnode };
	int paramsskip = parse_func_params(paramsctx);
	if (paramsskip <= 0)
	{
		annihilate_tree(paramsnode);
		return paramsskip;
	}
	
	skip += paramsskip;

	if (ctx.current[skip].type != TOKEN_BRACKET || ctx.current[skip].data[0] != ')')
		return ERROR_PARSER_UNCLOSED_BRACKET;

	ctx.node->left = paramsnode;
	ctx.node->right = NULL;

	return skip + 1;
}


static int parse_primary(context_t ctx)
{
	int skip = 0;
	if ((skip = parse_number(ctx))) return skip;
	else if ((skip = parse_function(ctx))) return skip;
	else if ((skip = parse_variable(ctx))) return skip;
	else if (ctx.current[0].type == TOKEN_BRACKET && ctx.current[0].data[0] == '(')
	{
		context_t exprctx = { .current = ctx.current + 1, .remaining = ctx.remaining - 1, .node = ctx.node };
		skip = 1 + parse_expression(exprctx);
		if (skip <= 1) return skip - 1;
		if (ctx.current[skip].type == TOKEN_BRACKET && ctx.current[skip].data[0] == ')') return skip + 1;
		else return ERROR_PARSER_UNCLOSED_BRACKET;
	}
	else if (ctx.current[0].type == TOKEN_BAR)
	{
		ast_node_t* exprnode = NEW_NODE();
		context_t exprctx = { .current = ctx.current + 1, .remaining = ctx.remaining - 1, .node = exprnode };
		skip = 1 + parse_expression(exprctx);
		if (skip <= 1) return skip - 1;
		if (ctx.current[skip].type == TOKEN_BAR)
		{
			ctx.node->type = NODE_ABS;
			ctx.node->left = exprnode;
			return skip + 1;
		}
		else return ERROR_PARSER_UNCLOSED_BRACKET;
	}
	return 0;
}

static int parse_power(context_t ctx)
{
	ast_node_t* tempnode = NEW_NODE();
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
		ast_node_t* expnode = NEW_NODE();
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
		ast_node_t* newnode = NEW_NODE();
		if (!newnode) return -1;
		ctx.node->type = NODE_UNARY_MINUS;
		ctx.node->left = newnode;

		context_t newctx = { .current = ctx.current + 1, .remaining = ctx.remaining - 1, .node = newnode };
		int skip = parse_power(newctx);
		return skip <= 0 ? skip : (skip + 1);
	}
	return parse_power(ctx);
}

static int parse_term(context_t ctx)
{
	ast_node_t* firstnode = NEW_NODE();
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
		ast_node_t* factornode = NEW_NODE();
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
		else if ((tempskip = parse_power((context_t) {
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

		ast_node_t* newtempnode = NEW_NODE();
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
	ast_node_t* firstnode = NEW_NODE();
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
		ast_node_t* termnode = NEW_NODE();
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

		ast_node_t* newtempnode = NEW_NODE();
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

static int parse_func_decl(context_t ctx)
{
	int identskip = parse_identifier(ctx);
	if (identskip <= 0) return identskip;

	int skip = identskip;
	if (ctx.current[skip].type == TOKEN_BRACKET && ctx.current[skip].data[0] == '(')
	{
		skip++;

		ast_node_t* paramsnode = NEW_NODE();
		if (!paramsnode) return -1;
		context_t paramsctx = { .current = ctx.current + skip, .remaining = ctx.remaining - skip, .node = paramsnode };
		
		int paramsskip = parse_func_params(paramsctx);
		if (paramsskip < 0)
		{
			annihilate_tree(paramsnode);
			return paramsskip;
		}
		else if (paramsskip == 0) return 0;

		skip += paramsskip;

		if (ctx.current[skip].type != TOKEN_BRACKET || ctx.current[skip].data[0] != ')')
		{
			annihilate_tree(paramsnode);
			return ERROR_PARSER_UNCLOSED_BRACKET;
		}
		skip++;

		ctx.node->type = NODE_FUNC_CALL;
		ctx.node->left = paramsnode;
	}
	else return 0;

	return skip;
}
static int parse_statement(context_t ctx)
{
	ast_node_t* defnode = NEW_NODE();
	if (!defnode) return -1;
	context_t defctx = ctx;
	defctx.node = defnode;

	int skip = parse_func_decl(defctx);
	if (skip <= 0)
		skip = parse_variable(defctx);

	if (skip < 0)
	{
		annihilate_tree(defnode);
		return skip;
	}

	if (skip > 0 && ctx.current[skip].type == TOKEN_EQUAL)
	{
		skip++;

		ast_node_t* exprnode = NEW_NODE();
		if (!exprnode)
		{
			annihilate_tree(defnode);
			return -1;
		}

		context_t exprctx = { .current = ctx.current + skip, .remaining = ctx.remaining - skip, .node = exprnode };
		int exprskip = parse_expression(exprctx);
		if (exprskip <= 0)
		{
			annihilate_tree(defnode);
			annihilate_tree(exprnode);
			return exprskip ? exprskip : ERROR_PARSER_EXPECTED_DEFINITION;
		}

		skip += exprskip;

		ctx.node->type = NODE_ASSIGNMENT;
		ctx.node->left = defnode;
		ctx.node->right = exprnode;

		return skip;
	}

	annihilate_tree(defnode);
	return parse_expression(ctx);
}

int parse_content(const token_t* tokens, int count, ast_node_t* root)
{
	return parse_statement((context_t) { .current = tokens, .remaining = count - 1, .node = root });
}


/*static void debug_print_tree(ast_node_t* node, int indent)
{
	for (int i = 0; i < indent; i++)
		putchar('\t');
	
	switch (node->type)
	{
	case NODE_ASSIGNMENT:
		debug_print_tree(node->left, indent);
		printf("=");
		debug_print_tree(node->right, indent + 1);
		break;
	case NODE_NUMBER:
		printf("[NUM]: %.f\n", node->number); break;
	case NODE_VARIABLE:
		if (node->ident.type == IDENTIFIER_SYMBOLIC)
			printf("[VAR]: %c", node->ident.sym);
		else if (node->ident.type == IDENTIFIER_LITERAL)
			printf("[VAR]: %s", get_literal(node->ident.litid).name);

		if (node->ident.subscript != -1)
			printf("%d\n", node->ident.subscript);
		else putchar('\n');
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
	case NODE_ABS:
		printf("||:\n");
		debug_print_tree(node->left, indent + 1);
		break;
	case NODE_FUNC_CALL:
		if (node->ident.type == IDENTIFIER_SYMBOLIC)
			printf("<%c", node->ident.sym);
		else if (node->ident.type == IDENTIFIER_LITERAL)
			printf("<%s", get_literal(node->ident.litid).name);

		if (node->ident.subscript != -1)
			printf("%d>:\n", node->ident.subscript);
		else printf(">:\n");

		debug_print_tree(node->left, indent + 1);
		break;
	case NODE_FUNC_PARAM_JOINT:
		printf("{J}:\n");
		debug_print_tree(node->left, indent + 1);
		if (node->right) debug_print_tree(node->right, indent + 1);
		else
		{
			for (int i = 0; i < indent; i++) putchar('\t');
			printf("{END}\n");
		}
		break;
	}
}*/
