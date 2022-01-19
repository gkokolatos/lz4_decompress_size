/*
 * Copyright (c) 2022, Georgios
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "lz4frame.h"
#include "cfp.h"

#define EOL '\n'

struct Cfp
{
	FILE   *fp;

	int		inited;
	LZ4F_decompressionContext_t	dtx;
	size_t	allocedlen;
	char   *outbuf;

	size_t	savedallocedlen;
	size_t	savedlen;
	char   *savedbuf;
};

/* internal use functions */
static int
has_suffix(const char *path, const char *suffix)
{
	size_t	plen = strlen(path);
	size_t	slen = strlen(suffix);

	if (plen <= slen)
		return 1;

	return strncmp(path + plen - slen, suffix, slen);
}

static int
cfp_lazy(Cfp *cfp, size_t size)
{
	size_t	status;

	if (cfp->inited)
		return 0;

	status = LZ4F_createDecompressionContext(&cfp->dtx, LZ4F_VERSION);
	if (LZ4F_isError(status))
	{
		fprintf(stderr, "failed to create decompression context: %s\n",
				LZ4F_getErrorName(status));
		return 1;
	}

	/* make certain that you can read at least one full header */
	cfp->allocedlen = size > LZ4F_HEADER_SIZE_MAX ? size : LZ4F_HEADER_SIZE_MAX;
	cfp->savedallocedlen = cfp->allocedlen;
	cfp->outbuf = malloc(cfp->allocedlen);
	cfp->savedbuf = malloc(cfp->savedallocedlen);
	cfp->savedlen = 0;

	if (!cfp->outbuf || !cfp->savedbuf)
	{
		fprintf(stderr, "malloc failed");
		return 1;
	}

	cfp->inited = 1;

	return 0;
}

/*
 * Read up to size from saved buf
 * if EOL flag is set, then stop at EOL, if comes first
 */
static size_t
cfp_read_saved(char * const buf, size_t size, Cfp *cfp, int eol)
{
	char   *p;
	size_t	readlen = 0;

	if (cfp->savedlen == 0)
		return readlen;

	readlen = cfp->savedlen <= size ? cfp->savedlen : size;
	if (eol && (p = memchr(cfp->savedbuf, EOL, readlen)))
		readlen = ++p - cfp->savedbuf;

	memcpy(buf, cfp->savedbuf, readlen);
	cfp->savedlen -= readlen;

	if (cfp->savedlen > 0)
		memmove(cfp->savedbuf, cfp->savedbuf + readlen, cfp->savedlen);

	return readlen;
}

/* public functions */
Cfp *
cfopen(const char *path)
{
	Cfp	   *cfp = NULL;

	if (has_suffix(path, ".lz4"))
	{
		fprintf(stderr, "invalid input, missing .lz4  suffix %s\n",
				path);
		return NULL;
	}

	cfp = calloc(1, sizeof(*cfp));
	if (cfp == NULL)
	{
		fprintf(stderr, "calloc failed %s", strerror(errno));
		return NULL;
	}

	cfp->fp = fopen(path, "rb");
	if (cfp->fp == NULL)
	{
		fprintf(stderr, "failed to open input file %s, %s\n", path, strerror(errno));
		free(cfp);
		return NULL;
	}

	/* Be explicit although calloc has taken care of it */
	cfp->inited = 0;

	return cfp;
}

int
cfclose(Cfp *cfp)
{
	if (cfp->fp && fclose(cfp->fp))
	{
		fprintf(stderr, "failed to close file %s", strerror(errno));
		return 1;
	}

	if (cfp->inited)
	{
		LZ4F_freeDecompressionContext(cfp->dtx);
		free(cfp->outbuf);
		free(cfp->savedbuf);
	}

	free(cfp);

	return 0;
}
/*
 * Populate buf with size, if available, decompressed bytes
 */
ssize_t
cfread(char * const buf, size_t size, Cfp *cfp)
{
	size_t	dsize = 0;
	size_t	rsize;

	char   *readbuf;

	/* Lazy init */
	if (cfp->inited == 0 && cfp_lazy(cfp, size))
		return -1;

	/* Verfiy that there is enough space in the outbuf */
	if (size > cfp->allocedlen)
	{
		cfp->allocedlen = size;
		cfp->outbuf = realloc(cfp->outbuf, cfp->allocedlen);
	}

	/* use already decompressed content if available */
	dsize = cfp_read_saved(buf, size, cfp, 0);
	if (dsize == size)
		return dsize;

	readbuf = malloc(size);

	do
	{
		char   *rp;
		char   *rend;

		rsize = fread(readbuf, 1, size, cfp->fp);
		if (rsize < size && !feof(cfp->fp))
		{
			fprintf(stderr, "failed to read from stream %s", strerror(errno));
			return -1;
		}

		rp = readbuf;
		rend = readbuf + rsize;

		while (rp < rend)
		{
			size_t	status;
			size_t	outlen = cfp->allocedlen;
			size_t	read_remain = rend - rp;

			memset(cfp->outbuf, 0, outlen);
			status = LZ4F_decompress(cfp->dtx, cfp->outbuf, &outlen,
									 rp, &read_remain, NULL);
			if (LZ4F_isError(status))
			{
				fprintf(stderr, "failed to decompress, %s\n",
						LZ4F_getErrorName(status));
				return -1;
			}

			rp += read_remain;

			/* fill in what space is available in buf */
			if (outlen > 0 && dsize < size)
			{
				size_t	lib = size - dsize;
				size_t	len = outlen < lib ? outlen : lib;

				memcpy(buf + dsize, cfp->outbuf, len);
				dsize += len;

				/* move what did not fit, if any, at the begining of the buf */
				if (len < outlen)
					memmove(cfp->outbuf, cfp->outbuf + len, outlen - len);
				outlen -= len;
			}

			/* if there is available output, save it */
			if (outlen > 0)
			{
				while (cfp->savedlen + outlen > cfp->savedallocedlen)
				{
					cfp->savedallocedlen *= 2;
					cfp->outbuf = realloc(cfp->outbuf, cfp->savedallocedlen);
					cfp->savedbuf = realloc(cfp->savedbuf, cfp->savedallocedlen);
				}

				memcpy(cfp->savedbuf + cfp->savedlen, cfp->outbuf, outlen);
				cfp->savedlen += outlen;
			}
		}
	} while (rsize == size && dsize < size);

	free(readbuf);

	return dsize;
}

int
cfgetc(Cfp *cfp)
{
	char	buf = EOF;
	ssize_t	ret;
	if (!cfp)
		return -1;

	ret = cfread(&buf, sizeof(buf), cfp);
	if (ret < 0)
		return -1;

	return (int)buf;
}

char *
cfgets(char * const buf, size_t size, Cfp *cfp)
{
	size_t	dsize = 0;
	size_t	rsize;
	int		found = 0;

	char   *readbuf;

	/* Lazy init */
	if (cfp->inited == 0 && cfp_lazy(cfp, size))
		return NULL;

	/* Verfiy that there is enough space in the outbuf */
	if (size > cfp->allocedlen)
	{
		cfp->allocedlen = size;
		cfp->outbuf = realloc(cfp->outbuf, cfp->allocedlen);
	}

	/* NULL terminate buf */
	memset(buf, '\0', size);

	/* use already decompressed content if available */
	dsize = cfp_read_saved(buf, size -1, cfp, 1);
	if (dsize == size -1 || (dsize > 0 && *(buf + dsize -1) == EOL))
		return buf;

	readbuf = malloc(size);

	do
	{
		char   *rp;
		char   *rend;

		rsize = fread(readbuf, 1, size -1, cfp->fp);
		if (rsize < size -1 && !feof(cfp->fp))
		{
			fprintf(stderr, "failed to read from stream %s", strerror(errno));
			return NULL;
		}

		rp = readbuf;
		rend = readbuf + rsize;

		while (rp < rend)
		{
			size_t	status;
			size_t	outlen = cfp->allocedlen;
			size_t	read_remain = rend - rp;

			memset(cfp->outbuf, 0, outlen);
			status = LZ4F_decompress(cfp->dtx, cfp->outbuf, &outlen,
									 rp, &read_remain, NULL);
			if (LZ4F_isError(status))
			{
				fprintf(stderr, "failed to decompress, %s\n",
						LZ4F_getErrorName(status));
				return NULL;
			}

			rp += read_remain;

			/* fill in what space is available in buf if EOL not already filled*/
			if (outlen > 0 && dsize < size -1 && found == 0)
			{
				char   *p;
				size_t	lib = size -1 - dsize;
				size_t	len = outlen < lib ? outlen : lib;

				if ((p = memchr(cfp->outbuf, EOL, outlen)) && (size_t)(++p - cfp->outbuf) <= len)
				{
					len = p - cfp->outbuf;
					found = 1;
				}

				memcpy(buf + dsize, cfp->outbuf, len);
				dsize += len;

				/* move what did not fit, if any, at the begining of the buf */
				if (len < outlen)
					memmove(cfp->outbuf, cfp->outbuf + len, outlen - len);
				outlen -= len;
			}

			/* if there is available output, save it */
			if (outlen > 0)
			{
				while (cfp->savedlen + outlen > cfp->savedallocedlen)
				{
					cfp->savedallocedlen *= 2;
					cfp->outbuf = realloc(cfp->outbuf, cfp->savedallocedlen);
					cfp->savedbuf = realloc(cfp->savedbuf, cfp->savedallocedlen);
				}

				memcpy(cfp->savedbuf + cfp->savedlen, cfp->outbuf, outlen);
				cfp->savedlen += outlen;
			}
		}
	} while (rsize == size -1 && (dsize < size -1 || found == 1));

	free(readbuf);

	if (dsize == 0)
		return NULL;

	return buf;
}
