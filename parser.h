#pragma once

#include "lexer.h"

enum EXPRESSION_TYPES
{
	ET_NODE, ET_ATTRIBUTE
};

#define EXPRESSION_BASE \
	enum EXPRESSION_TYPES Type; \
	size_t Start, End;

typedef struct Class_s
{
	char const * Name;
	struct Class_s * Next;
} Class;

enum ATTRIBUTE_VALUE_TYPES
{
	AVT_NONE, AVT_STRING, AVT_IDENTIFIER, AVT_REFERENCE,
	AVT_INTEGER, AVT_FLOAT
};

typedef struct AttributeExpression_s
{
	EXPRESSION_BASE

	char const * Key;
	enum ATTRIBUTE_VALUE_TYPES ValueType;

	union
	{
		char const * sValue;	//	AVT_STRING
		char const * iValue;	//	AVT_IDENTIFIER
		char const * rValue;	//	AVT_REFERENCE
		long long int lValue;	//	AVT_INTEGER
		double dValue;			//	AVT_FLOAT
	};

	struct AttributeExpression_s * Next;
} AttributeExpression;

enum NODE_BODY_TYPES
{
	NBT_NONE, NBT_CHILDREN, NBT_DOCUMENT
};

typedef struct NodeExpression_s
{
	EXPRESSION_BASE

	char const * Name;

	Class * Classes;
	char const * Id;

	AttributeExpression * Attributes;

	enum NODE_BODY_TYPES BodyType;

	union
	{
		struct NodeExpression_s * Children;
		char const * Document;
	};

	struct NodeExpression_s * Next;
} NodeExpression;

struct ParserState_s;
typedef struct ParserState_s ParserState;

typedef bool (*ParserErrorSink)(ParserState * p, size_t loc, size_t cnt, char const * err);

struct ParserState_s
{
	LexerState const * lexer;
	Token const * curToken;

	ParserErrorSink ErrorSink;

	NodeExpression * Nodes;
};

ParserState * Parse(LexerState const * l, ParserErrorSink ers);
void FreeParserState(ParserState * l);

bool ReportParserErrorDefault(ParserState * p, size_t loc, size_t cnt, char const * err);
