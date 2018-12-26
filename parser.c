#include "parser.h"
#include "stdio.h"

static Token const * ConsumeToken(ParserState * const p)
{
	if (p->curToken == NULL)
		return p->curToken = p->lexer->Tokens;

	Token const * const next = p->curToken->Next;
	return p->curToken = next;
}

static Token const * PeekToken(ParserState * const p)
{
	if (p->curToken == NULL)
		return p->lexer->Tokens;
	else
		return p->curToken->Next;
}

static bool ReportTkError(ParserState * p, Token const * tk, char const * err)
{
	return p->ErrorSink(p, tk->Start, tk->End - tk->Start, err);
}

static NodeExpression * ParseNode(ParserState * p)
{
	Token const * tk = ConsumeToken(p);
	//	This one is guaranteed to be an identifier.

	NodeExpression * ne = calloc(1, sizeof(NodeExpression));
	ne->Type = ET_NODE;
	ne->Start = tk->Start;
	ne->Name = tk->sValue;

	printf("Node named %s.\n", ne->Name);

	Class * * cl = &(ne->Classes);

	//	If the token consumed here isn't a dot, it's used by the code after.
	while ((tk = ConsumeToken(p))->Type == TT_DOT)
	{
		//	Dot requires an identifier (class name) after it.
		tk = ConsumeToken(p);

		if (tk->Type != TT_IDENTIFIER)
		{
			ne->End = tk->End;

			if (ReportTkError(p, tk, "Expected identifier after dot."))
				return ne;
			else
				continue;
		}

		*cl = calloc(1, sizeof(Class));
		(*cl)->Name = tk->sValue;
		cl = &((*cl)->Next);

		printf("\tClass named %s.\n", tk->sValue);
	}

	if (tk->Type == TT_HASH)
	{
		//	Hash requires an identifier (ID) after it.
		tk = ConsumeToken(p);

		if (tk->Type != TT_IDENTIFIER)
		{
			ne->End = tk->End;

			if (ReportTkError(p, tk, "Expected identifier after hash."))
				return ne;
		}
		else
			ne->Id = tk->sValue;

		printf("\tID named %s.\n", tk->sValue);
	}

	AttributeExpression * * at = &(ne->Attributes);

	for (/* nothing */; tk->Type == TT_IDENTIFIER; tk = ConsumeToken(p))
	{
		AttributeExpression * ae = *at = calloc(1, sizeof(AttributeExpression));
		ae->Type = ET_ATTRIBUTE;
		ae->Start = tk->Start;
		ae->Key = tk->sValue;
		at = &(ae->Next);

		printf("\tAttribute named %s.\n", tk->sValue);

		tk = ConsumeToken(p);

		switch (tk->Type)
		{
			//	All these mean that this attribute has a null value.
		case TT_IDENTIFIER:		//	Next is another attribute.
		case TT_SEMICOLON:		//	Node ends.
		case TT_BRACKET_OPEN:	//	Node children begin.
		case TT_DOCUMENT:		//	Node document begins.
			ae->ValueType = AVT_NONE;
			printf("\t\tNo value.\n");
			break;

			//	This means the attribute has an explicit value.
		case TT_EQUAL:
			tk = ConsumeToken(p);

			switch (tk->Type)
			{
			case TT_INTEGER:
				ae->ValueType = AVT_INTEGER;
				ae->lValue = tk->lValue;
				printf("\t\tInteger value %llu.\n", tk->lValue);
				break;

			case TT_FLOAT:
				ae->ValueType = AVT_FLOAT;
				ae->dValue = tk->dValue;
				printf("\t\tFloat value %d.\n", tk->dValue);
				break;

			case TT_STRING:
				ae->ValueType = AVT_STRING;
				ae->sValue = tk->sValue;
				printf("\t\tString value %s.\n", tk->sValue);
				break;

			case TT_IDENTIFIER:
				ae->ValueType = AVT_IDENTIFIER;
				ae->iValue = tk->sValue;
				printf("\t\tIdentifier value %s.\n", tk->sValue);
				break;

			case TT_DOLLAR:
				ae->ValueType = AVT_REFERENCE;

				tk = ConsumeToken(p);

				if (tk->Type != TT_IDENTIFIER)
				{
					ne->End = tk->End;

					if (ReportTkError(p, tk, "Expected identifier after dollar sign."))
						return ne;
					else
						continue;
				}

				ae->rValue = tk->sValue;
				printf("\t\tReference value %s.\n", tk->sValue);
				break;

			default:
				ne->End = tk->End;

				if (ReportTkError(p, tk, "Unexpected token after equal sign."))
					return ne;
				else
					continue;
			}

			break;

		default:
			ne->End = tk->End;

			if (ReportTkError(p, tk, "Expected token after attribute key."))
				return ne;
			else
				continue;
		}
	}

	if (tk->Type == TT_DOCUMENT)
	{
		ne->BodyType = NBT_DOCUMENT;
		ne->End = tk->End;
		ne->Document = tk->sValue;
		printf("\tDocument body.\n");
	}
	else if (tk->Type == TT_SEMICOLON)
	{
		ne->BodyType = NBT_NONE;
		ne->End = tk->End;
		printf("\tNo body.\n");
	}
	else if (tk->Type == TT_BRACKET_OPEN)
	{
		ne->BodyType = NBT_CHILDREN;
		NodeExpression * * nextNode = &(ne->Children);

		while ((tk = PeekToken(p))->Type != TT_BRACKET_CLOSE)
		{
			if (tk->Type != TT_IDENTIFIER)
			{
				if (ReportTkError(p, tk, "Expected identifier to start child node."))
					return ne;
				else
				{
					ConsumeToken(p);
					continue;
				}
			}

			printf("\tChild:\n");

			*nextNode = ParseNode(p);
			nextNode = &((*nextNode)->Next);
		}

		ne->End = tk->End;
		(void)ConsumeToken(p);	//	Consumes the closing bracket.
	}
	else
	{
		ReportTkError(p, tk, "Unexpected token in node.");
	}

	return ne;
}

ParserState * Parse(LexerState const * l, ParserErrorSink ers)
{
	ParserState * p = calloc(1, sizeof(ParserState));
	p->lexer = l;
	p->ErrorSink = ers;
	p->curToken = NULL;

	NodeExpression * * nextNode = &(p->Nodes);
	Token const * tk;

	while ((tk = PeekToken(p))->Type != TT_EOF)
	{
		if (tk->Type != TT_IDENTIFIER)
		{
			if (ReportTkError(p, tk, "Expected identifier to start top-level node."))
				return p;
			else
			{
				ConsumeToken(p);
				continue;
			}
		}

		*nextNode = ParseNode(p);
		nextNode = &((*nextNode)->Next);
	}

	return p;
}

void FreeParserState(ParserState * l)
{

}

bool ReportParserErrorDefault(ParserState * p, size_t loc, size_t cnt, char const * err)
{
	long line = 1, i
		, nlBeforeStart = -1, nlBeforeEnd = -1
		, wsBeforeStart = -1, wsBeforeEnd = -1
		, nlAfterStart = -1, nlAfterEnd = -1;

	//	Find the start of the line where the error starts, as well as
	//	the line number.
	for (i = 0; i < (long)loc; ++i)
		if (p->lexer->Input[i] == '\n')
		{
			nlBeforeStart = wsBeforeStart = i;
			++line;
		}
		else if (wsBeforeStart == i - 1)
			switch (p->lexer->Input[i])
			{
			case ' ': case '\t':
				wsBeforeStart = i;
			default:
				break;
			}

	//	Find the end of the line where the error starts.
	//	Note that this uses a different indexer, as to not affect the next loop.
	for (long j = i; j <= (long)p->lexer->InputSize; ++j)
		if (p->lexer->Input[j] == '\n' || p->lexer->Input[j] == '\0')
		{
			nlAfterStart = j;
			break;
		}

	//	Find the start of the line where the error ends, as well as
	//	the line number.
	for (/* nothing */; i < (long)loc + (long)cnt; ++i)
		if (p->lexer->Input[i] == '\n')
		{
			nlBeforeEnd = wsBeforeEnd = i;
			++line;
		}
		else if (wsBeforeEnd == i - 1)
			switch (p->lexer->Input[i])
			{
			case ' ': case '\t':
				wsBeforeEnd = i;
			default:
				break;
			}

	//	Find the end of the line where the error ends.
	for (/* nothing */; i <= (long)p->lexer->InputSize; ++i)
		if (p->lexer->Input[i] == '\n' || p->lexer->Input[i] == '\0')
		{
			nlAfterEnd = i;
			break;
		}

	fprintf(stderr, "%zd:%ld: (%2X) %s\n", line, (long)loc - nlBeforeStart, p->lexer->Input[loc], err);

	if (nlAfterStart > nlBeforeStart)
	{
		if (nlBeforeStart < nlBeforeEnd)
		{
			//	Error starts and ends on different lines.

			fprintf(stderr, "TODO!\n");
		}
		else
		{
			//	Error starts and ends on the same line.

			fwrite(p->lexer->Input + nlBeforeStart + 1, nlAfterStart - nlBeforeStart - 1, 1, stderr);
			putc('\n', stderr);

			if (wsBeforeStart > nlBeforeStart)
				fwrite(p->lexer->Input + nlBeforeStart + 1, wsBeforeStart - nlBeforeStart, 1, stderr);

			if ((long)loc > wsBeforeStart)
				fprintf(stderr, "%*.s^", (int)(loc - wsBeforeStart - 1), "");

			if (cnt > 1)
			{
				for (/* nothing */; cnt > 1; --cnt)
					putc('~', stderr);

				putc('^', stderr);
			}

			putc('\n', stderr);
		}
	}

	return false;
}
