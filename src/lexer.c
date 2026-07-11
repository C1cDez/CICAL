#include "lexer.h"

#include <ctype.h>


int tokenize_line(const char* line, token_t* tokens, int maxsize)
{
	int j = 0;
	int len = (int)strlen(line);
	for (int i = 0; j < maxsize && i < len; i++)
	{
		char c = line[i];
		token_t* tok = tokens + j;
		if (isspace(c)) continue;
		tok->data = NULL;

		if (c == ',') tok->type = TOKEN_COMMA;
		else if (c == '.') tok->type = TOKEN_DOT;
		else if (c == '|') tok->type = TOKEN_BAR;
		else if (c == '@') tok->type = TOKEN_AT;
		else if (c == '_') tok->type = TOKEN_UNDERSCORE;
		else if (c == '=') tok->type = TOKEN_EQUAL;
		else if (c == ';') tok->type = TOKEN_SEMICOLON;
		else if (c == '(' || c == ')')
		{
			tok->type = TOKEN_BRACKET;
			tok->data = (void*)c;
		}
		else if (c == '+' || c == '-' || c == '*' || c == '/' || c == '^')
		{
			tok->type = TOKEN_OPERATION;
			tok->data = (void*)c;
		}
		else if (isdigit(c))
		{
			tok->type = TOKEN_DIGITS;
			
			int dlen = (int)strspn(line + i, "0123456789");
			tok->data = calloc(1, dlen + 1);
			if (!tok->data) PANIC("Unexpected unallocation");
			memcpy(tok->data, line + i, dlen);
			i += dlen - 1;
		}
		else if (isalpha(c))
		{
			const sfunc_t* sfunc = get_sfunc(line + i);
			const lfunc_t* lfunc = get_lfunc(line + i);
			const alpha_name_t* an = get_alpha_name(line + i);

			if (sfunc)
			{
				tok->type = TOKEN_SFUNC;
				tok->data = (void*)sfunc;
				i += (int)strlen(sfunc->name) - 1;
			}
			else if (lfunc)
			{
				tok->type = TOKEN_LFUNC;
				tok->data = (void*)lfunc;
				i += (int)strlen(lfunc->name) - 1;
			}
			else if (an)
			{
				tok->type = TOKEN_ALPHA;
				tok->data = (void*)an;
				i += (int)strlen(an->str) - 1;
			}
			else
			{
				tok->type = TOKEN_SYMBOL;
				tok->data = (void*)c;
			}
		}
		else return ERROR_LEXER_UNDEFINED_SYMBOL;

		j++;
	}

	if (j == maxsize) PANIC("Unexpected end of buffer of lexer");
	tokens[j++].type = TOKEN_EOL;
	return j;
}
void cleanup_tokens(token_t* tokens, int count)
{
	for (int i = 0; i < count; i++)
	{
		if (tokens[i].type == TOKEN_DIGITS && tokens[i].data)
			free(tokens[i].data);
	}
}
