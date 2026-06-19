#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "cical.h"

#include "lexer.h"
#include "parser.h"
#include "compute.h"


#define SILENT_PRINT "\x1b[30;1;3m"
#define NORMAL_PRINT "\x1b[0m"
#define PURPLE_PRINT "\x1b[35m"
#define YELLOW_PRINT "\x1b[33m"
#define RED_PRINT "\x1b[31m"

static void printerr(int code)
{
	printf(RED_PRINT "ERROR: %x, ", code);
	switch (code)
	{
	case ERROR_LEXER_UNDEFINED_SYMBOL: printf("Undefined symbol.\n"); break;
	case ERROR_PARSER_EXPECTED_NUMBER_AFTER_DOT: printf("Expected number in form N.N.\n"); break;
	case ERROR_PARSER_UNCLOSED_BRACKET: printf("Unclosed bracket found.\n"); break;
	case ERROR_PARSER_EXPECTED_ARGUMENT: printf("Function expected an argument.\n"); break;
	case ERROR_PARSER_EXPECTED_DEFINITION: printf("Expected definition.\n"); break;
	case ERROR_PARSER_NOT_FINISHED_STATEMENT: printf("Not finished statement.\n"); break;
	case ERROR_COMPUTE_UNDEFINED_VARIABLE: printf("Undefined variable.\n"); break;
	case ERROR_COMPUTE_UNEXPECTED_NAN: printf("Unexpected NaN.\n"); break;
	case ERROR_COMPUTE_UNDEFINED_OPERATION: printf("Undefined operation.\n"); break;
	case ERROR_COMPUTE_UNDEFINED_FUNCTION: printf("Undefined function.\n"); break;
	case ERROR_COMPUTE_TOO_FEW_ARGUMENTS: printf("Too few arguments for function.\n"); break;
	case ERROR_COMPUTE_EXPECTED_REGULAR_DECLARATION: printf("Expected variable declaration.\n"); break;
	case ERROR_COMPUTE_EXPECTED_ONLY_VARIABLES: printf("Expected only variables in function declaration.\n"); break;
	case -1: printf("somthing went wrong.\n"); break;
	default: printf("[UNDEFINED]\n");
	}
	printf(NORMAL_PRINT);
}

static void list_standrard(char c)
{
	if (c == 0 || c == 'c')
		printf(
			SILENT_PRINT
			"Constants: pi = 3.141.., e = 2.718.., phi = 1.618.., tau = 6.282.., gamma = -0.577...\n"
			"Greek alphabet: alpha, beta, gamma, mu, pi, rho, tau, phi\n"
			NORMAL_PRINT
		);
	if (c == 0 || c == 'f')
		printf(
			SILENT_PRINT
			"Functions:\n"
			"* General: sqrt, cbrt, ln (log base e), lg (log base 10), log, exp, erf (error function)\n"
			"* Trigonometry: [arc]sin[h], [arc]cos[h], [arc]tan[h], [arc]cot[h]\n"
			"* Misc: min, max, gcd, lcm, mod\n"
			NORMAL_PRINT
		);
}

static int PRECISION = 6;

#define DEFAULT_TOKENS_MAXCOUNT 1024
static token_t TOKENS[DEFAULT_TOKENS_MAXCOUNT] = { 0 };

static void handle_line(const char* line)
{
	int err = 0;

	int count = tokenize_line(line, TOKENS, DEFAULT_TOKENS_MAXCOUNT);
	if (count < 0) { err = count; goto end; }

	ast_node_t* node = calloc(1, sizeof(ast_node_t));
	if (!node) PANIC("Unexpected unallocation");
	err = parse_content(TOKENS, node);
	cleanup_tokens(TOKENS, DEFAULT_TOKENS_MAXCOUNT);
	if (err < 0) goto end;

	compresult_t cr = { 0 };
	if (execute(node, &cr))
	{
		err = cr.error;
		if (!err) printf(YELLOW_PRINT "= %.*f\n", PRECISION, cr.value);
		annihilate_tree(node);
	}
	else err = 0;

end:
	memset(TOKENS, 0, sizeof(TOKENS));
	if (err) printerr(err);
}


int main()
{
	preload_defaults();
	printf("Welcome to CICAL (C Interpretable Calculator)\n" SILENT_PRINT "Use !h for help\n" NORMAL_PRINT);

	char line[128] = { 0 };
	while (1)
	{
		printf(PURPLE_PRINT ">>>" NORMAL_PRINT " ");
		fgets(line, sizeof(line), stdin);
		if (line[0] == 0) break;

		line[strlen(line) - 1] = 0;

		if (line[0] == '!')
		{
			const char* command = line + 1;
			if (command[0] == 'h')
			{
				printf(
					SILENT_PRINT
					"Control commands:\n"
					"!q - Quit\n"
					"!c - Clear screen\n"
					"!p - Set output precision\n"
					"!u[f|v] - Un-define functions, variables\n"
					"!l[f|c] - List standard functions, constants\n"
					NORMAL_PRINT
				);
			}
			else if (command[0] == 'q')
			{
				printf(SILENT_PRINT "Quitting\n" NORMAL_PRINT);
				break;
			}
			else if (command[0] == 'c') system("cls");
			else if (command[0] == 'p')
			{
				const char* num = command + 1;
				while (*num == ' ') num++;
				PRECISION = atoi(num);
				printf(SILENT_PRINT "Set output precision to %d digits\n" NORMAL_PRINT, PRECISION);
			}
			else if (command[0] == 'u')
			{
				if (command[1] == 0 || command[1] == 'v')
					printf(SILENT_PRINT "Removed %d variables\n" NORMAL_PRINT, remove_variables());
				if (command[1] == 0 || command[1] == 'f')
					printf(SILENT_PRINT "Removed %d functions\n" NORMAL_PRINT, remove_dfuncs());
			}
			else if (command[0] == 'l') list_standrard(command[1]);
		}
		else if (line[0] == '/' || line[0] == 0) {}
		else handle_line(line);
	}
}
