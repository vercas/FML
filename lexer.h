#pragma once

#include "stdlib.h"
#include "stdint.h"
#include "stdbool.h"

enum TOKEN_TYPES
{
	TT_IDENTIFIER, TT_INTEGER, TT_FLOAT, TT_STRING,
	TT_EQUAL, TT_BRACKET_OPEN, TT_BRACKET_CLOSE,
	TT_DOCUMENT, TT_SEMICOLON, TT_DOT, TT_HASH, TT_DOLLAR,
	TT_EOF
};

typedef struct Token_s
{
	enum TOKEN_TYPES Type;
	size_t Start, End;

	union
	{
		char const * sValue;	//	TT_IDENTIFIER, TT_STRING, TT_DOCUMENT
		long long int lValue;	//	TT_INTEGER
		double dValue;			//	TT_FLOAT
	};

	struct Token_s * Next;
} Token;

struct LexerState_s;
typedef struct LexerState_s LexerState;

typedef bool (*LexerErrorSink)(LexerState * l, size_t loc, char const * err);

struct LexerState_s
{
	Token const * Tokens;
	Token * lastToken, * workingToken;
	char const * Input;
	size_t InputSize;
	char const * Buffer;
	LexerErrorSink ErrorSink;
};

LexerState * Lex(char * str, size_t const len, LexerErrorSink ers);
void FreeLexerState(LexerState * l);

bool ReportLexerErrorDefault(LexerState * l, size_t loc, char const * err);
