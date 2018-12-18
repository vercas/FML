#include "parser.h"

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

static bool ReportTkError(ParserState * p, Token * tk, char const * err)
{
	return p->ErrorSink(p, tk->Start, tk->End - tk->Start, err);
}

static NodeExpression * ParseNode(ParserState * p)
{
	Token * tk = ConsumeToken(p);
}

ParserState * Parse(LexerState const * l, ParserErrorSink ers)
{
	ParserState * p = calloc(1, sizeof(ParserState));
	p->lexer = l;
	p->ErrorSink = ers;
	p->curToken = NULL;

	NodeExpression * * nextNode = &(p->Nodes);
	Token * nextToken;

	while ((nextToken = PeekToken(p))->Type != TT_EOF)
	{
		if (nextToken->Type != TT_IDENTIFIER)
		{
			if (ReportTkError(p, nextToken, "Expected identifier."))
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

}
