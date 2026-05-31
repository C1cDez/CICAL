#include "cical.h"

#include <string.h>
#include <ctype.h>


int tokenize_line(const char* line, token_t* tokens, int maxsize)
{
	char temp[sizeof(tokens[0].data)] = { 0 };

	int j = 0;
	int len = (int)strlen(line);
	for (int i = 0; j < maxsize && i < len; i++)
	{
		char c = line[i];
		token_t* tok = tokens + j;
		if (isspace(c)) continue;

		if (c == ',') tok->type = TOKEN_COMMA;
		else if (c == '.') tok->type = TOKEN_DOT;
		else if (c == '|') tok->type = TOKEN_BAR;
		else if (c == '(' || c == ')')
		{
			tok->type = TOKEN_BRACKET;
			tok->data[0] = c;
		}
		else if (c == '_') tok->type = TOKEN_UNDERSCORE;
		else if (c == '+' || c == '-' || c == '*' || c == '/' || c == '^')
		{
			tok->type = TOKEN_OPERATION;
			tok->data[0] = c;
		}
		else if (c == '=') tok->type = TOKEN_EQUAL;
		else if (isdigit(c))
		{
			int k = 0;
			while (isdigit(line[i]) && i < len && k < TOKEN_DATA_SIZE)
			{
				tok->data[k] = line[i];
				i++; k++;
			}
			i--;
			tok->type = TOKEN_NUMBER;
		}
		else if (isalpha(c))
		{
			int k = 0;
			while (isalpha(line[i]) && k < TOKEN_DATA_SIZE && i < len)
			{
				temp[k] = line[i];
				k++; i++;
			}
			i--;

			for (int m = 0; m < k && j < maxsize; j++)
			{
				token_t* t = tokens + j;
				int f = find_literal(temp + m);
				if (f != -1)
				{
					t->type = TOKEN_LITERAL;
					t->data[0] = f;
					m += (int)strlen(get_literal(f).name);
				}
				else
				{
					t->type = TOKEN_ALPHA;
					t->data[0] = temp[m];
					m++;
				}
			}
			j--;

			memset(temp, 0, sizeof(temp));
		}
		else return ERROR_LEXER_UNDEFINED_SYMBOL;

		j++;
	}
	return j;
}
