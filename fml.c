#include "stdio.h"
#include "errno.h"
#include "string.h"
#include "lexer.h"
#include "parser.h"

struct PrintContext
{
	NodeExpression const * Node;
	int Spaces;
	bool IsLast;
	FILE * Output;

	struct PrintContext * Prev;
};

static void PrintIndent(struct PrintContext * pc, bool beginning)
{
	if (pc->Prev)
		PrintIndent(pc->Prev, false);

	if (pc->Spaces)
		fprintf(pc->Output, "%*.s", pc->Spaces, "");

	if (pc->IsLast)
	{
		if (beginning)
			fputs("└───", pc->Output);
		else
			fputs("    ", pc->Output);
	}
	else
	{
		if (beginning)
			fputs("├───", pc->Output);
		else
			fputs("│   ", pc->Output);
	}
}

static void PrintNode(struct PrintContext * pc)
{
	PrintIndent(pc, true);
	fprintf(pc->Output, " Node \"%s\"\n", pc->Node->Name);

	if (pc->Node->Classes)
	{
		PrintIndent(pc, false);
		fputs("  Classes: ", pc->Output);

		Class const * cl = pc->Node->Classes;
		fputs(cl->Name, pc->Output);

		while ((cl = cl->Next) != NULL)
			fprintf(pc->Output, ", %s", cl->Name);

		putc('\n', pc->Output);
	}

	if (pc->Node->Id)
	{
		PrintIndent(pc, false);
		fprintf(pc->Output, "  Id: %s\n", pc->Node->Id);
	}

	if (pc->Node->Attributes)
	{
		PrintIndent(pc, false);
		fputs("  Attributes ┐\n", pc->Output);

		struct PrintContext spc = {NULL, 13, false, pc->Output, pc};

		for (AttributeExpression const * at = pc->Node->Attributes; at != NULL; at = at->Next)
		{
			spc.IsLast = at->Next == NULL;
			PrintIndent(&spc, true);

			putc(' ', pc->Output);
			fputs(at->Key, pc->Output);

			switch (at->ValueType)
			{
			case AVT_NONE:
				fputs(" (no value)\n", pc->Output);
				break;
			case AVT_STRING:
				fprintf(pc->Output, " = \"%s\" (string)\n", at->sValue);
				break;
			case AVT_IDENTIFIER:
				fprintf(pc->Output, " = %s (identifier)\n", at->iValue);
				break;
			case AVT_REFERENCE:
				fprintf(pc->Output, " = $%s (reference)\n", at->rValue);
				break;
			case AVT_INTEGER:
				fprintf(pc->Output, " = %llu (integer)\n", at->lValue);
				break;
			case AVT_FLOAT:
				fprintf(pc->Output, " = %f (float)\n", at->dValue);
				break;
			}
		}
	}

	if (pc->Node->BodyType == NBT_DOCUMENT)
	{
		PrintIndent(pc, false);
		fprintf(pc->Output, "  Document:\n"
			"--------------------------------------------------------------------------------\n"
			"%s\n"
			"--------------------------------------------------------------------------------\n"
			, pc->Node->Document);
	}
	else if (pc->Node->BodyType == NBT_CHILDREN)
	{
		PrintIndent(pc, false);

		if (pc->Node->Children)
		{
			fputs("  Children ┐\n", pc->Output);

			struct PrintContext spc = {NULL, 11, false, pc->Output, pc};

			for (NodeExpression const * nd = pc->Node->Children; nd != NULL; nd = nd->Next)
			{
				spc.Node = nd;
				spc.IsLast = nd->Next == NULL;
				PrintNode(&spc);
			}
		}
		else
		{
			fputs("  Children (empty list)\n", pc->Output);
		}
	}
}

static void PrintParserState(ParserState const * p, FILE * output)
{
	struct PrintContext pc = {NULL, 5, false, output, NULL};

	fputs("Root ┐\n", output);

	for (NodeExpression const * nd = p->Nodes; nd != NULL; nd = nd->Next)
	{
		pc.Node = nd;
		pc.IsLast = nd->Next == NULL;
		PrintNode(&pc);
	}
}

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

	ParserState * p = Parse(l, ReportParserErrorDefault);

	PrintParserState(p, stdout);

	FreeParserState(p);
	FreeLexerState(l);

#undef PHAIL

    return 0;
}
