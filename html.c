/*
 * Copyright (c) 2002, 2007, Scott Nicol <esniper@users.sf.net>
 * All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include "buffer.h"
#include "http.h"
#include "html.h"
#include "esniper.h"

/*
 * rudimentary HTML parser, maybe, we should use libxml2 instead?
 */

/*
 * Conversion tables for accented letters (named entities)
 * Includes many latin1 additions (acute, grave, cedil, circ, tilde and uml)
 * e.g. http://htmlhelp.com/reference/html40/entities/latin1.html
 *
 * How it works:
 * e.g. Entity : &auml; -> table index 'a' (0x61) -> table content 'ä' (0xc3a4) -> utf-8 character
 */

typedef int convtab_t[64];

convtab_t acute2utf8 = {0, 0xc381, 0, 0, 0, 0xc389, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xc393, /* 40 */
			0, 0, 0, 0, 0, 0xc39a, 0, 0, 0, 0xc39d, 0, 0, 0, 0, 0, 0,      /* 50 */
			0, 0xc3a1, 0, 0, 0, 0xc3a9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xc3b3, /* 60 */
			0, 0, 0, 0, 0, 0xc3ba, 0, 0, 0, 0xc3bd, 0, 0, 0, 0, 0, 0 };    /* 70 */

convtab_t grave2utf8 = {0, 0xc380, 0, 0, 0, 0xc388, 0, 0, 0, 0, 0xc38c, 0, 0, 0, 0, 0xc392, /* 40 */
			0, 0, 0, 0, 0, 0xc399, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                /* 50 */
			0, 0xc3a0, 0, 0, 0, 0xc3a8, 0, 0, 0, 0, 0xc3ac, 0, 0, 0, 0, 0xc3b2, /* 60 */
			0, 0, 0, 0, 0, 0xc3b9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };              /* 70 */

convtab_t cedil2utf8 = {0, 0, 0, 0xc387, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 40 */
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,      /* 50 */
			0, 0, 0, 0xc3a7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 60 */
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };    /* 70 */

convtab_t circ2utf8 =  {0, 0xc382, 0, 0, 0, 0xc38a, 0, 0, 0, 0xc38e, 0, 0, 0, 0, 0, 0xc394, /* 40 */
			0, 0, 0, 0, 0, 0xc39b, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                /* 50 */
			0, 0xc3a2, 0, 0, 0, 0xc3aa, 0, 0, 0, 0xc3ae, 0, 0, 0, 0, 0, 0xc3b4, /* 60 */
			0, 0, 0, 0, 0, 0xc3bb, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };              /* 70 */

convtab_t tilde2utf8 = {0, 0xc383, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xc391, 0, /* 40 */
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,           /* 50 */
			0, 0xc3a3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xc3b1, 0, /* 60 */
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };         /* 70 */

convtab_t uml2utf8[] = {0, 0xc384, 0, 0, 0, 0xc38b, 0, 0, 0, 0xc38f, 0, 0, 0, 0, 0, 0xc396, /* 40 */
			0, 0, 0, 0xc39f, 0, 0xc39c, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,           /* 50 */
			0, 0xc3a4, 0, 0, 0, 0xc3ab, 0, 0, 0, 0xc3af, 0, 0, 0, 0, 0, 0xc3b6, /* 60 */
			0, 0, 0, 0xc39f, 0, 0xc3bc, 0, 0, 0, 0xc3bf, 0, 0, 0, 0, 0, 0 };    /* 70 */

/*
 * Add up to 4 bytes of utf8 character to buffer
 */

int addutf8char(char* buf, size_t bufLen, int c)
{
	char* start = buf;

	*buf='?';

	while(c) {
		if(c&0xFF000000) *buf++ = (c&0xFF000000) >> 24;
		c = (c & 0x00FFFFFF) << 8;
	}

	return (buf == start ? 0 : buf - start - 1);
}

/*
 * Converts ansii char to utf-8 using conversion table above
 */

int conv2utf8(char* buf, char c, convtab_t* convTab, int convLen)
{
	int ret = 0;

	*buf = '?';
	c-=0x40;

	if((unsigned)c < convLen && (*convTab)[c]) return addutf8char(buf, -1, (*convTab)[c]);

	return ret;
}

/*
 * Html parser definitions
 */

typedef enum { ne_table, ne_utf8, ne_char } namedentitytype_t;

typedef struct _namedentity {
	char* entity;
	namedentitytype_t t;
	union {
		convtab_t* tab;		/* Entity has multiple variations - using table */
		int c;			/* 1:1 relationship - c contains utf8 character */
	};
} namedentity_t;

namedentity_t entity2utf8[] = { "acute", ne_table, { .tab = (convtab_t*) &acute2utf8 },
				"grave", ne_table, { .tab = (convtab_t*) &grave2utf8},
				"cedil", ne_table, { .tab = (convtab_t*) &cedil2utf8},
				"circ", ne_table, { .tab = (convtab_t*) &circ2utf8},
				"tilde", ne_table, { .tab = (convtab_t*) &tilde2utf8 },
				"uml", ne_table, { .tab = (convtab_t*) &uml2utf8 },
				"trade", ne_utf8, { .c = 0xe284a2 },
				"amp", ne_char, { .c = (int)'&' },
				"gt", ne_char, { .c = (int)'>' },
				"lt", ne_char, { .c = (int)'<' },
				"quot",  ne_char, { .c = (int)'&' } };

/*
 * Get next tag text, eliminating leading and trailing whitespace
 * and leaving only a single space for all internal whitespace.
 */
const char *
getTag(memBuf_t *mp)
{
	static char *buf = NULL;
	static size_t bufsize = 0;
	size_t count = 0;
	int inStr = 0, comment = 0, c;

	if (memEof(mp)) {
		log(("getTag(): returning NULL\n"));
		return NULL;
	}
	while ((c = memGetc(mp)) != EOF && c != '<')
		;
	if (c == EOF) {
		log(("getTag(): returning NULL\n"));
		return NULL;
	}

	/* first char - check for comment */
	c = memGetc(mp);
	if (c == '>') {
		log(("getTag(): returning empty tag\n"));
		return "";
	} else if (c == EOF) {
		log(("getTag(): returning NULL\n"));
		return NULL;
	}
	addchar(buf, bufsize, count, (char)c);
	if (c == '!') {
		int c2 = memGetc(mp);

		if (c2 == '>' || c2 == EOF) {
			term(buf, bufsize, count);
			log(("getTag(): returning %s\n", buf));
			return buf;
		}
		addchar(buf, bufsize, count, (char)c2);
		if (c2 == '-') {
			int c3 = memGetc(mp);

			if (c3 == '>' || c3 == EOF) {
				term(buf, bufsize, count);
				log(("getTag(): returning %s\n", buf));
				return buf;
			}
			addchar(buf, bufsize, count, (char)c3);
			comment = 1;
		}
	}

	if (comment) {
		while ((c = memGetc(mp)) != EOF) {
			if (c=='>' && buf[count-1]=='-' && buf[count-2]=='-') {
				term(buf, bufsize, count);
				log(("getTag(): returning %s\n", buf));
				return buf;
			}
			if (isspace(c) && buf[count-1] == ' ')
				continue;
			addchar(buf, bufsize, count, (char)c);
		}
	} else {
		while ((c = memGetc(mp)) != EOF) {
			switch (c) {
			case '\\':
				addchar(buf, bufsize, count, (char)c);
				c = memGetc(mp);
				if (c == EOF) {
					term(buf, bufsize, count);
					log(("getTag(): returning %s\n", buf));
					return buf;
				}
				addchar(buf, bufsize, count, (char)c);
				break;
			case '>':
				if (inStr)
					addchar(buf, bufsize, count, (char)c);
				else {
					term(buf, bufsize, count);
					log(("getTag(): returning %s\n", buf));
					return buf;
				}
				break;
			case ' ':
			case '\n':
			case '\r':
			case '\t':
			case '\v':
				if (inStr)
					addchar(buf, bufsize, count, (char)c);
				else if (count > 0 && buf[count-1] != ' ')
					addchar(buf, bufsize, count, ' ');
				break;
			case '"':
				inStr = !inStr;
				/* fall through */
			default:
				addchar(buf, bufsize, count, (char)c);
			}
		}
	}
	term(buf, bufsize, count);
	log(("getTag(): returning %s\n", count ? buf : "NULL"));
	return count ? buf : NULL;
}

/*
 * Get next non-tag text, eliminating leading and trailing whitespace
 * and leaving only a single space for all internal whitespace.
 */
char *
getNonTag(memBuf_t *mp)
{
	static char *buf = NULL;
	static size_t bufsize = 0;
	size_t count = 0, amp = 0;
	int c, f, i;

	char* lang=getenv("LANG");
	char* utf8=strstr(lang, "UTF-8");

	if (memEof(mp)) {
		log(("getNonTag(): returning NULL\n"));
		return NULL;
	}
	while ((c = memGetc(mp)) != EOF) {
		switch (c) {
		case '<':
			memUngetc(mp);
			if (count) {
				if (buf[count-1] == ' ')
					--count;
				term(buf, bufsize, count);
				log(("getNonTag(): returning %s\n", buf));
				return buf;
			} else
				(void)getTag(mp);
			break;
		case ' ':
		case '\n':
		case '\r':
		case '\t':
		case '\v':
		case 0x82: /* UTF-8 */
		case 0xC2: /* UTF-8 */
		case 0xA0: /* iso-8859-1 nbsp */
			if (count && buf[count-1] != ' ')
				addchar(buf, bufsize, count, ' ');
			break;
		case 0xC3: /* UTF-8 */
			if(utf8)
				addchar(buf, bufsize, count, (char)c);
			else {
				if (count && buf[count-1] != ' ')
					addchar(buf, bufsize, count, ' ');
			}
			break;
		case ';':
			if (amp > 0) {
				char *cp = &buf[amp];

				term(buf, bufsize, count);
				if (*cp == '#') {														/* Convert numeric entities */
					if (*(cp+1)=='x' || *(cp+1)=='X')
						sscanf(cp+2, "%x", &c);												/* Hex */
					else
						sscanf(cp+1, "%u", &c);												/* Decimal */

					count = amp + addutf8char(&buf[amp-1], -1, (utf8 ? c : c & 0x000000FF));
				} else {															/* Convert named entities */
					f = 0;
					for(i = 0; f == 0 && i < (sizeof(entity2utf8)/sizeof(namedentity_t)); i++) {
						if(utf8 && !strcmp(cp+1, entity2utf8[i].entity) && entity2utf8[i].t == ne_table) {				/* Using transfer table */
							count = amp + conv2utf8(&buf[amp-1], *cp, entity2utf8[i].tab, sizeof(convtab_t)/sizeof(int));
							f = 1;
						} else if(!strcmp(cp, entity2utf8[i].entity) && (entity2utf8[i].t == ne_utf8 || entity2utf8[i].t == ne_char)) {	/* Up to 4 bytes utf-8 characters */
							count = amp + addutf8char(&buf[amp-1], -1, (utf8 ? entity2utf8[i].c : entity2utf8[i].c & 0x000000FF));
							f = 1;
						}
					}
				}
				if(f) {
					;
				} else if (!strcmp(cp, "nbsp")) {												/* Everything else */
					buf[amp-1] = ' ';
					count = amp;
					if (count && buf[count-1] == ' ')
						--count;
				} else
					addchar(buf, bufsize, count, (char)c);
				amp = 0;
			} else
				addchar(buf, bufsize, count, (char)c);
			break;
		case '&':
			amp = count + 1;
			/* fall through */
		default:
			addchar(buf, bufsize, count, (char)c);
		}
	}
	if (count && buf[count-1] == ' ')
		--count;
	term(buf, bufsize, count);
	log(("getNonTag(): returning %s\n", count ? buf : "NULL"));
	return count ? buf : NULL;
} /* getNonTag() */

char *
getNthNonTagFromString(const char *s, int n)
{
	memBuf_t buf;
	int i;

	strToMemBuf(s, &buf);
	for (i = 1; i < n; i++)
		getNonTag(&buf);
	return myStrdup(getNonTag(&buf));
}

char *
getNonTagFromString(const char *s)
{
	memBuf_t buf;

	strToMemBuf(s, &buf);
	return myStrdup(getNonTag(&buf));
}

int
getIntFromString(const char *s)
{
	memBuf_t buf;

	strToMemBuf(s, &buf);
	return atoi(getNonTag(&buf));
}

const char PAGENAME[] = "var pageName = \"";

/*
 * Get pagename variable, or NULL if not found.
 */
char *
getPageName(memBuf_t *mp)
{
	const char *line;

	log(("getPageName():\n"));
	while ((line = getTag(mp))) {
		char *tmp;

		if (strncmp(line, "!--", 3))
			continue;
		if ((tmp = strstr(line, PAGENAME))) {
			tmp = getPageNameInternal(tmp);
			log(("getPageName(): pagename = %s\n", nullStr(tmp)));
			return tmp;
		}
	}
	log(("getPageName(): Cannot find pagename, returning NULL\n"));
	return NULL;
}

char *
getPageNameInternal(char *s)
{
	char *pagename = s + sizeof(PAGENAME) - 1;
	char *quote = strchr(pagename, '"');

	if (!quote) { /* TG: returns NULL, not \0 */
		log(("getPageNameInternal(): Cannot find trailing quote in pagename: %s\n", pagename));
		return NULL;
	}
	*quote = '\0';
	log(("getPageName(): pagename = %s\n", pagename));
	return pagename;
}

/*
 * Search to end of table, returning /table tag (or NULL if not found).
 * Embedded tables are skipped.
 */
const char *
getTableEnd(memBuf_t *mp)
{
	int nesting = 1;
	const char *cp;

	while ((cp = getTag(mp))) {
		if (!strcmp(cp, "/table")) {
			if (--nesting == 0)
				return cp;
		} else if (!strncmp(cp, "table", 5) &&
			   (isspace((int)*(cp+5)) || *(cp+5) == '\0')) {
			++nesting;
		}
	}
	return NULL;
}

/*
 * Search for next table item.  Return NULL at end of a row, and another NULL
 * at the end of a table.
 */
char *
getTableCell(memBuf_t *mp)
{
	int nesting = 1;
	const char *cp, *start = mp->readptr, *end = NULL;
	static char *buf = NULL;
	static size_t bufsize = 0;
	size_t count = 0;

	while ((cp = getTag(mp))) {
		if (nesting == 1 && 
		    (!strncmp(cp, "td", 2) || !strncmp(cp, "th", 2)) &&
		    (isspace((int)*(cp+2)) || *(cp+2) == '\0')) {
			/* found <td>, now must find </td> */
			start = mp->readptr;
		} else if (nesting == 1 && 
			(!strcmp(cp, "/td") || !strcmp(cp, "/th"))) {
			/* end of this item */
			for (end = mp->readptr - 1; *end != '<'; --end)
				;
			for (cp = start; cp < end; ++cp) {
				addchar(buf, bufsize, count, *cp);
			}
			term(buf, bufsize, count);
			return buf;
		} else if (nesting == 1 && !strcmp(cp, "/tr")) {
			/* end of this row */
			return NULL;
		} else if (!strcmp(cp, "/table")) {
			/* end of this table? */
			if (--nesting == 0)
				return NULL;
		} else if (!strncmp(cp, "table", 5) &&
			   (isspace((int)*(cp+5)) || *(cp+5) == '\0')) {
			++nesting;
		}
	}
	/* error? */
	return NULL;
}

/*
 * Return NULL-terminated table row, or NULL at end of table.
 * All cells are malloc'ed and should be freed by the calling function.
 */
char **
getTableRow(memBuf_t *mp)
{
	char **ret = NULL, *cp = NULL;
	size_t size = 0, i = 0;

	do {
		cp = getTableCell(mp);
		if (cp || i) {
			if (i >= size) {
				size += 10;
				ret = (char **)myRealloc(ret, size * sizeof(char *));
			}
			ret[i++] = myStrdup(cp);
		}
	} while ((cp));
	return ret;
}

/*
 * Return number of columns in row, or -1 if null.
 */
int
numColumns(char **row)
{
	int ncols = 0;

	if (!row)
		return -1;
	while (row[ncols++])
		;
	return --ncols;
}

/*
 * Free a TableRow allocated by getTableRow
 */
void
freeTableRow(char **row)
{
	char **cpp;

	if (row) {
		for (cpp = row; *cpp; ++cpp)
			free(*cpp);
		free(row);
	}
}

/*
 * Search for next table tag.
 */
const char *
getTableStart(memBuf_t *mp)
{
	const char *cp;

	while ((cp = getTag(mp))) {
		if (!strncmp(cp, "table", 5) &&
		    (isspace((int)*(cp+5)) || *(cp+5) == '\0'))
			return cp;
	}
	return NULL;
}
