#include "stdlib.h"
#include "stdio.h"
#include "errno.h"
#include "string.h"
#include "stdbool.h"

void phail(void)
{
	int er = errno;

	fprintf(stderr, "Aborted; errno = %d (%s)\n", er, strerror(er));
	exit(1);
}

enum TOKEN_TYPES
{
	TT_IDENTIFIER, TT_NUMBER, TT_STRING,
	TT_EQUAL, TT_BRACKET_OPEN, TT_BRACKET_CLOSE,
	TT_DOCUMENT, TT_SEMICOLON, TT_DOT, TT_HASH,
	TT_EOF
};

typedef struct Token_s
{
	enum TOKEN_TYPES Type;
	size_t Start, End;

	union
	{
		char const * sValue;
		double nValue;
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

void AppendToken(LexerState * l, Token * tk)
{
	tk->Next = NULL;

	if (l->lastToken == NULL)
		l->Tokens = l->lastToken = tk;
	else
	{
		l->lastToken->Next = tk;
		l->lastToken = tk;
	}
}

//	This returns a pointer to the last character of an identifier
//	in the given string.
char * LexIdentifier(LexerState * l, char * str, size_t len)
{
	int leadBytesLeft = 0;
	while (len-- > 0)
	{
		char c = *str;

		if (leadBytesLeft > 0)
			switch ((unsigned char)c)
			{
				//	All the valid leading bytes.
			case 128 ... 191:
				--leadBytesLeft;
				++str;
				break;

			default:
				l->ErrorSink(l, (size_t)(str - l->Buffer), "Expected UTF-8 lead byte; sequence is invalid.");
				return NULL;
			}
		else
			switch ((unsigned char)c)
			{
				//	UTF-8 first bytes that need one lead byte.
			case 192 ... 223:
				leadBytesLeft = 1;
				++str;
				break;

				//	UTF-8 first bytes that need two lead bytes.
			case 224 ... 239:
				leadBytesLeft = 2;
				++str;
				break;
			
				//	UTF-8 first bytes that need three lead bytes.
			case 240 ... 247:
				leadBytesLeft = 3;
				++str;
				break;

			case 248 ... 255:
				l->ErrorSink(l, (size_t)(str - l->Buffer), "UTF-8 first byte requiring more than 3 lead bytes is invalid.");
				return NULL;

			case 'a' ... 'z': case 'A' ... 'Z': case '_':
				//	Note: hyphens and digits are allowed in identifiers, just not at the beginning.
			case '0' ... '9': case '-':
				++str;	//	Move on to the next character.
				break;

				//	Invalid encoding bytes will be reported by the main lexer loop.
			default:
				goto end_of_identifier;
			}
	}

	if (leadBytesLeft > 0)
	{
		l->ErrorSink(l, l->InputSize, "Unfinished UTF-8 multi-byte sequence.");
		return NULL;
	}

end_of_identifier:
	return str - 1;
}

//	This returns a pointer to the last character of a number
//	in the given string, and sets the nValue of the given token
//	to the numeric representation of the number found here.
char * LexNumber(LexerState * l, char * str, size_t len)
{
	if (*str == '+' || *str == '-')
		++str, --len;

	char * w = str;

	if (*str == '0' && len > 2)
	{
		char c = str[1];
		str += 2;
		len -= 2;

		switch (c)
		{
		case 'b': goto lex_binary_number;
		case 'o': goto lex_octal_number;
		case 'x': goto lex_hexadecimal_number;

		//	This means decimal, and it's necessary to give back one character.
		case '1' ... '9':
			--str;
			++len;
			break;

			//	Dot and 'e' mean float, and the two characters here are needed.
		case '.': case 'e':
			str -= 2;	//	Undo these operations.
			len += 2;	//	And continue to decimal parsing.
			break;

			//	In these cases, the first two characters can be safely discarded.
		case 'd': case '_': case '0':
			break;
		}
	}

lex_decimal_number:
	while (len-- > 0)
	{
		char c = *str++;

		switch (c)
		{
		case '1' ... '9':
			*w++ = c;
			break;

			//	Underscores inside numbers are just for spacing.
		case '_':
			break;

		default:
			goto end_of_number;
		}
	}

lex_binary_number:

lex_octal_number:

lex_hexadecimal_number:

end_of_number:
	return str - 1;
}

//	This returns a pointer to the final unescaped double quotes
//	of a string.
char * LexString(LexerState * l, char * str, size_t len)
{
	bool inEscape = false;
	char * w = str;
	int leadBytesLeft = 0;

	while (len-- > 0)
	{
		char c = *str;

		if (inEscape)
		{
			switch ((unsigned char)c)
			{
			case 'a': *w++ = '\a'; break;
			case 'b': *w++ = '\b'; break;
			case 'f': *w++ = '\f'; break;
			case 'n': *w++ = '\n'; break;
			case 'r': *w++ = '\r'; break;
			case 't': *w++ = '\t'; break;
			case 'v': *w++ = '\v'; break;
			case '0': *w++ = '\0'; break;

				//	UTF-8 multi-byte sequences are non-sensical here...
			case 128 ... 255:
				l->ErrorSink(l, (size_t)(str - l->Buffer), "Unexpected UTF-8 multi-byte sequence byte after backslash in string.");
				return NULL;

			default:
				*w++ = c;
				break;
			}

			inEscape = false;
		}
		else if (leadBytesLeft > 0)
			switch ((unsigned char)c)
			{
				//	All the valid leading bytes.
			case 128 ... 191:
				--leadBytesLeft;
				++str;
				break;

			default:
				l->ErrorSink(l, (size_t)(str - l->Buffer), "Expected UTF-8 lead byte; sequence is invalid.");
				return NULL;
			}
		else
			switch ((unsigned char)c)
			{
			case '\\':
				inEscape = true;
				break;

			case '"':
				if (leadBytesLeft > 0)
				{
					l->ErrorSink(l, l->InputSize, "Unfinished UTF-8 multi-byte sequence.");
					return NULL;
				}

				*w = '\0';
				return str;

			case '\a': case '\b': case '\f': case '\n': case '\r':
			case '\t': case '\v': case '\0':
				if (l->ErrorSink(l, (size_t)(str - l->Buffer), "Unescaped special character encountered in string."))
					return NULL;
				else
					break;

			case 128 ... 191:
				if (l->ErrorSink(l, (size_t)(str - l->Buffer), "Unexpected UTF-8 leading byte."))
					return NULL;
				else
					break;

			case 192 ... 223:
				leadBytesLeft = 1;
				*w++ = c;
				break;

			case 224 ... 239:
				leadBytesLeft = 2;
				*w++ = c;
				break;

			case 240 ... 247:
				leadBytesLeft = 3;
				*w++ = c;
				break;

			case 248 ... 255:
				l->ErrorSink(l, (size_t)(str - l->Buffer), "UTF-8 first byte requiring more than 3 lead bytes is invalid.");
				return NULL;

			default:
				*w++ = c;
				break;
			}

		++str;
	}

	//	Reaching this point means the end of the input was reached before
	//	the proper end of a string. Sad.
	l->ErrorSink(l, l->InputSize, "Unterminated string.");
	return NULL;
}

//	This returns a pointer to the last character of a document node.
char * LexDocument(LexerState * l, char * str, size_t len)
{
	char * closeSequenceStart = NULL;
	int startEquals = 0;

	while (len-- > 0)
	{
		char c = *str++;

		switch (c)
		{
		case '=':
			++startEquals;
			break;

		case '[':
			goto post_opening_sequence;

		default:
			if (l->ErrorSink(l, (size_t)(str - l->Buffer - 1), "Unexpected character in document opening sequence."))
				return NULL;
			else
				break;
		}
	}

	//	Reaching this point means the end of the input was reached before
	//	the proper document opening sequence was finished.
	l->ErrorSink(l, l->InputSize, "Unterminated document opening sequence.");
	return NULL;

post_opening_sequence:
	//	If a newline is found after the opening sequence, it's discarded.
	if (*str == '\n')
	{
		++str;
		--len;
	}
	else if (*str == '\r' && len > 1 && str[1] == '\n')
	{
		str += 2;
		len -= 2;
	}

	l->workingToken->sValue = str;

	while (len-- > 0)
	{
		char c = *str++;

		switch (c)
		{
			//	Do nothing.
		case '=': break;

		case ']':
			if (closeSequenceStart != NULL && str - closeSequenceStart - 2 == startEquals)
				goto post_closing_sequence;
			else
				closeSequenceStart = str - 1;
			break;

		default:
			closeSequenceStart = NULL;
			break;
		}
	}

	//	Reaching this point means the end of the input was reached before
	//	the proper document opening sequence was finished.
	l->ErrorSink(l, l->InputSize, "Unterminated document body.");
	return NULL;

post_closing_sequence:
	//	If a newline is found before the closing sequence, it's discarded.
	if (*(closeSequenceStart - 1) == '\n')
	{
		if (*(closeSequenceStart - 2) == '\r')
		{
			str += 2;
			len -= 2;
			closeSequenceStart += 2;
		}
		else
		{
			++str;
			--len;
			--closeSequenceStart;
		}
	}

	*closeSequenceStart = '\0';
	return str - 1;
}

//	This returns a pointer to the last character of a comment.
char * LexComment(LexerState * l, char * str, size_t len)
{
	if (len > 0)
	{
		--len;

		switch (*str++)
		{
		case '/':
			goto lex_line_comment;

		case '*':
			goto lex_block_comment;

		default:
			//	This lets the main loop deal with reporting this error in particular.
			return str - 1;
		}
	}

	//	This point is reached when the length is 0.
	l->ErrorSink(l, l->InputSize - 1, "Unexpected character.");
	return str;

lex_line_comment:
	while (len-- > 0)
		if (*str++ == '\n')
			return str - 1;

	//	End of input is a valid ending for this comment.
	return str;

lex_block_comment:
	(void)l;
	bool foundAsterisk = false;

	while (len-- > 0)
	{
		char c = *str++;

		switch (c)
		{
		case '*':
			foundAsterisk = true;
			break;

		case '/':
			if (foundAsterisk)
				return str - 1;
			else
				break;

		default:
			foundAsterisk = false;
			break;
		}
	}

	l->ErrorSink(l, l->InputSize, "Unterminated block comment.");
	return str;
}

LexerState * Lex(char * str, size_t const len, LexerErrorSink ers)
{
	LexerState * l = calloc(1, sizeof(LexerState));
	l->Input = str;
	l->InputSize = len;
	l->Buffer = str = malloc(len + 1);
	l->ErrorSink = ers;

	memcpy(str, l->Input, len + 1);

	char * r = str - 1;
	Token * tk = l->workingToken = malloc(sizeof(Token));

	while (r < str + len)
	{
		char c = *(++r);
		// printf("Char #%zd: %c\n", (size_t)(r - str), c);

		switch ((unsigned char)c)
		{
			//	Whitespace, tabulators, newlines, and carriage returns
			//	are completely ignored.
		case ' ': case '\t': case '\n': case '\r':
			*r = '\0';	//	Turn this into a null terminator.
			break;

			//	Alphabetic letters and underscores start an identifier.
			//	So do UTF-8 first bytes.
		case 'a' ... 'z': case 'A' ... 'Z': case '_': case 192 ... 255:
			tk->Type = TT_IDENTIFIER;
			tk->Start = (size_t)(r - str);
			tk->sValue = r;

			r = LexIdentifier(l, r, len - (size_t)(r - str));
			goto yield_token;

			//	Digits and minus start a number.
		case '0' ... '9': case '-':
			tk->Type = TT_NUMBER;
			tk->Start = (size_t)(r - str);

			r = LexNumber(l, r, len - (size_t)(r - str));
			goto yield_token;

			//	Double-quoted string.
		case '"':
			tk->Type = TT_STRING;
			tk->Start = (size_t)(r - str);
			tk->sValue = r + 1;
			*r = '\0';

			r = LexString(l, r + 1, len - (size_t)(r - str));
			goto yield_token;

			//	Opening square brackets start documents.
		case '[':
			tk->Type = TT_DOCUMENT;
			tk->Start = (size_t)(r - str);
			*r = '\0';

			r = LexDocument(l, r + 1, len - (size_t)(r - str));
			goto yield_token;

			//	Start of a comment.
		case '/':
			*r = '\0';	//	This might show up right after an identifier.

			r = LexComment(l, r + 1, len - (size_t)(r - str));
			break;	//	This will not yield a token.

			//	The single-character tokens.
		case '=':
			tk->Type = TT_EQUAL;
			goto single_char_tokens;
		case '.':
			tk->Type = TT_DOT;
			goto single_char_tokens;
		case '#':
			tk->Type = TT_HASH;
			goto single_char_tokens;
		case '{':
			tk->Type = TT_BRACKET_OPEN;
			goto single_char_tokens;
		case '}':
			tk->Type = TT_BRACKET_CLOSE;
			goto single_char_tokens;
		case ';':
			tk->Type = TT_SEMICOLON;
			//	Fallthrough.
		
		single_char_tokens:
			tk->Start = (size_t)(r - str);
			*r = '\0';
			goto yield_token;

		yield_token:
			//	Null means lexing must stop.
			if (!r)
				return l;

			tk->End = (size_t)(r - str);
			AppendToken(l, tk);
			l->workingToken = tk = malloc(sizeof(Token));
			break;

			//	These are UTF-8 leading bytes in a multi-byte sequence.
		case 128 ... 191:
			if (l->ErrorSink(l, (size_t)(r - str), "Unexpected UTF-8 leading byte."))
				return l;
			else
				break;

		case '\0':
			//	A null terminator is only allowed at the end of the input.
			if (r == str + len)
				goto finish_lexing;
			//	else, fall through

			//	Other characters don't start valid tokens.
		default:
			if (l->ErrorSink(l, (size_t)(r - str), "Unexpected character."))
				return l;
			else
				break;
		}
	}

finish_lexing:
	tk->Type = TT_EOF;
	tk->Start = tk->End = len;
	AppendToken(l, tk);

	return l;
}

void FreeLexerState(LexerState * l)
{
	free((void *)(l->Buffer));

	for (Token const * tk = l->Tokens; tk != NULL; /* nothing */)
	{
		Token const * tkNext = tk->Next;
		free((void *)tk);
		tk = tkNext;
	}
}

bool ReportLexerError(LexerState * l, size_t loc, char const * err)
{
	long line = 1, lastnl = -1, lastwsp = -1, nextnl, i;

	//	Find the start of the line where the error is, as well as
	//	the line number.
	for (i = 0; i <= (long)loc; ++i)
		if (l->Input[i] == '\n')
		{
			lastnl = lastwsp = i;
			++line;
		}
		else if (lastwsp == i - 1)
			switch (l->Input[i])
			{
			case ' ': case '\t':
				lastwsp = i;
			default:
				break;
			}

	//	Find the end of the line where the error is.
	for (/* nothing */; i <= (long)l->InputSize; ++i)
		if (l->Input[i] == '\n' || l->Input[i] == '\0')
		{
			nextnl = i;
			break;
		}

	fprintf(stderr, "%zd:%ld: (%2X) %s\n", line, (long)loc - lastnl, l->Input[loc], err);

	if (nextnl > lastnl)
	{
		fwrite(l->Input + lastnl + 1, nextnl - lastnl - 1, 1, stderr);
		putc('\n', stderr);

		if (lastwsp > lastnl)
			fwrite(l->Input + lastnl + 1, lastwsp - lastnl, 1, stderr);

		if ((long)loc > lastwsp)
			fprintf(stderr, "%*.s^\n", (int)(loc - lastwsp - 1), "");
	}

	return false;
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

	LexerState * l = Lex(str, cnt, &ReportLexerError);

	puts("Finished lexing.");

	for (Token const * tk = l->Tokens; tk != NULL; tk = tk->Next)
	{
		putchar('[');

		switch (tk->Type)
		{
		case TT_IDENTIFIER:		fputs("IDENTIFIER", stdout); break;
		case TT_NUMBER:			fputs("NUMBER\t", stdout); break;
		case TT_STRING:			fputs("STRING\t", stdout); break;
		case TT_EQUAL:			fputs("EQUAL\t", stdout); break;
		case TT_BRACKET_OPEN:	fputs("BRK_OPEN", stdout); break;
		case TT_BRACKET_CLOSE:	fputs("BRK_CLOSE", stdout); break;
		case TT_DOCUMENT:		fputs("DOCUMENT", stdout); break;
		case TT_SEMICOLON:		fputs("SEMICOLON", stdout); break;
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

		case TT_NUMBER:
			printf("; %f]\n", tk->nValue);
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
