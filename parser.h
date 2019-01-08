#pragma once

#include "lexer.h"

enum EXPRESSION_TYPES
{
	ET_NODE, ET_CLASS, ET_ATTRIBUTE
};

#define EXPRESSION_BASE \
	enum EXPRESSION_TYPES Type; \
	size_t Start, End;

typedef struct Class_s
{
	EXPRESSION_BASE

	char const * Name;
	size_t NameLength;
	struct Class_s * Next;
} Class;

enum ATTRIBUTE_VALUE_TYPES
{
	AVT_NONE, AVT_STRING, AVT_IDENTIFIER, AVT_REFERENCE,
	AVT_INTEGER, AVT_FLOAT
};

typedef struct Attribute_s
{
	EXPRESSION_BASE

	char const * Key;
	size_t KeyLength;

	enum ATTRIBUTE_VALUE_TYPES ValueType;

	union
	{
		struct					//	AVT_STRING, AVT_IDENTIFIER, AVT_REFERENCE
		{
			char const * sValue;
			size_t sLength;
		};

		long long int lValue;	//	AVT_INTEGER
		double dValue;			//	AVT_FLOAT
	};

	struct Attribute_s * Next;
} Attribute;

enum NODE_BODY_TYPES
{
	NBT_NONE, NBT_CHILDREN, NBT_DOCUMENT
};

typedef struct Node_s
{
	EXPRESSION_BASE

	char const * Name;

	Class * Classes;
	char const * Id;

	Attribute * Attributes;

	enum NODE_BODY_TYPES BodyType;

	union
	{
		struct
		{
			struct Node_s * Children, * LastChild;
			size_t ChildrenCount;
		};

		struct
		{
			char const * Document;
			size_t DocumentLength;
		};
	};

	struct Node_s * Next;
} Node;

struct ParserState_s;
typedef struct ParserState_s ParserState;

typedef bool (*ParserErrorSink)(ParserState * p, size_t loc, size_t cnt, char const * err);

struct ParserState_s
{
	LexerState const * lexer;
	Token const * curToken;

	ParserErrorSink ErrorSink;

	Node * Nodes, * LastNode;
};

ParserState * Parse(LexerState const * l, ParserErrorSink ers);
void FreeParserState(ParserState * p);

bool ReportParserErrorDefault(ParserState * p, size_t loc, size_t cnt, char const * err);
