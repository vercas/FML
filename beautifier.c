#include "beautifier.h"
#include <errno.h>
#include <string.h>
#include <assert.h>

#define MIN(a,b) \
	({ __typeof__ (a) _a = (a); \
		__typeof__ (b) _b = (b); \
		_a < _b ? _a : _b; })

#define MAX(a,b) \
	({ __typeof__ (a) _a = (a); \
		__typeof__ (b) _b = (b); \
		_a > _b ? _a : _b; })

typedef struct SinkContext_s
{
	FmlBeautifierSink Sink;
	void * Context;
	int IndentLevel;
	size_t LineWidth;
} SinkContext;

static int _SinkEx(SinkContext * sct, char const * str, size_t len)
{
	// printf("_SinkEx %zu %s\n", len, str);
	int const res = sct->Sink(str, len, sct->Context);

	if (res == 0)
		sct->LineWidth += len;

	return res;
}
#define SinkEx(a, b, c) do { int _res = _SinkEx(a, b, c); if (_res != 0) return _res; } while (false)

static int _Sink(SinkContext * sct, char const * str)
{
	// printf("_Sink %s\n", str);
	size_t const len = strlen(str);

	return _SinkEx(sct, str, len);
}
#define Sink(a, b) do { int _res = _Sink(a, b); if (_res != 0) return _res; } while (false)

static int _SinkIndent(SinkContext * sct)
{
	// printf("_SinkIndent %d\n", sct->IndentLevel);

	static char const * const tabs = "\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t"
									 "\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t";

	for (int i = 0; i < sct->IndentLevel; i += 40)
		SinkEx(sct, tabs, MIN(40, sct->IndentLevel - i));

	return 0;
}
#define SinkIndent(a) do { int _res = _SinkIndent(a); if (_res != 0) return _res; } while (false)

static int _SinkEquals(SinkContext * sct, int cnt)
{
	// printf("_SinkIndent %d\n", sct->IndentLevel);

	static char const * const tabs = "========================================";

	for (int i = 0; i < cnt; i += 40)
		SinkEx(sct, tabs, MIN(40, cnt - i));

	return 0;
}
#define SinkEquals(a, b) do { int _res = _SinkEquals(a, b); if (_res != 0) return _res; } while (false)

static int _SinkNewline(SinkContext * sct)
{
	static char const * const newline = "\n";

	return _SinkEx(sct, newline, 1);
}
#define SinkNewline(a) do { int _res = _SinkNewline(a); if (_res != 0) return _res; } while (false)

static int _SinkSpace(SinkContext * sct)
{
	static char const * const space = " ";

	return _SinkEx(sct, space, 1);
}
#define SinkSpace(a) do { int _res = _SinkSpace(a); if (_res != 0) return _res; } while (false)

static int _SinkLL(SinkContext * sct, long long int ll)
{
	char buf[30] = {0};
	size_t len = snprintf(buf, 30, "%lld", ll);

	return _SinkEx(sct, buf, len);
}
#define SinkLL(a, b) do { int _res = _SinkLL(a, b); if (_res != 0) return _res; } while (false)

static int _SinkFloat(SinkContext * sct, double f)
{
	char buf[100] = {0};
	size_t len = snprintf(buf, 100, "%f", f);

	return _SinkEx(sct, buf, len);
}
#define SinkFloat(a, b) do { int _res = _SinkFloat(a, b); if (_res != 0) return _res; } while (false)

//	Finally the real stuff.

static int SinkString(SinkContext * sct, char const * str, size_t len)
{
	SinkEx(sct, "\"", 1);

	for (/* nothing */; len > 0; --len, ++str)
		switch (*str)
		{
		case '\a': SinkEx(sct, "\\a", 2); break;
		case '\b': SinkEx(sct, "\\b", 2); break;
		case '\f': SinkEx(sct, "\\f", 2); break;
		case '\n': SinkEx(sct, "\\n", 2); break;
		case '\r': SinkEx(sct, "\\r", 2); break;
		case '\t': SinkEx(sct, "\\t", 2); break;
		case '\v': SinkEx(sct, "\\v", 2); break;
		case '\0': SinkEx(sct, "\\0", 2); break;

		case '\\': SinkEx(sct, "\\\\", 2); break;
		case '"':  SinkEx(sct, "\\\"", 2); break;

		default: SinkEx(sct, str, 1);
		}

	SinkEx(sct, "\"", 1);

	return 0;
}

static int SinkDocument(SinkContext * sct, char const * str, size_t len)
{
	int seqLen = -1, lineCount = 1;

	struct ClosingSequence
	{
		int Length;
		struct ClosingSequence * Next;
	} * ClosingSequences = NULL, * * NCS = &ClosingSequences;

	for (int i = 0; i < len; ++i)
		switch (str[i])
		{
		case ']':
			if (seqLen < 0)
				seqLen = 0;
			else
			{
				*NCS = calloc(1, sizeof(struct ClosingSequence));
				(*NCS)->Length = seqLen;
				NCS = &((*NCS)->Next);

				seqLen = -1;
			}

			break;

		case '=':
			if (seqLen >= 0)
				++seqLen;

			break;

		case '\n':
			++lineCount;
			break;
		}

	for (seqLen = 0; seqLen < len; ++seqLen)
	{
		struct ClosingSequence * cs;

		for (cs = ClosingSequences; cs != NULL; cs = cs->Next)
			if (cs->Length == seqLen)
				break;
		//	Stop at the first closing sequence found with the same length.

		if (cs == NULL)
			break;
		//	`cs` being NULL means no closing sequence was found of this length.
	}

	assert(seqLen < len);

	for (struct ClosingSequence * cs = ClosingSequences; cs != NULL; /* nothing */)
	{
		struct ClosingSequence * csNext = cs->Next;
		free((void *)cs);
		cs = csNext;
	}

	SinkEx(sct, "[", 1);
	SinkEquals(sct, seqLen);
	SinkEx(sct, "[", 1);

	if (seqLen < 5 && lineCount == 1 && len < 30)
	{
		SinkEx(sct, str, len);
	}
	else
	{
		SinkNewline(sct);
		SinkEx(sct, str, len);
		SinkNewline(sct);
	}

	SinkEx(sct, "]", 1);
	SinkEquals(sct, seqLen);
	SinkEx(sct, "]", 1);

	return 0;
}

static int SinkNode(SinkContext * sct, Node const * n)
{
	// printf("SinkNode 0x%p\n", n);
	int res;

	SinkIndent(sct);
	Sink(sct, n->Name);

	for (Class const * cl = n->Classes; cl != NULL; cl = cl->Next)
	{
		SinkEx(sct, ".", 1);
		Sink(sct, cl->Name);
	}

	if (n->Id)
	{
		SinkEx(sct, "#", 1);
		Sink(sct, n->Id);
	}

	for (Attribute * a = n->Attributes; a != NULL; a = a->Next)
	{
		SinkSpace(sct);
		Sink(sct, a->Key);

		if (a->ValueType == AVT_NONE)
			continue;

		SinkEx(sct, "=", 1);

		switch (a->ValueType)
		{
		case AVT_STRING:
			res = SinkString(sct, a->sValue, a->sLength);

			if (res != 0)
				return res;
			break;

		case AVT_REFERENCE:
			SinkEx(sct, "$", 1);
		case AVT_IDENTIFIER:
			SinkEx(sct, a->sValue, a->sLength);
			break;

		case AVT_INTEGER:
			SinkLL(sct, a->lValue);
			break;

		case AVT_FLOAT:
			SinkFloat(sct, a->dValue);
			break;

		default:
			return -10001;
		}
	}

	switch (n->BodyType)
	{
	case NBT_NONE:
		SinkEx(sct, ";", 1);
		break;

	case NBT_DOCUMENT:
		SinkSpace(sct);
		res = SinkDocument(sct, n->Document, n->DocumentLength);

		if (res != 0)
			return res;
		break;

	case NBT_CHILDREN:
		do{}while(false);	//	Screw you, C.

		Node * c = n->Children;

		if (c == NULL)
		{
			SinkEx(sct, " { }", 4);
			break;
		}

		SinkNewline(sct);
		SinkIndent(sct);
		SinkEx(sct, "{", 1);
		SinkNewline(sct);

		sct->IndentLevel++;

		do
		{
			res = SinkNode(sct, c);

			if (res != 0)
				return res;

			c = c->Next;
		} while (c != NULL && res == 0);

		sct->IndentLevel--;

		SinkIndent(sct);
		SinkEx(sct, "}", 1);
	}

	SinkNewline(sct);

	return 0;
}

int FmlBeautifyEx(Node const * n, FmlBeautifierSink sink, void * ctxt)
{
	if (n == NULL)
		return -10000;

	SinkContext sct = {sink, ctxt, 0, 0};
	int res;

	do
	{
		res = SinkNode(&sct, n);

		if (res != 0)
			return res;

		n = n->Next;
	} while (n != NULL && res == 0);

	return res;
}

static int SinkToFile(char const * str, size_t len, void * ctxt)
{
	// printf("Sinking string: %s\n", str);
	//fwrite(str, 1, len, stderr);
	errno = 0;
	size_t res = fwrite(str, 1, len, (FILE *)ctxt);

	assert(res == len || errno != 0);

	return errno;
}

int FmlBeautify(Node const * n, FILE * file)
{
	int res = FmlBeautifyEx(n, &SinkToFile, file);

	if (res == 0)
		res = fflush(file);

	return res;
}
