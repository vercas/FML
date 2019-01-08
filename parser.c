#include "parser.h"
#include <stdio.h>

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

static Node * ParseNode(ParserState * p)
{
	Token const * tk = ConsumeToken(p);
	//	This one is guaranteed to be an identifier.

	Node * ne = calloc(1, sizeof(Node));
	ne->Type = ET_NODE;
	ne->Start = tk->Start;
	ne->Name = tk->sValue;

	// printf("Node named %s.\n", ne->Name);

	Class * * cl = &(ne->Classes);

	//	If the token consumed here isn't a dot, it's used by the code after.
	while ((tk = ConsumeToken(p))->Type == TT_DOT)
	{
		size_t start = tk->Start;

		//	Dot requires an identifier (class name) after it.
		tk = ConsumeToken(p);

		if (tk->Type != TT_IDENTIFIER)
		{
			ne->End = tk->End;

			if (ReportTkError(p, tk, "Expected identifier after dot.")
				|| tk->Type == TT_EOF)
				return ne;
			else
				continue;
		}

		*cl = calloc(1, sizeof(Class));
		(*cl)->Type = ET_CLASS;
		(*cl)->Start = start;
		(*cl)->End = tk->End;
		(*cl)->Name = tk->sValue;

		cl = &((*cl)->Next);

		// printf("\tClass named %s.\n", tk->sValue);
	}

	if (tk->Type == TT_HASH)
	{
		//	Hash requires an identifier (ID) after it.
		tk = ConsumeToken(p);

		if (tk->Type != TT_IDENTIFIER)
		{
			ne->End = tk->End;

			if (ReportTkError(p, tk, "Expected identifier after hash.")
				|| tk->Type == TT_EOF)
				return ne;
		}
		else
			ne->Id = tk->sValue;

		// printf("\tID named %s.\n", tk->sValue);

		tk = ConsumeToken(p);	//	Moves on to the next token.
	}

	Attribute * * at = &(ne->Attributes);

	for (/* nothing */; tk->Type == TT_IDENTIFIER; tk = ConsumeToken(p))
	{
		Attribute * ae = *at = calloc(1, sizeof(Attribute));
		ae->Type = ET_ATTRIBUTE;
		ae->Start = tk->Start;
		ae->End = tk->End;
		ae->Key = tk->sValue;
		at = &(ae->Next);

		// printf("\tAttribute named %s.\n", tk->sValue);

		tk = ConsumeToken(p);

		switch (tk->Type)
		{
			//	All these mean that this attribute has a null value.
		case TT_IDENTIFIER:		//	Next is another attribute.
		case TT_SEMICOLON:		//	Node ends.
		case TT_BRACKET_OPEN:	//	Node children begin.
		case TT_DOCUMENT:		//	Node document begins.
			ae->ValueType = AVT_NONE;
			// printf("\t\tNo value.\n");
			break;

			//	This means the attribute has an explicit value.
		case TT_EQUAL:
			tk = ConsumeToken(p);

			switch (tk->Type)
			{
			case TT_INTEGER:
				ae->ValueType = AVT_INTEGER;
				ae->lValue = tk->lValue;
				// printf("\t\tInteger value %llu.\n", tk->lValue);
				break;

			case TT_FLOAT:
				ae->ValueType = AVT_FLOAT;
				ae->dValue = tk->dValue;
				// printf("\t\tFloat value %f.\n", tk->dValue);
				break;

			case TT_STRING:
				ae->ValueType = AVT_STRING;
				ae->sValue = tk->sValue;
				ae->sLength = tk->sLength;
				// printf("\t\tString value %s.\n", tk->sValue);
				break;

			case TT_IDENTIFIER:
				ae->ValueType = AVT_IDENTIFIER;
				ae->sValue = tk->sValue;
				ae->sLength = tk->sLength;
				// printf("\t\tIdentifier value %s.\n", tk->sValue);
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

				ae->sValue = tk->sValue;
				ae->sLength = tk->sLength;
				// printf("\t\tReference value %s.\n", tk->sValue);
				break;

			case TT_EOF:
				ne->End = tk->End;

				ReportTkError(p, tk, "Unfinished attribute.");
				return ne;

			default:
				ne->End = tk->End;

				if (ReportTkError(p, tk, "Unexpected token after equal sign."))
					return ne;
				else
					continue;
			}

			ae->End = tk->End;

			break;

		case TT_EOF:
			ne->End = tk->End;

			ReportTkError(p, tk, "Unclosed node.");
			return ne;

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
		ne->DocumentLength = tk->sLength;
		// printf("\tDocument body.\n");
	}
	else if (tk->Type == TT_SEMICOLON)
	{
		ne->BodyType = NBT_NONE;
		ne->End = tk->End;
		// printf("\tNo body.\n");
	}
	else if (tk->Type == TT_BRACKET_OPEN)
	{
		ne->BodyType = NBT_CHILDREN;
		Node * * nextNode = &(ne->Children);

		while ((tk = PeekToken(p))->Type != TT_BRACKET_CLOSE)
		{
			if (tk->Type != TT_IDENTIFIER)
			{
				if (ReportTkError(p, tk, "Expected identifier to start child node.")
					|| tk->Type == TT_EOF)
					return ne;
				else
				{
					ConsumeToken(p);
					continue;
				}
			}

			// printf("\tChild:\n");

			ne->LastChild = *nextNode = ParseNode(p);
			nextNode = &((*nextNode)->Next);
			ne->ChildrenCount++;
		}

		ne->End = tk->End;
		(void)ConsumeToken(p);	//	Consumes the closing bracket.
	}
	else if (tk->Type == TT_EOF)
	{
		ne->End = tk->End;

		ReportTkError(p, tk, "Unclosed node.");
	}
	else
	{
		ne->End = tk->End;

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

	Node * * nextNode = &(p->Nodes);
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

		p->LastNode = *nextNode = ParseNode(p);
		nextNode = &((*nextNode)->Next);
	}

	return p;
}

void FreeParserState(ParserState * p)
{
	//	Freeing the node tree is done iteratively - it's actually flattened.

	for (Node * n = p->Nodes; n != NULL; /* nothing */)
	{
		for (Attribute const * at = n->Attributes; at != NULL; /* nothing */)
		{
			Attribute const * atNext = at->Next;
			free((void *)at);
			at = atNext;
		}

		for (Class const * cl = n->Classes; cl != NULL; /* nothing */)
		{
			Class const * clNext = cl->Next;
			free((void *)cl);
			cl = clNext;
		}

		if (n->BodyType == NBT_CHILDREN && n->LastChild != NULL)
		{
			n->LastChild->Next = n->Next;
			n->Next = n->Children;
			//	This turns this node's children into its siblings! :DDD
		}

		Node * nNext = n->Next;
		free(n);
		n = nNext;
	}

	free(p);
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

	fprintf(stderr, "%zd:%ld: %s\n", line, (long)loc - nlBeforeStart, err);

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
