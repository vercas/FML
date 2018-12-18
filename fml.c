#include "stdlib.h"
#include "stdio.h"
#include "errno.h"
#include "string.h"
#include "lexer.h"

static void phail(void)
{
	int er = errno;

	fprintf(stderr, "Aborted; errno = %d (%s)\n", er, strerror(er));
	exit(1);
}

int main(int argc, char * * argv)
{
	(void)argc;
	(void)argv;

	FILE * fIn = fopen("test.fml", "r");
	if (!fIn) phail();

#define PHAIL if (res != 0) phail();

	int res = fseek(fIn, 0, SEEK_END);
	PHAIL

	size_t const cnt = ftell(fIn);
	char * const str = malloc(cnt + 1);

	res = fseek(fIn, 0, SEEK_SET);
	PHAIL
	fread(str, 1, cnt, fIn);
	PHAIL
	str[cnt] = '\0';

	printf("%zd characters\n", cnt);
	fputs(str, stdout);

	LexerState * l = Lex(str, cnt, &ReportLexerErrorDefault);

	puts("Finished lexing.");

	for (Token const * tk = l->Tokens; tk != NULL; tk = tk->Next)
	{
		putchar('[');

		switch (tk->Type)
		{
		case TT_IDENTIFIER:		fputs("IDENTIFIER", stdout); break;
		case TT_INTEGER:		fputs("INTEGER", stdout); break;
		case TT_FLOAT:			fputs("FLOAT\t", stdout); break;
		case TT_STRING:			fputs("STRING\t", stdout); break;
		case TT_EQUAL:			fputs("EQUAL\t", stdout); break;
		case TT_BRACKET_OPEN:	fputs("BRK_OPEN", stdout); break;
		case TT_BRACKET_CLOSE:	fputs("BRK_CLOSE", stdout); break;
		case TT_DOCUMENT:		fputs("DOCUMENT", stdout); break;
		case TT_SEMICOLON:		fputs("SEMICOLON", stdout); break;
		case TT_DOLLAR:			fputs("DOLLAR\t", stdout); break;
		case TT_DOT:			fputs("DOT\t", stdout); break;
		case TT_HASH:			fputs("HASH\t", stdout); break;
		case TT_EOF:			fputs("EOF\t", stdout); break;

		default:
			printf("UNKNOWN TOKEN TYPE %d", tk->Type);
			break;
		}

		printf("\t; %4zd-%4zd", tk->Start, tk->End);

		switch (tk->Type)
		{
		case TT_IDENTIFIER: case TT_STRING:
			printf("; %s]\n", tk->sValue);
			break;

		case TT_INTEGER:
			printf("; %lld]\n", tk->lValue);
			break;

		case TT_FLOAT:
			printf("; %f]\n", tk->dValue);
			break;

		case TT_DOCUMENT:
			printf("; %s]\n", tk->sValue);
			break;

		default:
			puts("]");	//	Should automagically add newline.
			break;
		}
	}

	FreeLexerState(l);

#undef PHAIL

    return 0;
}
