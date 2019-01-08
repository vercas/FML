#pragma once

#include "parser.h"
#include <stdio.h>

typedef int (*FmlBeautifierSink)(char const * str, size_t len, void * ctxt);

int FmlBeautifyEx(Node const * n, FmlBeautifierSink sink, void * ctxt);

int FmlBeautify(Node const * n, FILE * file);
