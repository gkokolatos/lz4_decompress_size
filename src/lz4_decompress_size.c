/*
 * Copyright (c) 2022, Georgios
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */
/*
 * Decompress LZ4 compressed input using the Cfp API
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lz4frame.h"

#include "cfp.h"

#define MAX_FILENAME 128
#define MESSAGE_MAX  8

#define EOL '\n'

typedef enum
{
	RANDOM,
	CHARACTER,
	LINE,
	INVALID
} Mode;

static void
usage(const char *prog)
{
	fprintf(stdout, "%s LZ4 frame API decompress of size into buffer\n", prog);
	fprintf(stdout, "Usage: %s <mode> <file>\n", prog);
	fprintf(stdout, "where:\n");
	fprintf(stdout, "mode             decompress mode random, char, line [r|c|l]\n");
	fprintf(stdout, "file             LZ4 compressed input file for program\n");
}

static Mode
parse_mode(const char *input)
{
	Mode	mode = INVALID;

	if (!input)
		return INVALID;

	if (!strcmp(input, "r"))
		mode = RANDOM;
	else if (!strcmp(input, "c"))
		mode = CHARACTER;
	else if (!strcmp(input, "l"))
		mode = LINE;

	return mode;
}

/*
 * generate a random length of at least 1 byte, up to buf_len.
 */
static size_t
generate_random_length(size_t buf_len)
{
	return (rand() % buf_len) + 1;
}
static int
decompress_random(Cfp *cfp)
{
	char   *buf;
	size_t	size;
	size_t	read;

	buf = malloc(MESSAGE_MAX);

	/* continue reading until there are enough bytes to be decompressed */
	for (size = generate_random_length(MESSAGE_MAX);
		 (read = cfread(buf, size, cfp)) > 0;
		 size = generate_random_length(MESSAGE_MAX))
		fprintf(stdout, "%.*s", (int)read, buf);

	free(buf);

	return 0;
}

static int
decompress_character(Cfp *cfp)
{
	int c;

	/* continue reading until there are enough bytes to be decompressed */
	while ((c = cfgetc(cfp)) != EOF)
		fprintf(stdout, "%c", c);

	return 0;
}

static int
decompress_line(Cfp *cfp)
{
	char   *buf;
	char   *out;
	size_t	size;

	size = 128;
	buf = malloc(size);
	if (!buf)
		return 1;

	while ((out = cfgets(buf, size, cfp)) != NULL)
	{
		if (strlen(out) < size -1 && strchr(out, EOL) == NULL)
		{
			free(buf);
			return 1;
		}
		fputs(out, stdout);
	}

	free(buf);

	return 0;
}

int
main(int argc, char *argv[])
{
	Cfp		   *cfp;
	Mode		mode;
	const char *prog = argv[0];
	int			ret;

	if (argc != 3)
	{
		usage(prog);
		return 1;
	}

	if ((mode = parse_mode(argv[1])) == INVALID)
	{
		usage(prog);
		return 1;
	}

	/* init things */
	cfp = cfopen(argv[2]);
	if (cfp == NULL)
		return 1;

	switch (mode)
	{
		case RANDOM:
			ret = decompress_random(cfp);
			break;
		case CHARACTER:
			ret = decompress_character(cfp);
			break;
		case LINE:
			ret = decompress_line(cfp);
			break;
		default:
			ret = 1;
	}

	/* cleanup */
	if (cfclose(cfp))
		return 1;

	return ret;
}
