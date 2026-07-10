#include "parser.h"
#include "lexer.h"


#define NEW_NODE() calloc(1, sizeof(ast_node_t))
#define VALID_NODE(name) if (!name) PANIC("Unexpected unallocation")


typedef struct
{
	token_t* current;
	ast_node_t* node;
} context_t;
static context_t subctx(context_t old, int skip, ast_node_t* node)
{
	return (context_t) { .current = old.current + skip, .node = node };
}

static int parse_number(context_t ctx);
static int parse_prevanswer(context_t ctx);
static int parse_identifier(context_t ctx);
static int parse_sfunction(context_t ctx);
static int parse_lfunction(context_t ctx);
static int parse_dfunction(context_t ctx);
static int parse_arguments(context_t ctx);
static int parse_function(context_t ctx);
static int parse_variable(context_t ctx);
static int parse_primary(context_t ctx);
static int parse_multiple(context_t ctx);
static int parse_power(context_t ctx);
static int parse_super(context_t ctx);
static int parse_factor(context_t ctx);
static int parse_term(context_t ctx);
static int parse_expression(context_t ctx);
static int parse_definable(context_t ctx);
static int parse_statement(context_t ctx);

static int parse_number(context_t ctx)
{
	if (ctx.current[0].type != TOKEN_DIGITS) return 0;

	int skip = 1;

	ctx.node->type = NODE_NUMBER;
	ctx.node->number = strtod(ctx.current[0].data, NULL);

	if (ctx.current[skip].type == TOKEN_DOT)
	{
		if (ctx.current[++skip].type == TOKEN_DIGITS)
		{
			const char* frac_str = ctx.current[skip].data;
			int power = -(int)strlen(frac_str);
			double frac = strtod(frac_str, NULL);
			ctx.node->number += frac * pow(10.0, power);
			skip++;
		}
		else return ERROR_PARSER_EXPECTED_NUMBER_AFTER_DOT;
	}

	return skip;
}
static int parse_prevanswer(context_t ctx)
{
	if (ctx.current[0].type != TOKEN_AT) return 0;
	ctx.node->type = NODE_PREVANSWER;
	return 1;
}
static int parse_identifier(context_t ctx)
{
	if (ctx.current[0].type == TOKEN_SYMBOL)
	{
		ctx.node->ident.type = IDENTIFIER_SYMBOL;
		ctx.node->ident.symbol = (void*)ctx.current[0].data;
	}
	else if (ctx.current[0].type == TOKEN_ALPHA)
	{
		ctx.node->ident.type = IDENTIFIER_ALPHA;
		ctx.node->ident.alpha = ctx.current[0].data;
	}
	else return 0;

	int skip = 1;

	if (ctx.current[skip].type == TOKEN_DIGITS)
	{
		ctx.node->ident.subscript = atoi(ctx.current[skip].data);
		skip++;
	}
	else ctx.node->ident.subscript = -1;

	return skip;
}
static int parse_sfunction(context_t ctx)
{
	if (ctx.current[0].type != TOKEN_SFUNC) return 0;

	int skip = 1;

	ast_node_t* primenode = NEW_NODE();
	VALID_NODE(primenode);
	int primeskip = parse_primary(subctx(ctx, 1, primenode));
	if (primeskip <= 0)
	{
		annihilate_tree(primenode);
		return primeskip ? primeskip : ERROR_PARSER_EXPECTED_ARGUMENT;
	}

	skip += primeskip;

	ctx.node->type = NODE_SFUNCTION;
	ctx.node->sfunc = ctx.current[0].data;
	ctx.node->left = primenode;
	ctx.node->right = NULL;

	return skip;
}
static int parse_lfunction(context_t ctx)
{
	if (ctx.current[0].type != TOKEN_LFUNC) return 0;

	int skip = 1;

	if (!(ctx.current[skip].type == TOKEN_BRACKET && ctx.current[skip].data == '('))
		return ERROR_PARSER_EXPECTED_ARGUMENT;

	skip++;

	ast_node_t* argsnode = NEW_NODE();
	VALID_NODE(argsnode);
	int argsskip = parse_arguments(subctx(ctx, skip, argsnode));
	if (argsskip <= 0)
	{
		annihilate_tree(argsnode);
		return argsskip;
	}

	skip += argsskip;

	if (!(ctx.current[skip].type == TOKEN_BRACKET && ctx.current[skip].data == ')'))
		return ERROR_PARSER_UNCLOSED_BRACKET;
	skip++;

	ctx.node->type = NODE_LFUNCTION;
	ctx.node->lfunc = ctx.current[0].data;
	ctx.node->left = argsnode;
	ctx.node->right = NULL;

	return skip;
}
static int parse_dfunction(context_t ctx)
{
	int identskip = parse_identifier(ctx);
	if (identskip <= 0) return identskip;

	if (!get_dfunc(ctx.node->ident)) return 0;

	int skip = identskip;

	if (!(ctx.current[skip].type == TOKEN_BRACKET && ctx.current[skip].data == '('))
		return 0;

	skip++;

	ast_node_t* argsnode = NEW_NODE();
	VALID_NODE(argsnode);
	int argsskip = parse_arguments(subctx(ctx, skip, argsnode));
	if (argsskip <= 0)
	{
		annihilate_tree(argsnode);
		return argsskip;
	}

	skip += argsskip;

	if (!(ctx.current[skip].type == TOKEN_BRACKET && ctx.current[skip].data == ')'))
		return ERROR_PARSER_UNCLOSED_BRACKET;
	skip++;

	ctx.node->type = NODE_DFUNCTION;
	ctx.node->left = argsnode;
	ctx.node->right = NULL;

	return skip;
}
static int parse_arguments(context_t ctx)
{
	int skip = -1;

	ast_node_t* tempnode = ctx.node;
	do
	{
		skip++;

		ast_node_t* exprnode = NEW_NODE();
		VALID_NODE(exprnode);
		int exprskip = parse_expression(subctx(ctx, skip, exprnode));
		if (exprskip <= 0)
		{
			annihilate_tree(exprnode);
			return exprskip;
		}

		skip += exprskip;

		ast_node_t* newtempnode = NEW_NODE();
		VALID_NODE(newtempnode);
		tempnode->type = NODE_ARGUMENT_JOINT;
		tempnode->left = exprnode;
		tempnode->right = newtempnode;
		tempnode = newtempnode;
	} while (ctx.current[skip].type == TOKEN_COMMA);

	/* remove dummy node */
	ast_node_t* temp = ctx.node;
	while (temp->right->type == NODE_ARGUMENT_JOINT) temp = temp->right;
	free(temp->right);
	temp->right = NULL;

	return skip;
}
static int parse_function(context_t ctx)
{
	int skip = 0;
	if ((skip = parse_dfunction(ctx))) return skip;
	else if ((skip = parse_lfunction(ctx))) return skip;
	else if ((skip = parse_sfunction(ctx))) return skip;
	else return skip;
}
static int parse_variable(context_t ctx)
{
	int skip = parse_identifier(ctx);
	if (skip <= 0) return skip;
	ctx.node->type = NODE_VARIABLE;
	return skip;
}
static int parse_primary(context_t ctx)
{
	int skip = 0;
	if ((skip = parse_number(ctx))) return skip;
	else if ((skip = parse_function(ctx))) return skip;
	else if ((skip = parse_variable(ctx))) return skip;
	else if ((skip = parse_prevanswer(ctx))) return skip;
	else if (ctx.current[0].type == TOKEN_BRACKET && ctx.current[0].data == '(')
	{
		skip++;

		ast_node_t* exprnode = NEW_NODE();
		VALID_NODE(exprnode);
		int exprskip = parse_expression(subctx(ctx, 1, exprnode));
		if (exprskip <= 0)
		{
			annihilate_tree(exprnode);
			return exprskip;
		}

		skip += exprskip;

		if (!(ctx.current[skip].type == TOKEN_BRACKET && ctx.current[skip].data == ')'))
			return ERROR_PARSER_UNCLOSED_BRACKET;

		*ctx.node = *exprnode;
		free(exprnode);
		return skip + 1;
	}
	else if (ctx.current[0].type == TOKEN_BAR)
	{
		skip++;

		ast_node_t* exprnode = NEW_NODE();
		VALID_NODE(exprnode);
		int exprskip = parse_expression(subctx(ctx, 1, exprnode));
		if (exprskip <= 0)
		{
			annihilate_tree(exprnode);
			return exprskip;
		}

		skip += exprskip;

		if (ctx.current[skip].type != TOKEN_BAR)
			return ERROR_PARSER_UNCLOSED_BRACKET;

		ctx.node->type = NODE_ABS;
		ctx.node->left = exprnode;
		ctx.node->right = NULL;
		return skip + 1;
	}
	else return skip;
}
static int parse_multiple(context_t ctx)
{
	ast_node_t* primenode = NEW_NODE();
	VALID_NODE(primenode);
	int primeskip = parse_primary(subctx(ctx, 0, primenode));
	if (primeskip <= 0)
	{
		annihilate_tree(primenode);
		return primeskip;
	}

	int skip = primeskip;

	if (ctx.current[skip].type == TOKEN_OPERATION && ctx.current[skip].data == '^')
	{
		skip++;
		
		ast_node_t* powernode = NEW_NODE();
		VALID_NODE(powernode);
		int powerctx = parse_power(subctx(ctx, skip, powernode));
		if (powerctx <= 0)
		{
			annihilate_tree(powernode);
			annihilate_tree(primenode);
			return powerctx;
		}

		skip += powerctx;
		ctx.node->type = NODE_POW;
		ctx.node->left = primenode;
		ctx.node->right = powernode;
	}
	else
	{
		*ctx.node = *primenode;
		free(primenode);
	}
	return skip;
}
static int parse_power(context_t ctx)
{
	if (ctx.current[0].type == TOKEN_OPERATION && ctx.current[0].data == '-')
	{
		ast_node_t* multnode = NEW_NODE();
		VALID_NODE(multnode);
		int multskip = parse_multiple(subctx(ctx, 1, multnode));
		if (multskip <= 0)
		{
			annihilate_tree(multnode);
			return multskip;
		}
		
		ctx.node->type = NODE_NEGATE;
		ctx.node->left = multnode;
		ctx.node->right = NULL;
		return multskip + 1;
	}
	else return parse_multiple(ctx);
}
static int parse_super(context_t ctx)
{
	ast_node_t* initmultnode = NEW_NODE();
	VALID_NODE(initmultnode);
	int initmultskip = parse_multiple(subctx(ctx, 0, initmultnode));
	if (initmultskip <= 0)
	{
		annihilate_tree(initmultnode);
		return initmultskip;
	}

	int skip = initmultskip;

	ast_node_t* tempnode = initmultnode;
	while (1)
	{
		ast_node_t* multnode = NEW_NODE();
		VALID_NODE(multnode);
		int multskip = parse_multiple(subctx(ctx, skip, multnode));
		if (multskip == 0)
		{
			annihilate_tree(multnode);
			break;
		}
		else if (multskip < 0)
		{
			annihilate_tree(multnode);
			annihilate_tree(tempnode);
			return multskip;
		}

		skip += multskip;

		ast_node_t* newtempnode = NEW_NODE();
		VALID_NODE(newtempnode);
		newtempnode->type = NODE_MULTIPLY;
		newtempnode->left = tempnode;
		newtempnode->right = multnode;
		tempnode = newtempnode;
	}

	*ctx.node = *tempnode;
	free(tempnode);
	return skip;
}
static int parse_factor(context_t ctx)
{
	if (ctx.current[0].type == TOKEN_OPERATION && ctx.current[0].data == '-')
	{
		ast_node_t* supernode = NEW_NODE();
		VALID_NODE(supernode);
		int superskip = parse_super(subctx(ctx, 1, supernode));
		if (superskip <= 0)
		{
			annihilate_tree(supernode);
			return superskip;
		}

		ctx.node->type = NODE_NEGATE;
		ctx.node->left = supernode;
		ctx.node->right = NULL;
		return superskip + 1;
	}
	else return parse_super(ctx);
}
static int parse_term(context_t ctx)
{
	ast_node_t* initfactornode = NEW_NODE();
	VALID_NODE(initfactornode);
	int initfactorskip = parse_factor(subctx(ctx, 0, initfactornode));
	if (initfactorskip <= 0)
	{
		annihilate_tree(initfactornode);
		return initfactorskip;
	}

	int skip = initfactorskip;

	ast_node_t* tempnode = initfactornode;
	while (ctx.current[skip].type == TOKEN_OPERATION && 
		(ctx.current[skip].data == '*' || ctx.current[skip].data == '/'))
	{
		int multop = ctx.current[skip].data == '*';

		skip++;

		ast_node_t* factornode = NEW_NODE();
		VALID_NODE(factornode);
		int factorskip = parse_factor(subctx(ctx, skip, factornode));
		if (factorskip <= 0)
		{
			annihilate_tree(factornode);
			annihilate_tree(tempnode);
			return factorskip;
		}

		skip += factorskip;

		ast_node_t* newtempnode = NEW_NODE();
		VALID_NODE(newtempnode);
		newtempnode->type = multop ? NODE_MULTIPLY : NODE_DIVIDE;
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
	ast_node_t* inittermnode = NEW_NODE();
	VALID_NODE(inittermnode);
	int inittermskip = parse_term(subctx(ctx, 0, inittermnode));
	if (inittermskip <= 0)
	{
		annihilate_tree(inittermnode);
		return inittermskip;
	}

	int skip = inittermskip;

	ast_node_t* tempnode = inittermnode;
	while (ctx.current[skip].type == TOKEN_OPERATION &&
		(ctx.current[skip].data == '+' || ctx.current[skip].data == '-'))
	{
		int addop = ctx.current[skip].data == '+';

		skip++;

		ast_node_t* termnode = NEW_NODE();
		VALID_NODE(termnode);
		int termskip = parse_term(subctx(ctx, skip, termnode));
		if (termskip <= 0)
		{
			annihilate_tree(termnode);
			annihilate_tree(tempnode);
			return termskip;
		}

		skip += termskip;

		ast_node_t* newtempnode = NEW_NODE();
		VALID_NODE(newtempnode);
		newtempnode->type = addop ? NODE_ADD : NODE_SUBTRACT;
		newtempnode->left = tempnode;
		newtempnode->right = termnode;
		tempnode = newtempnode;
	}

	*ctx.node = *tempnode;
	free(tempnode);
	return skip;
}
static int parse_definable(context_t ctx)
{
	int identskip = parse_identifier(ctx);
	if (identskip <= 0) return identskip;

	int skip = identskip;

	if (!(ctx.current[skip].type == TOKEN_BRACKET && ctx.current[skip].data == '('))
	{
		ctx.node->type = NODE_VARIABLE;
		return skip;
	}

	skip++;

	ast_node_t* argsnode = NEW_NODE();
	VALID_NODE(argsnode);
	int argsskip = parse_arguments(subctx(ctx, skip, argsnode));
	if (argsskip <= 0)
	{
		annihilate_tree(argsnode);
		return argsskip;
	}

	skip += argsskip;

	if (!(ctx.current[skip].type == TOKEN_BRACKET && ctx.current[skip].data == ')'))
		return ERROR_PARSER_UNCLOSED_BRACKET;
	skip++;

	ctx.node->type = NODE_DFUNCTION;
	ctx.node->left = argsnode;
	ctx.node->right = NULL;
	return skip;
}
static int parse_statement(context_t ctx)
{
	ast_node_t* defnode = NEW_NODE();
	VALID_NODE(defnode);
	int defskip = parse_definable(subctx(ctx, 0, defnode));
	if (defskip <= 0 || ctx.current[defskip].type != TOKEN_EQUAL)
	{
		annihilate_tree(defnode);

		int skip = parse_expression(ctx);
		if (skip <= 0) return skip;
		else return ctx.current[skip].type == TOKEN_EOL ? 
			skip + 1 : ERROR_PARSER_NOT_FINISHED_STATEMENT;
	}

	int skip = defskip + 1;

	ast_node_t* exprnode = NEW_NODE();
	VALID_NODE(exprnode);
	int exprskip = parse_expression(subctx(ctx, skip, exprnode));
	if (exprskip <= 0)
	{
		annihilate_tree(exprnode);
		annihilate_tree(defnode);
		return exprskip ? exprskip : ERROR_PARSER_EXPECTED_DEFINITION;
	}

	skip += exprskip;
	if (ctx.current[skip].type != TOKEN_EOL)
		return ERROR_PARSER_NOT_FINISHED_STATEMENT;
	skip++;

	ctx.node->type = NODE_ASSIGNMENT;
	ctx.node->left = defnode;
	ctx.node->right = exprnode;
	return skip;
}

int parse_content(const struct token* tokens, ast_node_t* root)
{
	return parse_statement((context_t) { tokens, root });
}

/*
static void debug_print_identifier(identifier_t i)
{
	if (i.type == IDENTIFIER_SYMBOL)
	{
		if (i.subscript == -1)
			printf("%c", i.symbol);
		else
			printf("%c%d", i.symbol, i.subscript);
	}
	else
	{
		if (i.subscript == -1)
			printf("%s", i.alpha->str);
		else
			printf("%s%d", i.alpha->str, i.subscript);
	}
}
void debug_print_tree(const ast_node_t* node, int indent)
{
	for (int i = 0; i < indent; i++)
		putchar('\t');

	if (!node)
	{
		printf("<0>\n");
		return;
	}

	switch (node->type)
	{
	case NODE_NUMBER:
		printf("[NUM]: %f\n", node->number);
		break;
	case NODE_VARIABLE:
		printf("[VAR]: ");
		debug_print_identifier(node->ident);
		putchar('\n');
		break;
	case NODE_POW:
		printf("[^]:\n");
		debug_print_tree(node->left, indent + 1);
		debug_print_tree(node->right, indent + 1);
		break;
	case NODE_NEGATE:
		printf("[-]:\n");
		debug_print_tree(node->left, indent + 1);
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
	case NODE_ASSIGNMENT:
		debug_print_tree(node->left, indent + 1);
		printf("=\n");
		debug_print_tree(node->right, indent + 1);
		break;
	case NODE_SFUNCTION:
		printf("<%s>:\n", node->sfunc->name);
		debug_print_tree(node->left, indent + 1);
		break;
	case NODE_LFUNCTION:
		printf("<%s>:\n", node->sfunc->name);
		debug_print_tree(node->left, indent + 1);
		break;
	case NODE_DFUNCTION:
		printf("<");
		debug_print_identifier(node->ident);
		printf(">:\n");
		debug_print_tree(node->left, indent + 1);
		break;
	case NODE_ARGUMENT_JOINT:
		printf("{J}:\n");
		debug_print_tree(node->left, indent + 1);
		debug_print_tree(node->right, indent + 1);
		break;
	}
}
*/
