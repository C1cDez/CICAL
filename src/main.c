#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "cical.h"


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
	case ERROR_PARSER_EXPECTED_DEFINITION: printf("Declaration expected a definition.\n"); break;
	case ERROR_COMPUTE_UNDEFINED_VARIABLE: printf("Undefined variable.\n"); break;
	case ERROR_COMPUTE_UNEXPECTED_NAN: printf("Unexpected NaN.\n"); break;
	case ERROR_COMPUTE_UNDEFINED_OPERATION: printf("Undefined operation.\n"); break;
	case ERROR_COMPUTE_UNDEFINED_FUNCTION: printf("Undefined function.\n"); break;
	case -1: printf("somthing went wrong.\n"); break;
	default: printf("[UNDEFINED]\n");
	}
	printf(NORMAL_PRINT);
}


#define DEFAULT_TOKENS_MAXCOUNT 1024
static token_t TOKENS[DEFAULT_TOKENS_MAXCOUNT] = { 0 };

static int PRECISION = 6;

static void handle_line(const char* line)
{
	int err = 0;

	int count = tokenize_line(line, TOKENS, DEFAULT_TOKENS_MAXCOUNT);
	if (count < 0) { err = count; goto end; }

	ast_node_t* node = calloc(1, sizeof(ast_node_t));
	if (!node) { err = -1; goto end; }
	err = parse_content(TOKENS, count, node);
	if (err < 0) goto end;

	compresult_t cr = { 0 };
	if (execute_sequence(node, &cr))
	{
		annihilate_tree(node);
		if (cr.error) { err = cr.error; goto end; }
		printf(YELLOW_PRINT "= %.*f\n" NORMAL_PRINT, PRECISION, cr.value);
	}

	err = 0;
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
		fgets(line, 128, stdin);
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
				while (num[0] == ' ') num++;
				PRECISION = atoi(num);
				printf(SILENT_PRINT "Set output precision to %d digits\n" NORMAL_PRINT, PRECISION);
			}
		}
		else if (line[0] == '/' || line[0] == 0) {}
		else handle_line(line);
	}
}
