#pragma once

// --------------------- ERRORS ---------------------

#define ERROR_LEXER_UNDEFINED_SYMBOL 0xc1ca1100

#define ERROR_PARSER_EXPECTED_NUMBER_AFTER_DOT 0xc1ca1200
#define ERROR_PARSER_UNCLOSED_BRACKET 0xc1ca1201
#define ERROR_PARSER_EXPECTED_ARGUMENT 0xc1ca1202

#define ERROR_COMPUTE_UNDEFINED_VARIABLE 0xc1ca1300
#define ERROR_COMPUTE_DIVISION_BY_ZERO 0xc1ca1301
#define ERROR_COMPUTE_UNDEFINED_OPERATION 0xc1ca1302
#define ERROR_COMPUTE_ILLEGAL_DOMAIN 0xc1ca1303


// --------------------- CALC ---------------------

typedef struct
{
	const char* name;
	enum
	{
		LITERAL_FUNCTION,
		LITERAL_CONST
	} type;
	int two_arguments;
} literal_t;

literal_t get_literal(int idx);
int find_literal(const char* str);

typedef struct
{
	char sym;
	int ident;
} variable_t;


// --------------------- LEXER ---------------------

#define TOKEN_DATA_SIZE 20

enum
{
	TOKEN_NUMBER,
	TOKEN_ALPHA,
	TOKEN_LITERAL,
	TOKEN_OPERATION,
	TOKEN_BRACKET,
	TOKEN_UNDERSCORE,
	TOKEN_COMMA,
	TOKEN_DOT,
	TOKEN_EQUAL
};

typedef struct token
{
	int type;
	char data[TOKEN_DATA_SIZE];
} token_t;

int tokenize_line(const char* line, token_t* tokens, int maxsize);


// --------------------- PARSER ---------------------

enum
{
	NODE_NUMBER,
	NODE_VARIABLE,
	NODE_UNARY_MINUS,
	NODE_POW,
	NODE_MULTIPLY,
	NODE_DIVIDE,
	NODE_ADD,
	NODE_SUBTRACT,
	NODE_FUNCCALL,
	NODE_CONST,
	NODE_ASSIGNMENT
};
typedef struct ast_node
{
	int type;
	union
	{
		double number;
		variable_t variable;
		int funcidx;
	};
	struct ast_node* left, * right;
} ast_node_t;

void annihilate_tree(ast_node_t* node);

int parse_content(const token_t* tokens, int count, ast_node_t* root);


// --------------------- COMPUTE ---------------------

typedef struct
{
	double value;
	int error;
} compresult_t;

int execute_sequence(const ast_node_t* node, compresult_t* cr);
