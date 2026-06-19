#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

/* --------------------- ERRORS --------------------- */

#define PANIC(s) { fprintf(stderr, s); exit(__COUNTER__); }

#define ERROR_LEXER_UNDEFINED_SYMBOL 0xc1ca1100

#define ERROR_PARSER_EXPECTED_NUMBER_AFTER_DOT 0xc1ca1200
#define ERROR_PARSER_UNCLOSED_BRACKET 0xc1ca1201
#define ERROR_PARSER_EXPECTED_ARGUMENT 0xc1ca1202
#define ERROR_PARSER_EXPECTED_DEFINITION 0xc1ca1203
#define ERROR_PARSER_NOT_FINISHED_STATEMENT 0xc1ca1204

#define ERROR_COMPUTE_UNDEFINED_VARIABLE 0xc1ca1300
#define ERROR_COMPUTE_UNEXPECTED_NAN 0xc1ca1301
#define ERROR_COMPUTE_UNDEFINED_OPERATION 0xc1ca1302
#define ERROR_COMPUTE_UNDEFINED_FUNCTION 0xc1ca1303
#define ERROR_COMPUTE_TOO_FEW_ARGUMENTS 0xc1ca1304
#define ERROR_COMPUTE_EXPECTED_REGULAR_DECLARATION 0xc1ca1305
#define ERROR_COMPUTE_EXPECTED_ONLY_VARIABLES 0xc1ca1306

/* --------------------- CORE --------------------- */

/* long names */
typedef struct
{
	const char* str;
} alpha_name_t;
const alpha_name_t* get_alpha_name(const char* str);

/* identifiers */
typedef struct
{
	union
	{
		char symbol;
		alpha_name_t* alpha;
	};
	enum
	{
		IDENTIFIER_SYMBOL = 1,
		IDENTIFIER_ALPHA
	} type;
	int subscript;
} identifier_t;
int ident_eq(identifier_t a, identifier_t b);

/* variables */
const struct ast_node* get_variable(identifier_t ident);
int remove_variables();

/* functions */
struct ast_node;
struct compresult;
struct varenv;

typedef struct
{
	const char* name;
	double (*logic)(double);
} sfunc_t;
const sfunc_t* get_sfunc(const char* str);

typedef struct
{
	const char* name;
	struct compresult (*logic)(const struct ast_node* node, struct varenv* env);
} lfunc_t;
const lfunc_t* get_lfunc(const char* str);

typedef struct
{
	identifier_t ident;
	const struct ast_node* args;
	const struct ast_node* impl;
} dfunc_t;
const dfunc_t* get_dfunc(identifier_t ident);
int remove_dfuncs();

/* main */
int execute(const struct ast_node* root, struct compresult* cr);
void preload_defaults();
void annihilate_tree(const struct ast_node* node);
