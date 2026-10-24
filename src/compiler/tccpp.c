/*
 *  TCC - Tiny C Compiler
 * 
 *  Copyright (c) 2001-2004 Fabrice Bellard
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "tcc.h"
#undef _tcc_error
#undef gnu_ext
#undef total_bytes
#undef total_lines
#undef total_idents

/* #define to 1 to enable (see parse_pp_string()) */
#define ACCEPT_LF_IN_STRINGS 0

static void tok_print(TCCState *s1, const int *str, const char *msg, ...);
static void next_nomacro(TCCState *s1);
static void parse_number(TCCState *s1, const char *p);
static void parse_string(TCCState *s1, const char *p, int len);


static const char tcc_keywords[] = 
#define DEF(id, str) str "\0"
#include "tcctok.h"
#undef DEF
;

/* WARNING: the content of this string encodes token numbers */
static const unsigned char tok_two_chars[] =
/* outdated -- gr
    "<=\236>=\235!=\225&&\240||\241++\244--\242==\224<<\1>>\2+=\253"
    "-=\255*=\252/=\257%=\245&=\246^=\336|=\374->\313..\250##\266";
*/{
    '<','=', TOK_LE,
    '>','=', TOK_GE,
    '!','=', TOK_NE,
    '&','&', TOK_LAND,
    '|','|', TOK_LOR,
    '+','+', TOK_INC,
    '-','-', TOK_DEC,
    '=','=', TOK_EQ,
    '<','<', TOK_SHL,
    '>','>', TOK_SAR,
    '+','=', TOK_A_ADD,
    '-','=', TOK_A_SUB,
    '*','=', TOK_A_MUL,
    '/','=', TOK_A_DIV,
    '%','=', TOK_A_MOD,
    '&','=', TOK_A_AND,
    '^','=', TOK_A_XOR,
    '|','=', TOK_A_OR,
    '-','>', TOK_ARROW,
    '.','.', TOK_TWODOTS,
    '#','#', TOK_TWOSHARPS,
    0
};

ST_FUNC void skip(TCCState *s1, int c)
{
    if (s1->pp->token != c) {
        char tmp[40];
        pstrcpy(tmp, sizeof tmp, get_tok_str(s1, c, &s1->pp->value));
        _tcc_error(s1, "'%s' expected (got '%s')", tmp, get_tok_str(s1, s1->pp->token, &s1->pp->value));
	}
    next(s1);
}

ST_FUNC void expect(TCCState *s1, const char *msg)
{
    _tcc_error(s1, "%s expected", msg);
}

/* ------------------------------------------------------------------------- */
/* The tokens object owns all interned names and macro stream storage.
 * Compiler diagnostics run only after VO has removed its access records. */
static void *neocc_token_resize(TCCState *s1, void *pointer, size_t size)
{
	struct neocc_token_resize request = { pointer, size };
	void *result = NULL;
	unsigned int quantity;
	int status = vo_interact3(s1->tokens, NEOCC_AC3_TOKENS_RESIZE,
		(unsigned char *)&request, sizeof(request),
		(unsigned char *)&result, sizeof(result), &quantity,
		&neocc_host_caller);

	if (!RP(status) || quantity != sizeof(result))
		_tcc_error(s1, "cannot resize token stream");
	return result;
}

static struct neocc_token_result neocc_token_query(TCCState *s1, int id)
{
	struct neocc_token_result result;
	unsigned int quantity;
	int status = vo_interact3(s1->tokens, NEOCC_AC3_TOKENS_QUERY,
		(unsigned char *)&id, sizeof(id),
		(unsigned char *)&result, sizeof(result), &quantity,
		&neocc_host_caller);

	if (RN(status) || quantity != sizeof(result))
		_tcc_error(s1, "cannot query token");
	return result;
}

ST_FUNC struct neocc_token *neocc_token_at(TCCState *s1, int id)
{
	return neocc_token_query(s1, id).token;
}

ST_FUNC int neocc_token_next(TCCState *s1)
{
	return neocc_token_query(s1, NEOCC_TOKEN_FIRST - 1).next;
}

/* ------------------------------------------------------------------------- */
/* struct neocc_string handling */
static void cstr_realloc(TCCState *s1, struct neocc_string *cstr, int new_size)
{
    if (!neocc_string_reserve(cstr, new_size))
        _tcc_error(s1, "cannot grow compiler string");
}

/* add a byte */
ST_INLN void cstr_ccat(TCCState *s1, struct neocc_string *cstr, int ch)
{
    int size;
    size = cstr->size + 1;
    if (size > cstr->size_allocated)
        cstr_realloc(s1, cstr, size);
    cstr->data[size - 1] = ch;
    cstr->size = size;
}

ST_INLN char *unicode_to_utf8(TCCState *s1, char *b, uint32_t Uc)
{
    if (Uc<0x80) *b++=Uc;
    else if (Uc<0x800) *b++=192+Uc/64, *b++=128+Uc%64;
    else if (Uc-0xd800u<0x800) goto error;
    else if (Uc<0x10000) *b++=224+Uc/4096, *b++=128+Uc/64%64, *b++=128+Uc%64;
    else if (Uc<0x110000) *b++=240+Uc/262144, *b++=128+Uc/4096%64, *b++=128+Uc/64%64, *b++=128+Uc%64;
    else error: _tcc_error(s1, "0x%x is not a valid universal character", Uc);
    return b;
}

/* add a unicode character expanded into utf8 */
ST_INLN void cstr_u8cat(TCCState *s1, struct neocc_string *cstr, int ch)
{
    char buf[4], *e;
    e = unicode_to_utf8(s1, buf, (uint32_t)ch);
    cstr_cat(s1, cstr, buf, e - buf);
}

/* add string of 'len', or of its len/len+1 when 'len' == -1/0 */
ST_FUNC void cstr_cat(TCCState *s1, struct neocc_string *cstr,
                     const char *str, int len)
{
    int size;
    if (len <= 0)
        len = strlen(str) + 1 + len;
    size = cstr->size + len;
    if (size > cstr->size_allocated)
        cstr_realloc(s1, cstr, size);
    memmove(cstr->data + cstr->size, str, len);
    cstr->size = size;
}

/* add a wide char */
ST_FUNC void cstr_wccat(TCCState *s1, struct neocc_string *cstr, int ch)
{
    int size;
    size = cstr->size + sizeof(nwchar_t);
    if (size > cstr->size_allocated)
        cstr_realloc(s1, cstr, size);
    *(nwchar_t *)(cstr->data + size - sizeof(nwchar_t)) = ch;
    cstr->size = size;
}

/* reset string to empty */
ST_FUNC void cstr_reset(struct neocc_string *cstr)
{
    cstr->size = 0;
}

ST_FUNC int cstr_vprintf(TCCState *s1, struct neocc_string *cstr,
                        const char *fmt, va_list ap)
{
    int length = neocc_string_vprintf(cstr, fmt, ap);
    if (length < 0)
        _tcc_error(s1, "cannot grow compiler string");
    return length;
}

ST_FUNC int cstr_printf(TCCState *s1, struct neocc_string *cstr,
                       const char *fmt, ...)
{
    va_list ap; int len;
    va_start(ap, fmt);
    len = cstr_vprintf(s1, cstr, fmt, ap);
    va_end(ap);
    return len;
}

/* XXX: unicode ? */
static void add_char(TCCState *s1, struct neocc_string *cstr, int c)
{
    if (c == '\'' || c == '\"' || c == '\\') {
        /* XXX: could be more precise if char or string */
        cstr_ccat(s1, cstr, '\\');
    }
    if (c >= 32 && c <= 126) {
        cstr_ccat(s1, cstr, c);
    } else {
        cstr_ccat(s1, cstr, '\\');
        if (c == '\n') {
            cstr_ccat(s1, cstr, 'n');
        } else {
            cstr_ccat(s1, cstr, '0' + ((c >> 6) & 7));
            cstr_ccat(s1, cstr, '0' + ((c >> 3) & 7));
            cstr_ccat(s1, cstr, '0' + (c & 7));
        }
    }
}

/* ------------------------------------------------------------------------- */
/* Find or intern a token; its spelling and ID are owned by tokens. */
ST_FUNC struct neocc_token *tok_alloc(TCCState *s1, const char *str, int len)
{
	struct neocc_token_intern request = { str, (size_t)len };
	struct neocc_token_result result;
	unsigned int quantity;
	int status = vo_interact3(s1->tokens, NEOCC_AC3_TOKENS_INTERN,
		(unsigned char *)&request, sizeof(request),
		(unsigned char *)&result, sizeof(result), &quantity,
		&neocc_host_caller);

	if (!RP(status) || quantity != sizeof(result))
		_tcc_error(s1, "cannot intern token");
	return result.token;
}

ST_FUNC int tok_alloc_const(TCCState *s1, const char *str)
{
    return tok_alloc(s1, str, strlen(str))->tok;
}

/* XXX: buffer overflow */
/* XXX: float tokens */
ST_FUNC const char *get_tok_str(TCCState *s1, int v, union neocc_value *cv)
{
    char *p;
    int i, len;

    cstr_reset(&s1->pp->spelling);
    p = s1->pp->spelling.data;

    switch(v) {
    case TOK_CINT:
    case TOK_CUINT:
    case TOK_CLONG:
    case TOK_CULONG:
    case TOK_CLLONG:
    case TOK_CULLONG:
        /* XXX: not quite exact, but only useful for testing  */
        sprintf(p, "%llu", (unsigned long long)cv->i);
        break;
    case TOK_LCHAR:
        cstr_ccat(s1, &s1->pp->spelling, 'L');
    case TOK_CCHAR:
        cstr_ccat(s1, &s1->pp->spelling, '\'');
        add_char(s1, &s1->pp->spelling, cv->i);
        cstr_ccat(s1, &s1->pp->spelling, '\'');
        cstr_ccat(s1, &s1->pp->spelling, '\0');
        break;
    case TOK_PPNUM:
    case TOK_PPSTR:
        return (char*)cv->str.data;
    case TOK_LSTR:
        cstr_ccat(s1, &s1->pp->spelling, 'L');
    case TOK_STR:
        cstr_ccat(s1, &s1->pp->spelling, '\"');
        if (v == TOK_STR) {
            len = cv->str.size - 1;
            for(i=0;i<len;i++)
                add_char(s1, &s1->pp->spelling, ((unsigned char *)cv->str.data)[i]);
        } else {
            len = (cv->str.size / sizeof(nwchar_t)) - 1;
            for(i=0;i<len;i++)
                add_char(s1, &s1->pp->spelling, ((nwchar_t *)cv->str.data)[i]);
        }
        cstr_ccat(s1, &s1->pp->spelling, '\"');
        cstr_ccat(s1, &s1->pp->spelling, '\0');
        break;

    case TOK_CFLOAT:
        return strcpy(p, "<float>");
    case TOK_CDOUBLE:
        return strcpy(p, "<double>");
    case TOK_CLDOUBLE:
        return strcpy(p, "<long double>");
    case TOK_LINENUM:
        return strcpy(p, "<linenumber>");

    /* above tokens have value, the ones below don't */
    case TOK_LT:
        v = '<';
        goto addv;
    case TOK_GT:
        v = '>';
        goto addv;
    case TOK_DOTS:
        return strcpy(p, "...");
    case TOK_A_SHL:
        return strcpy(p, "<<=");
    case TOK_A_SAR:
        return strcpy(p, ">>=");
    case TOK_EOF:
        return strcpy(p, "<eof>");
    case 0: /* anonymous nameless symbols */
        return strcpy(p, "<no name>");
    default:
        v &= ~(SYM_FIELD | SYM_STRUCT);
        if (v < TOK_IDENT) {
            /* search in two bytes table */
            const unsigned char *q = tok_two_chars;
            while (*q) {
                if (q[2] == v) {
                    *p++ = q[0];
                    *p++ = q[1];
                    *p = '\0';
                    return s1->pp->spelling.data;
                }
                q += 3;
            }
            if (v >= 127 || (v < 32 && !is_space(v) && v != '\n')) {
                sprintf(p, "<\\x%02x>", v);
                break;
            }
    addv:
            *p++ = v;
            *p = '\0';
        } else if (v < neocc_token_next(s1)) {
            return neocc_token_at(s1, (v - TOK_IDENT) + TOK_IDENT)->str;
        } else if (v >= SYM_FIRST_ANOM) {
            /* special name for anonymous symbol */
            sprintf(p, "L.%u", v - SYM_FIRST_ANOM);
        } else {
            /* should never happen */
            return NULL;
        }
        break;
    }
    return s1->pp->spelling.data;
}

/* return the current character, handling end of block if necessary
   (but not stray) */
static int handle_eob(TCCState *s1)
{
    struct neocc_source_buffer *bf = s1->pp->source;
    int len;

    /* only tries to read if really end of buffer */
    if (bf->buf_ptr >= bf->buf_end) {
        if (bf->fd >= 0) {
#if defined(PARSE_DEBUG)
            len = 1;
#else
            len = IO_BUF_SIZE;
#endif
            len = read(bf->fd, bf->buffer, len);
            if (len < 0)
                len = 0;
        } else {
            len = 0;
        }
        s1->total_bytes += len;
        bf->buf_ptr = bf->buffer;
        bf->buf_end = bf->buffer + len;
        *bf->buf_end = CH_EOB;
    }
    if (bf->buf_ptr < bf->buf_end) {
        return bf->buf_ptr[0];
    } else {
        bf->buf_ptr = bf->buf_end;
        return CH_EOF;
    }
}

/* read next char from current input file and handle end of input buffer */
static int next_c(TCCState *s1)
{
    int ch = *++s1->pp->source->buf_ptr;
    /* end of buffer/file handling */
    if (ch == CH_EOB && s1->pp->source->buf_ptr >= s1->pp->source->buf_end)
        ch = handle_eob(s1);
    return ch;
}

/* input with '\[\r]\n' handling. */
static int handle_stray_noerror(TCCState *s1, int err)
{
    int ch;
    while ((ch = next_c(s1)) == '\\') {
        ch = next_c(s1);
        if (ch == '\n') {
    newl:
            s1->pp->source->line_num++;
        } else {
            if (ch == '\r') {
                ch = next_c(s1);
                if (ch == '\n')
                    goto newl;
                *--s1->pp->source->buf_ptr = '\r';
            }
            if (err)
                _tcc_error(s1, "stray '\\' in program");
            /* may take advantage of 'struct neocc_source_buffer.unget[4}' */
            return *--s1->pp->source->buf_ptr = '\\';
        }
    }
    return ch;
}

#define ninp(s1) handle_stray_noerror(s1, 0)

/* handle '\\' in strings, comments and skipped regions */
static int handle_bs(TCCState *s1, uint8_t **p)
{
    int c;
    s1->pp->source->buf_ptr = *p - 1;
    c = ninp(s1);
    *p = s1->pp->source->buf_ptr;
    return c;
}

/* skip the stray and handle the \\n case. Output an error if
   incorrect char after the stray */
static int handle_stray(TCCState *s1, uint8_t **p)
{
    int c;
    s1->pp->source->buf_ptr = *p - 1;
    c = handle_stray_noerror(s1, !(s1->pp->parse_flags & PARSE_FLAG_ACCEPT_STRAYS));
    *p = s1->pp->source->buf_ptr;
    return c;
}

/* handle the complicated stray case */
#define PEEKC(s1, c, p)\
{\
    c = *++p;\
    if (c == '\\')\
        c = handle_stray(s1, &p); \
}

static int skip_spaces(TCCState *s1)
{
    int ch;
    --s1->pp->source->buf_ptr;
    do {
        ch = ninp(s1);
    } while (s1->pp->character[ch - CH_EOF] & NEOCC_CHAR_SPACE);
    return ch;
}

/* single line C++ comments */
static uint8_t *parse_line_comment(TCCState *s1, uint8_t *p)
{
    int c;
    for(;;) {
        for (;;) {
            c = *++p;
    redo:
            if (c == '\n' || c == '\\')
                break;
            c = *++p;
            if (c == '\n' || c == '\\')
                break;
        }
        if (c == '\n')
            break;
        c = handle_bs(s1, &p);
        if (c == CH_EOF)
            break;
        if (c != '\\')
            goto redo;
    }
    return p;
}

/* C comments */
static uint8_t *parse_comment(TCCState *s1, uint8_t *p)
{
    int c;
    for(;;) {
        /* fast skip loop */
        for(;;) {
            c = *++p;
        redo:
            if (c == '\n' || c == '*' || c == '\\')
                break;
            c = *++p;
            if (c == '\n' || c == '*' || c == '\\')
                break;
        }
        /* now we can handle all the cases */
        if (c == '\n') {
            s1->pp->source->line_num++;
        } else if (c == '*') {
            do {
                c = *++p;
            } while (c == '*');
            if (c == '\\')
                c = handle_bs(s1, &p);
            if (c == '/')
                break;
            goto check_eof;
        } else {
            c = handle_bs(s1, &p);
        check_eof:
            if (c == CH_EOF)
                _tcc_error(s1, "unexpected end of file in comment");
            if (c != '\\')
                goto redo;
        }
    }
    return p + 1;
}

/* parse a string without interpreting escapes */
static uint8_t *parse_pp_string(TCCState *s1, uint8_t *p, int sep, struct neocc_string *str)
{
    int c;
    for(;;) {
        c = *++p;
    redo:
        if (c == sep) {
            break;
        } else if (c == '\\') {
            c = handle_bs(s1, &p);
            if (c == CH_EOF) {
        unterminated_string:
                /* XXX: indicate line number of start of string */
                s1->pp->token_flags &= ~TOK_FLAG_BOL;
                _tcc_error(s1, "missing terminating %c character", sep);
            } else if (c == '\\') {
                if (str)
                    cstr_ccat(s1, str, c);
                c = *++p;
                /* add char after '\\' unconditionally */
                if (c == '\\') {
                    c = handle_bs(s1, &p);
                    if (c == CH_EOF)
                        goto unterminated_string;
                }
                goto add_char;
            } else {
                goto redo;
            }
        } else if (c == '\n') {
        add_lf:
            if (ACCEPT_LF_IN_STRINGS) {
                s1->pp->source->line_num++;
                goto add_char;
            } else if (str) { /* not skipping */
                goto unterminated_string;
            } else {
                //tcc_warning("missing terminating %c character", sep);
                return p;
            }
        } else if (c == '\r') {
            c = *++p;
            if (c == '\\')
                c = handle_bs(s1, &p);
            if (c == '\n')
                goto add_lf;
            if (c == CH_EOF)
                goto unterminated_string;
            if (str)
                cstr_ccat(s1, str, '\r');
            goto redo;
        } else {
        add_char:
            if (str)
                cstr_ccat(s1, str, c);
        }
    }
    p++;
    return p;
}

/* skip block of text until #else, #elif or #endif. skip also pairs of
   #if/#endif */
static void preprocess_skip(TCCState *s1)
{
    int a, start_of_line, c, in_warn_or_error;
    uint8_t *p;

    p = s1->pp->source->buf_ptr;
    a = 0;
redo_start:
    start_of_line = 1;
    in_warn_or_error = 0;
    for(;;) {
        c = *p;
        switch(c) {
        case ' ':
        case '\t':
        case '\f':
        case '\v':
        case '\r':
            p++;
            continue;
        case '\n':
            s1->pp->source->line_num++;
            p++;
            goto redo_start;
        case '\\':
            c = handle_bs(s1, &p);
            if (c == CH_EOF)
                expect(s1, "#endif");
            if (c == '\\')
                ++p;
            continue;
        /* skip strings */
        case '\"':
        case '\'':
            if (in_warn_or_error)
                goto _default;
            s1->pp->token_flags &= ~TOK_FLAG_BOL;
            p = parse_pp_string(s1, p, c, NULL);
            break;
        /* skip comments */
        case '/':
            if (in_warn_or_error)
                goto _default;
            ++p;
            c = handle_bs(s1, &p);
            if (c == '*') {
                p = parse_comment(s1, p);
            } else if (c == '/') {
                p = parse_line_comment(s1, p);
            }
            continue;
        case '#':
            p++;
            if (start_of_line) {
                s1->pp->source->buf_ptr = p;
                next_nomacro(s1);
                p = s1->pp->source->buf_ptr;
                if (a == 0 && 
                    (s1->pp->token == TOK_ELSE || s1->pp->token == TOK_ELIF || s1->pp->token == TOK_ENDIF))
                    goto the_end;
                if (s1->pp->token == TOK_IF || s1->pp->token == TOK_IFDEF || s1->pp->token == TOK_IFNDEF)
                    a++;
                else if (s1->pp->token == TOK_ENDIF)
                    a--;
                else if( s1->pp->token == TOK_ERROR || s1->pp->token == TOK_WARNING)
                    in_warn_or_error = 1;
                else if (s1->pp->token == TOK_LINEFEED)
                    goto redo_start;
                else if (s1->pp->parse_flags & PARSE_FLAG_ASM_FILE)
                    p = parse_line_comment(s1, p - 1);
            }
#if !defined(TCC_TARGET_ARM) && !defined(TCC_TARGET_ARM64)
            else if (s1->pp->parse_flags & PARSE_FLAG_ASM_FILE)
                p = parse_line_comment(s1, p - 1);
#else
            /* ARM/ARM64 assembly uses '#' for constants */
#endif
            break;
_default:
        default:
            p++;
            break;
        }
        start_of_line = 0;
    }
 the_end: ;
    s1->pp->source->buf_ptr = p;
}

#if 0
/* return the number of additional 'ints' necessary to store the
   token */
static inline int tok_size(const int *p)
{
    switch(*p) {
        /* 4 bytes */
    case TOK_CINT:
    case TOK_CUINT:
    case TOK_CCHAR:
    case TOK_LCHAR:
    case TOK_CFLOAT:
    case TOK_LINENUM:
        return 1 + 1;
    case TOK_STR:
    case TOK_LSTR:
    case TOK_PPNUM:
    case TOK_PPSTR:
        return 1 + 1 + (p[1] + 3) / 4;
    case TOK_CLONG:
    case TOK_CULONG:
	return 1 + LONG_SIZE / 4;
    case TOK_CDOUBLE:
    case TOK_CLLONG:
    case TOK_CULLONG:
        return 1 + 2;
    case TOK_CLDOUBLE:
        return 1 + NEOCC_LDOUBLE_WORDS;
    default:
        return 1 + 0;
    }
}
#endif

/* token string handling */
ST_INLN void tok_str_new(struct neocc_token_string *s)
{
    s->str = NULL;
    s->len = s->need_spc = 0;
    s->allocated_len = 0;
    s->last_line_num = -1;
}

ST_FUNC struct neocc_token_string *tok_str_alloc(TCCState *s1)
{
    struct neocc_token_string *str = neocc_token_resize(s1, 0, sizeof *str);
    tok_str_new(str);
    return str;
}

ST_FUNC void tok_str_free_str(TCCState *s1, int *str)
{
    neocc_token_resize(s1, str, 0);
}

ST_FUNC void tok_str_free(TCCState *s1, struct neocc_token_string *str)
{
    tok_str_free_str(s1, str->str);
    neocc_token_resize(s1, str, 0);
}

ST_FUNC int *tok_str_realloc(TCCState *s1, struct neocc_token_string *s, int new_size)
{
    int *str, size;

    size = s->allocated_len;
    if (size < 16)
        size = 16;
    while (size < new_size)
        size = size * 2;
    if (size > s->allocated_len) {
        str = neocc_token_resize(s1, s->str, size * sizeof(int));
        s->allocated_len = size;
        s->str = str;
    }
    return s->str;
}

ST_FUNC void tok_str_add(TCCState *s1, struct neocc_token_string *s, int t)
{
    int len, *str;

    len = s->len;
    str = s->str;
    if (len >= s->allocated_len)
        str = tok_str_realloc(s1, s, len + 1);
    str[len++] = t;
    s->len = len;
}

ST_FUNC void begin_macro(TCCState *s1, struct neocc_token_string *str, int alloc)
{
	struct neocc_macro_push request = { str, alloc, s1->pp->source->line_num };
	int result = vo_interact2(s1->preprocessor,
		NEOCC_AC2_PREPROCESSOR_PUSH, (unsigned char *)&request,
		sizeof(request), &neocc_host_caller);

	if (!RP(result))
		_tcc_error(s1, "cannot enter macro stream");
}

ST_FUNC void end_macro(TCCState *s1)
{
	struct neocc_macro_pop output;
	unsigned int quantity;
	int result = vo_interact1(s1->preprocessor,
		NEOCC_AC1_PREPROCESSOR_POP, (unsigned char *)&output,
		sizeof(output), &quantity, &neocc_host_caller);

	if (!RP(result) || quantity != sizeof(output))
		_tcc_error(s1, "cannot leave macro stream");
	s1->pp->source->line_num = output.saved_line;
	if (output.release)
		tok_str_free(s1, output.release);
}

static void tok_str_add2(TCCState *s1, struct neocc_token_string *s,
                        int t, union neocc_value *cv)
{
    int len, *str;

    len = s->len;
    str = s->str;

    /* allocate space for worst case */
    if (len + TOK_MAX_SIZE >= s->allocated_len)
        str = tok_str_realloc(s1, s, len + TOK_MAX_SIZE + 1);
    str[len++] = t;
    switch(t) {
    case TOK_CINT:
    case TOK_CUINT:
    case TOK_CCHAR:
    case TOK_LCHAR:
    case TOK_CFLOAT:
    case TOK_LINENUM:
#if LONG_SIZE == 4
    case TOK_CLONG:
    case TOK_CULONG:
#endif
        str[len++] = cv->tab[0];
        break;
    case TOK_PPNUM:
    case TOK_PPSTR:
    case TOK_STR:
    case TOK_LSTR:
        {
            /* Insert the string into the int array. */
            size_t nb_words =
                1 + (cv->str.size + sizeof(int) - 1) / sizeof(int);
            if (len + nb_words >= s->allocated_len)
                str = tok_str_realloc(s1, s, len + nb_words + 1);
            str[len] = cv->str.size;
            memcpy(&str[len + 1], cv->str.data, cv->str.size);
            len += nb_words;
        }
        break;
    case TOK_CDOUBLE:
    case TOK_CLLONG:
    case TOK_CULLONG:
#if LONG_SIZE == 8
    case TOK_CLONG:
    case TOK_CULONG:
#endif
        str[len++] = cv->tab[0];
        str[len++] = cv->tab[1];
        break;
    case TOK_CLDOUBLE:
        str[len++] = cv->tab[0];
        str[len++] = cv->tab[1];
        if (NEOCC_LDOUBLE_WORDS >= 3)
        str[len++] = cv->tab[2];
        if (NEOCC_LDOUBLE_WORDS >= 4)
        str[len++] = cv->tab[3];
    default:
        break;
    }
    s->len = len;
}

/* add the current parse token in token string 's' */
ST_FUNC void tok_str_add_tok(TCCState *s1, struct neocc_token_string *s)
{
    union neocc_value cval;

    /* save line number info */
    if (s1->pp->source->line_num != s->last_line_num) {
        s->last_line_num = s1->pp->source->line_num;
        cval.i = s->last_line_num;
        tok_str_add2(s1, s, TOK_LINENUM, &cval);
    }
    tok_str_add2(s1, s, s1->pp->token, &s1->pp->value);
}

/* like tok_str_add2(), add a space if needed */
static void tok_str_add2_spc(TCCState *s1, struct neocc_token_string *s,
                            int t, union neocc_value *cv)
{
    if (s->need_spc == 3)
        tok_str_add(s1, s, ' ');
    s->need_spc = 2;
    tok_str_add2(s1, s, t, cv);
}

/* get a token from an integer array and increment pointer. */
static inline void tok_get(int *t, const int **pp, union neocc_value *cv)
{
    const int *p = *pp;
    int n, *tab;

    tab = cv->tab;
    switch(*t = *p++) {
#if LONG_SIZE == 4
    case TOK_CLONG:
#endif
    case TOK_CINT:
    case TOK_CCHAR:
    case TOK_LCHAR:
    case TOK_LINENUM:
        cv->i = *p++;
        break;
#if LONG_SIZE == 4
    case TOK_CULONG:
#endif
    case TOK_CUINT:
        cv->i = (unsigned)*p++;
        break;
    case TOK_CFLOAT:
	tab[0] = *p++;
	break;
    case TOK_STR:
    case TOK_LSTR:
    case TOK_PPNUM:
    case TOK_PPSTR:
        cv->str.size = *p++;
        cv->str.data = (char*)p;
        p += (cv->str.size + sizeof(int) - 1) / sizeof(int);
        break;
    case TOK_CDOUBLE:
    case TOK_CLLONG:
    case TOK_CULLONG:
#if LONG_SIZE == 8
    case TOK_CLONG:
    case TOK_CULONG:
#endif
        n = 2;
        goto copy;
    case TOK_CLDOUBLE:
        n = NEOCC_LDOUBLE_WORDS;
    copy:
        do
            *tab++ = *p++;
        while (--n);
        break;
    default:
        break;
    }
    *pp = p;
}

#if 0
# define TOK_GET(t,p,c) tok_get(t,p,c)
#else
# define TOK_GET(t,p,c) do { \
    int _t = **(p); \
    if (TOK_HAS_VALUE(_t)) \
        tok_get(t, p, c); \
    else \
        *(t) = _t, ++*(p); \
    } while (0)
#endif

static int macro_is_equal(TCCState *s1, const int *a, const int *b)
{
    union neocc_value cv;
    int t;

    if (!a || !b)
        return 1;

    while (*a && *b) {
        cstr_reset(&s1->pp->parsed);
        TOK_GET(&t, &a, &cv);
        cstr_cat(s1, &s1->pp->parsed, get_tok_str(s1, t, &cv), 0);
        TOK_GET(&t, &b, &cv);
        if (strcmp(s1->pp->parsed.data, get_tok_str(s1, t, &cv)))
            return 0;
    }
    return !(*a || *b);
}

/* defines handling */
ST_INLN void define_push(TCCState *s1, int v, int macro_type, int *str, struct neocc_symbol *first_arg)
{
    struct neocc_symbol *s, *o;

    o = define_find(s1, v);
    s = sym_stack_push(s1, NEOCC_SYMBOL_STACK_DEFINE, v, macro_type, 0);
    s->d = str;
    s->next = first_arg;
    neocc_token_at(s1, (v - TOK_IDENT) + TOK_IDENT)->sym_define = s;

    if (o && !macro_is_equal(s1, o->d, s->d))
	_tcc_warning(s1, "%s redefined", get_tok_str(s1, v, NULL));
}

/* undefined a define symbol. Its name is just set to zero */
ST_FUNC void define_undef(TCCState *s1, struct neocc_symbol *s)
{
    int v = s->v;
    if (v >= TOK_IDENT && v < neocc_token_next(s1))
        neocc_token_at(s1, (v - TOK_IDENT) + TOK_IDENT)->sym_define = NULL;
}

ST_INLN struct neocc_symbol *define_find(TCCState *s1, int v)
{
    v -= TOK_IDENT;
    if ((unsigned)v >= (unsigned)(neocc_token_next(s1) - TOK_IDENT))
        return NULL;
    return neocc_token_at(s1, (v) + TOK_IDENT)->sym_define;
}

/* free define stack until top reaches 'b' */
ST_FUNC void free_defines(TCCState *s1, struct neocc_symbol *b)
{
    struct neocc_symbol *top;

    while ((top = sym_stack_top(s1, NEOCC_SYMBOL_STACK_DEFINE)) != b) {
        tok_str_free_str(s1, top->d);
        define_undef(s1, top);
        sym_stack_drop(s1, NEOCC_SYMBOL_STACK_DEFINE, top);
    }
}

/* fake the nth "#if defined test_..." for tcc -dt -run */
static void maybe_run_test(TCCState *s1)
{
    const char *p;
    if (s1->pp->include_stack_ptr != s1->pp->include_stack)
        return;
    p = get_tok_str(s1, s1->pp->token, NULL);
    if (0 != memcmp(p, "test_", 5))
        return;
    if (0 != --s1->run_test)
        return;
    fprintf(s1->ppfp, &"\n[%s]\n"[!(s1->dflag & 32)], p), fflush(s1->ppfp);
    define_push(s1, s1->pp->token, MACRO_OBJ, NULL, NULL);
}

ST_FUNC void skip_to_eol(TCCState *s1, int warn)
{
    if (s1->pp->token == TOK_LINEFEED)
        return;
    if (warn)
        _tcc_warning(s1, "extra tokens after directive");
    while (s1->pp->macro_stack)
        end_macro(s1);
    s1->pp->source->buf_ptr = parse_line_comment(s1, s1->pp->source->buf_ptr - 1);
    next_nomacro(s1);
}

static struct neocc_cached_include *
search_cached_include(TCCState *s1, const char *filename, int add);

static int parse_include(TCCState *s1, int do_next, int test)
{
    int c, i;
    char name[1024], buf[1024], *p;
    struct neocc_cached_include *e;

    c = skip_spaces(s1);
    if (c == '<' || c == '\"') {
        cstr_reset(&s1->pp->parsed);
        s1->pp->source->buf_ptr = parse_pp_string(s1, s1->pp->source->buf_ptr, c == '<' ? '>' : c, &s1->pp->parsed);
        i = s1->pp->parsed.size;
        pstrncpy(name, sizeof name, s1->pp->parsed.data, i);
        next_nomacro(s1);
    } else {
        /* computed #include : concatenate tokens until result is one of
           the two accepted forms.  Don't convert pp-tokens to tokens here. */
	s1->pp->parse_flags = PARSE_FLAG_PREPROCESS
                    | PARSE_FLAG_LINEFEED
                    | (s1->pp->parse_flags & PARSE_FLAG_ASM_FILE);
        name[0] = 0;
        for (;;) {
            next(s1);
            p = name, i = strlen(p) - 1;
            if (i > 0
                && ((p[0] == '"' && p[i] == '"')
                 || (p[0] == '<' && p[i] == '>')))
                break;
            if (s1->pp->token == TOK_LINEFEED)
                _tcc_error(s1, "'#include' expects \"FILENAME\" or <FILENAME>");
            pstrcat(name, sizeof name, get_tok_str(s1, s1->pp->token, &s1->pp->value));
	}
        c = p[0];
        /* remove '<>|""' */
        memmove(p, p + 1, i - 1), p[i - 1] = 0;
    }

    if (!test)
        skip_to_eol(s1, 1);

    i = do_next ? s1->pp->source->include_next_index : -1;
    for (;;) {
        ++i;
        if (i == 0) {
            /* check absolute include path */
            if (!IS_ABSPATH(name))
                continue;
            buf[0] = '\0';
        } else if (i == 1) {
            /* search in file's dir if "header.h" */
            if (c != '\"')
                continue;
            p = s1->pp->source->true_filename;
            pstrncpy(buf, sizeof buf, p, tcc_basename(p) - p);
        } else {
            int j = i - 2, k = j - s1->nb_include_paths;
            if (k < 0)
                p = s1->include_paths[j];
            else if (k < s1->nb_sysinclude_paths)
                p = s1->sysinclude_paths[k];
            else if (test)
                return 0;
            else
                _tcc_error(s1, "include file '%s' not found", name);
            pstrcpy(buf, sizeof buf, p);
            pstrcat(buf, sizeof buf, "/");
        }
        pstrcat(buf, sizeof buf, name);
        e = search_cached_include(s1, buf, 0);
        if (e && (define_find(s1, e->ifndef_macro) || e->once)) {
            /* no need to parse the include because the 'ifndef macro'
               is defined (or had #pragma once) */
#ifdef INC_DEBUG
            printf("%s: skipping cached %s\n", s1->pp->source->filename, buf);
#endif
            if ((s1->verbose | 1) == 3) /* -vv[v] */
                printf("=> %*s%s\n",
                   (int)(s1->pp->include_stack_ptr - s1->pp->include_stack), "", buf);
            return 1;
        }
        if (tcc_open(s1, buf) >= 0)
            break;
    }

    if (test) {
        tcc_close(s1);
    } else {
        if (s1->pp->include_stack_ptr >= s1->pp->include_stack +
            NEOCC_INCLUDE_STACK_CAPACITY)
            _tcc_error(s1, "#include recursion too deep");
        /* push previous file on stack */
        *s1->pp->include_stack_ptr++ = s1->pp->source->prev;
        s1->pp->source->include_next_index = i;
#ifdef INC_DEBUG
        printf("%s: including %s\n", s1->pp->source->prev->filename, s1->pp->source->filename);
#endif
        /* update target deps */
        if (s1->gen_deps) {
            struct neocc_source_buffer *bf = s1->pp->source;
            while (i == 1 && (bf = bf->prev))
                i = bf->include_next_index;
            /* skip system include files */
            if ((s1->include_sys_deps || i - 2 < s1->nb_include_paths) &&
                !RP(neocc_preprocessor_add(s1,
                    NEOCC_AC2_PREPROCESSOR_DEPENDENCY, buf)))
                _tcc_error(s1, "cannot record dependency '%s'", buf);
        }
        /* add include file debug info */
        tcc_debug_bincl(s1);
    }
    return 1;
}

/* eval an expression for #if/#elif */
static int expr_preprocess(TCCState *s1)
{
    int c, t;
    int t0 = s1->pp->token;
    struct neocc_token_string *str;
    
    str = tok_str_alloc(s1);
    s1->pp->expression = 1;
    while (1) {
        next(s1); /* do macro subst */
        t = s1->pp->token;
        if (s1->pp->token < TOK_IDENT) {
            if (s1->pp->token == TOK_LINEFEED || s1->pp->token == TOK_EOF)
                break;
            if (s1->pp->token >= TOK_STR && s1->pp->token <= TOK_CLDOUBLE)
                _tcc_error(s1, "invalid constant in preprocessor expression");

        } else if (s1->pp->token == TOK_DEFINED) {
            s1->pp->parse_flags &= ~PARSE_FLAG_PREPROCESS; /* no macro subst */
            next(s1);
            t = s1->pp->token;
            if (t == '(') 
                next(s1);
            s1->pp->parse_flags |= PARSE_FLAG_PREPROCESS;
            if (s1->pp->token < TOK_IDENT)
                expect(s1, "identifier after 'defined'");
            if (s1->run_test)
                maybe_run_test(s1);
            c = 0;
            if (define_find(s1, s1->pp->token)
                || s1->pp->token == TOK___HAS_INCLUDE
                || s1->pp->token == TOK___HAS_INCLUDE_NEXT)
                c = 1;
            if (t == '(') {
                next(s1);
                if (s1->pp->token != ')')
                    expect(s1, "')'");
            }
            goto c_number;
        } else if (s1->pp->token == TOK___HAS_INCLUDE ||
                   s1->pp->token == TOK___HAS_INCLUDE_NEXT) {
            t = s1->pp->token;
            next(s1);
	    if (s1->pp->token != '(')
		expect(s1, "'('");
            c = parse_include(s1, t - TOK___HAS_INCLUDE, 1);
            if (s1->pp->token != ')')
                expect(s1, "')'");
            goto c_number;
        } else {
            /* if undefined macro, replace with zero */
            c = 0;
        c_number:
            s1->pp->token = TOK_CLLONG; /* type intmax_t */
            s1->pp->value.i = c;
        }
        tok_str_add_tok(s1, str);
    }
    if (0 == str->len)
        _tcc_error(s1, "#%s with no expression", get_tok_str(s1, t0, 0));
    tok_str_add(s1, str, TOK_EOF); /* simulate end of file */
    s1->pp->expression = t0; /* redirect pre-processor expression error messages */
    t = s1->pp->token;
    /* now evaluate C constant expression */
    begin_macro(s1, str, 1);
    next(s1);
    c = expr_const(s1);
    if (s1->pp->token != TOK_EOF)
        _tcc_error(s1, "...");
    s1->pp->expression = 0;
    end_macro(s1);
    s1->pp->token = t; /* restore LF or EOF */
    return c != 0;
}

ST_FUNC int pp_error(TCCState *s1, struct neocc_string *cs)
{
    int expression = s1->pp->expression;
    int success;

    /* A failure while expanding the diagnostic must not recurse here. */
    s1->pp->expression = 0;
    success = neocc_string_printf(cs, "bad preprocessor expression: #%s",
        get_tok_str(s1, expression, 0)) >= 0;
    s1->pp->macro_ptr = s1->pp->macro_stack->str;
    while (success && (next(s1), s1->pp->token != TOK_EOF))
        success = neocc_string_printf(cs, " %s",
            get_tok_str(s1, s1->pp->token, &s1->pp->value)) >= 0;
    return success;
}

/* parse after #define */
ST_FUNC void parse_define(TCCState *s1)
{
    struct neocc_symbol *s, *first, **ps;
    int v, t, varg, is_vaargs, t0;
    int saved_parse_flags = s1->pp->parse_flags;
    struct neocc_token_string str;

    v = s1->pp->token;
    if (v < TOK_IDENT || v == TOK_DEFINED)
        _tcc_error(s1, "invalid macro name '%s'", get_tok_str(s1, s1->pp->token, &s1->pp->value));
    first = NULL;
    t = MACRO_OBJ;
    /* We have to parse the whole define as if not in asm mode, in particular
       no line comment with '#' must be ignored.  Also for function
       macros the argument list must be parsed without '.' being an ID
       character.  */
    s1->pp->parse_flags = ((s1->pp->parse_flags & ~PARSE_FLAG_ASM_FILE) | PARSE_FLAG_SPACES);
    /* '(' must be just after macro definition for MACRO_FUNC */
    next_nomacro(s1);
    s1->pp->parse_flags &= ~PARSE_FLAG_SPACES;
    is_vaargs = 0;
    if (s1->pp->token == '(') {
        int dotid = set_idnum(s1, '.', 0);
        next_nomacro(s1);
        ps = &first;
        if (s1->pp->token != ')') for (;;) {
            varg = s1->pp->token;
            next_nomacro(s1);
            is_vaargs = 0;
            if (varg == TOK_DOTS) {
                varg = TOK___VA_ARGS__;
                is_vaargs = 1;
            } else if (s1->pp->token == TOK_DOTS && s1->gnu_ext) {
                is_vaargs = 1;
                next_nomacro(s1);
            }
            if (varg < TOK_IDENT)
        bad_list:
                _tcc_error(s1, "bad macro parameter list");
            s = sym_stack_push(s1, NEOCC_SYMBOL_STACK_DEFINE,
                               varg | SYM_FIELD, is_vaargs, 0);
            *ps = s;
            ps = &s->next;
            if (s1->pp->token == ')')
                break;
            if (s1->pp->token != ',' || is_vaargs)
                goto bad_list;
            next_nomacro(s1);
        }
        s1->pp->parse_flags |= PARSE_FLAG_SPACES;
        next_nomacro(s1);
        t = MACRO_FUNC;
        set_idnum(s1, '.', dotid);
    }

    /* The body of a macro definition should be parsed such that identifiers
       are parsed like the file mode determines (i.e. with '.' being an
       ID character in asm mode).  But '#' should be retained instead of
       regarded as line comment leader, so still don't set ASM_FILE
       in s1->pp->parse_flags. */
    s1->pp->parse_flags |= PARSE_FLAG_ACCEPT_STRAYS | PARSE_FLAG_SPACES | PARSE_FLAG_LINEFEED;
    tok_str_new(&str);
    t0 = 0;
    while (s1->pp->token != TOK_LINEFEED && s1->pp->token != TOK_EOF) {
        if (is_space(s1->pp->token)) {
            str.need_spc |= 1;
        } else {
            if (TOK_TWOSHARPS == s1->pp->token) {
                if (0 == t0)
                    goto bad_twosharp;
                s1->pp->token = TOK_PPJOIN;
                t |= MACRO_JOIN;
            }
            tok_str_add2_spc(s1, &str, s1->pp->token, &s1->pp->value);
            t0 = s1->pp->token;
        }
        next_nomacro(s1);
    }
    s1->pp->parse_flags = saved_parse_flags;
    tok_str_add(s1, &str, 0);
    if (t0 == TOK_PPJOIN)
bad_twosharp:
        _tcc_error(s1, "'##' cannot appear at either end of macro");
    define_push(s1, v, t, str.str, first);
    //tok_print(str.str, "#define (%d) %s %d:", t | is_vaargs * 4, get_tok_str(v, 0));
}

static struct neocc_cached_include *search_cached_include(TCCState *s1,
    const char *filename, int add)
{
    struct neocc_include_request request = { filename, add };
    struct neocc_cached_include *entry;
    unsigned int quantity;

    if (!RP(vo_interact3(s1->preprocessor, NEOCC_AC3_PREPROCESSOR_INCLUDE,
            (unsigned char *)&request, sizeof(request),
            (unsigned char *)&entry, sizeof(entry), &quantity,
            &neocc_host_caller)) || quantity != sizeof(entry))
        _tcc_error(s1, "cannot cache include '%s'", filename);
    return entry;
}

static int pragma_parse(TCCState *s1)
{
    next_nomacro(s1);
    if (s1->pp->token == TOK_push_macro || s1->pp->token == TOK_pop_macro) {
        int t = s1->pp->token, v;
        struct neocc_symbol *s;

        if (next(s1), s1->pp->token != '(')
            goto pragma_err;
        if (next(s1), s1->pp->token != TOK_STR)
            goto pragma_err;
        v = tok_alloc(s1, s1->pp->value.str.data, s1->pp->value.str.size - 1)->tok;
        if (next(s1), s1->pp->token != ')')
            goto pragma_err;
        if (t == TOK_push_macro) {
            while (NULL == (s = define_find(s1, v)))
                define_push(s1, v, 0, NULL, NULL);
            s->type.ref = s; /* set push boundary */
        } else {
            for (s = sym_stack_top(s1, NEOCC_SYMBOL_STACK_DEFINE); s;
                 s = s->prev)
                if (s->v == v && s->type.ref == s) {
                    s->type.ref = NULL;
                    break;
                }
        }
        if (s)
            neocc_token_at(s1, (v - TOK_IDENT) + TOK_IDENT)->sym_define = s->d ? s : NULL;
        else
            _tcc_warning(s1, "unbalanced #pragma pop_macro");
        s1->pp->debug_token = t, s1->pp->debug_symbol = v;

    } else if (s1->pp->token == TOK_once) {
        search_cached_include(s1, s1->pp->source->true_filename, 1)->once = 1;

    } else if (s1->output_type == TCC_OUTPUT_PREPROCESS) {
        /* tcc -E: keep pragmas below unchanged */
        unget_tok(s1, ' ');
        unget_tok(s1, TOK_PRAGMA);
        unget_tok(s1, '#');
        unget_tok(s1, TOK_LINEFEED);
        return 1;

    } else if (s1->pp->token == TOK_pack) {
        /* This may be:
           #pragma pack(1) // set
           #pragma pack() // reset to default
           #pragma pack(push) // push current
           #pragma pack(push,1) // push & set
           #pragma pack(pop) // restore previous */
        next(s1);
        skip(s1, '(');
        if (s1->pp->token == TOK_ASM_pop) {
            next(s1);
            if (s1->pp->pack_stack_ptr <= s1->pp->pack_stack) {
            stk_error:
                _tcc_error(s1, "out of pack stack");
            }
            s1->pp->pack_stack_ptr--;
        } else {
            int val = 0;
            if (s1->pp->token != ')') {
                if (s1->pp->token == TOK_ASM_push) {
                    next(s1);
                    if (s1->pp->pack_stack_ptr >= s1->pp->pack_stack +
                        NEOCC_PACK_STACK_CAPACITY - 1)
                        goto stk_error;
                    val = *s1->pp->pack_stack_ptr++;
                    if (s1->pp->token != ',')
                        goto pack_set;
                    next(s1);
                }
                if (s1->pp->token != TOK_CINT)
                    goto pragma_err;
                val = s1->pp->value.i;
                if (val < 1 || val > 16 || (val & (val - 1)) != 0)
                    goto pragma_err;
                next(s1);
            }
        pack_set:
            *s1->pp->pack_stack_ptr = val;
        }
        if (s1->pp->token != ')')
            goto pragma_err;

    } else if (s1->pp->token == TOK_comment) {
        struct neocc_string *text;
        int t;
        next(s1);
        skip(s1, '(');
        t = s1->pp->token;
        next(s1);
        skip(s1, ',');
        if (s1->pp->token != TOK_STR)
            goto pragma_err;
        text = neocc_temp_new(s1);
        cstr_cat(s1, text, s1->pp->value.str.data, 0);
        next(s1);
        if (s1->pp->token != ')')
            goto pragma_err;
        if (t == TOK_lib) {
            if (!RP(neocc_preprocessor_add(s1,
                    NEOCC_AC2_PREPROCESSOR_LIBRARY, text->data)))
                _tcc_error(s1, "cannot record pragma library");
        } else {
            if (t == TOK_option)
                tcc_set_options(s1, text->data);
        }
        neocc_temp_free(s1, text);

    } else {
        tcc_warning_c(s1, warn_all, "#pragma %s ignored", get_tok_str(s1, s1->pp->token, &s1->pp->value));
        return 0;
    }
    next(s1);
    return 1;
pragma_err:
    _tcc_error(s1, "malformed #pragma directive");
}

/* put alternative filename */
ST_FUNC void tccpp_putfile(TCCState *s1, const char *filename)
{
    char buf[1024];
    buf[0] = 0;
    if (!IS_ABSPATH(filename)) {
        /* prepend directory from real file */
        pstrcpy(buf, sizeof buf, s1->pp->source->true_filename);
        *tcc_basename(buf) = 0;
    }
    pstrcat(buf, sizeof buf, filename);
#ifdef _WIN32
    normalize_slashes(buf);
#endif
    if (0 == strcmp(s1->pp->source->filename, buf))
        return;
    //printf("new file '%s'\n", buf);
    if (!RP(vo_interact0(s1->sources, NEOCC_AC0_SOURCE_SAVE_NAME,
            &neocc_host_caller)))
        _tcc_error(s1, "cannot preserve source filename");
    pstrcpy(s1->pp->source->filename, sizeof s1->pp->source->filename, buf);
    tcc_debug_newfile(s1);
}

/* is_bof is true if first non space token at beginning of file */
ST_FUNC void preprocess(TCCState *s1, int is_bof)
{
    int c, n, saved_parse_flags;
    char buf[1024], *q;
    struct neocc_symbol *s;

    saved_parse_flags = s1->pp->parse_flags;
    s1->pp->parse_flags = PARSE_FLAG_PREPROCESS
        | PARSE_FLAG_TOK_NUM
        | PARSE_FLAG_TOK_STR
        | PARSE_FLAG_LINEFEED
        | (s1->pp->parse_flags & PARSE_FLAG_ASM_FILE)
        ;

    next_nomacro(s1);
 redo:
    switch(s1->pp->token) {
    case TOK_DEFINE:
        s1->pp->debug_token = s1->pp->token;
        next_nomacro(s1);
        s1->pp->debug_symbol = s1->pp->token;
        parse_define(s1);
        break;
    case TOK_UNDEF:
        s1->pp->debug_token = s1->pp->token;
        next_nomacro(s1);
        s1->pp->debug_symbol = s1->pp->token;
        s = define_find(s1, s1->pp->token);
        /* undefine symbol by putting an invalid name */
        if (s)
            define_undef(s1, s);
        next_nomacro(s1);
        break;
    case TOK_INCLUDE:
    case TOK_INCLUDE_NEXT:
        parse_include(s1, s1->pp->token - TOK_INCLUDE, 0);
        goto the_end;
    case TOK_IFNDEF:
        c = 1;
        goto do_ifdef;
    case TOK_IF:
        c = expr_preprocess(s1);
        goto do_if;
    case TOK_IFDEF:
        c = 0;
    do_ifdef:
        next_nomacro(s1);
        if (s1->pp->token < TOK_IDENT)
            _tcc_error(s1, "invalid argument for '#if%sdef'", c ? "n" : "");
        if (is_bof) {
            if (c) {
#ifdef INC_DEBUG
                printf("#ifndef %s\n", get_tok_str(s1, s1->pp->token, NULL));
#endif
                s1->pp->source->ifndef_macro = s1->pp->token;
            }
        }
        if (define_find(s1, s1->pp->token)
            || s1->pp->token == TOK___HAS_INCLUDE
            || s1->pp->token == TOK___HAS_INCLUDE_NEXT)
            c ^= 1;
        next_nomacro(s1);
    do_if:
        if (s1->pp->ifdef_stack_ptr >= s1->pp->ifdef_stack +
            NEOCC_IFDEF_STACK_CAPACITY)
            _tcc_error(s1, "memory full (ifdef)");
        *s1->pp->ifdef_stack_ptr++ = c;
        goto test_skip;
    case TOK_ELSE:
        next_nomacro(s1);
        if (s1->pp->ifdef_stack_ptr == s1->pp->ifdef_stack)
            _tcc_error(s1, "#else without matching #if");
        if (s1->pp->ifdef_stack_ptr[-1] & 2)
            _tcc_error(s1, "#else after #else");
        c = (s1->pp->ifdef_stack_ptr[-1] ^= 3);
        goto test_else;
    case TOK_ELIF:
        if (s1->pp->ifdef_stack_ptr == s1->pp->ifdef_stack)
            _tcc_error(s1, "#elif without matching #if");
        c = s1->pp->ifdef_stack_ptr[-1];
        if (c > 1)
            _tcc_error(s1, "#elif after #else");
        /* last #if/#elif expression was true: we skip */
        if (c == 1) {
            skip_to_eol(s1, 0);
            c = 0;
        } else {
            c = expr_preprocess(s1);
            s1->pp->ifdef_stack_ptr[-1] = c;
        }
    test_else:
        if (s1->pp->ifdef_stack_ptr == s1->pp->source->ifdef_stack_ptr + 1)
            s1->pp->source->ifndef_macro = 0;
    test_skip:
        if (!(c & 1)) {
            skip_to_eol(s1, 1);
            preprocess_skip(s1);
            is_bof = 0;
            goto redo;
        }
        break;
    case TOK_ENDIF:
        next_nomacro(s1);
        if (s1->pp->ifdef_stack_ptr <= s1->pp->source->ifdef_stack_ptr)
            _tcc_error(s1, "#endif without matching #if");
        s1->pp->ifdef_stack_ptr--;
        /* '#ifndef macro' was at the start of file. Now we check if
           an '#endif' is exactly at the end of file */
        if (s1->pp->source->ifndef_macro &&
            s1->pp->ifdef_stack_ptr == s1->pp->source->ifdef_stack_ptr) {
            s1->pp->source->ifndef_macro_saved = s1->pp->source->ifndef_macro;
            /* need to set to zero to avoid false matches if another
               #ifndef at middle of file */
            s1->pp->source->ifndef_macro = 0;
            s1->pp->token_flags |= TOK_FLAG_ENDIF;
        }
        break;

    case TOK_LINE:
        s1->pp->parse_flags &= ~PARSE_FLAG_TOK_NUM;
        next(s1);
        if (s1->pp->token != TOK_PPNUM) {
    _line_err:
            _tcc_error(s1, "wrong #line format");
        }
        c = 1;
        goto _line_num;
    case TOK_PPNUM:
        if (s1->pp->parse_flags & PARSE_FLAG_ASM_FILE)
            goto ignore;
        c = 0; /* no error with extra tokens */
    _line_num:
        for (n = 0, q = s1->pp->value.str.data; *q; ++q) {
            if (!isnum(*q))
                goto _line_err;
            n = n * 10 + *q - '0';
        }
        s1->pp->parse_flags &= ~PARSE_FLAG_TOK_STR; /* don't parse escape sequences */
        next(s1);
        if (s1->pp->token != TOK_LINEFEED) {
            if (s1->pp->token != TOK_PPSTR || s1->pp->value.str.data[0] != '"')
                goto _line_err;
            s1->pp->value.str.data[s1->pp->value.str.size - 2] = 0;
            tccpp_putfile(s1, s1->pp->value.str.data + 1);
            next(s1);
            /* skip optional level number & advance to next line */
            skip_to_eol(s1, c);
        }
        if (s1->pp->source->fd > 0)
            s1->total_lines += s1->pp->source->line_num - n;
        s1->pp->source->line_num = n;
        break;

    case TOK_ERROR:
    case TOK_WARNING:
    {
        q = buf;
        c = skip_spaces(s1);
        while (c != '\n' && c != CH_EOF) {
            if ((q - buf) < sizeof(buf) - 1)
                *q++ = c;
            c = ninp(s1);
        }
        *q = '\0';
        if (s1->pp->token == TOK_ERROR)
            _tcc_error(s1, "#error %s", buf);
        else
            _tcc_warning(s1, "#warning %s", buf);
        next_nomacro(s1);
        break;
    }
    case TOK_PRAGMA:
        if (!pragma_parse(s1))
            goto ignore;
        break;
    case TOK_LINEFEED:
        goto the_end;
    default:
        /* ignore gas line comment in an 'S' file. */
        if (saved_parse_flags & PARSE_FLAG_ASM_FILE)
            goto ignore;
        if (s1->pp->token == '!' && is_bof)
            /* '#!' is ignored at beginning to allow C scripts. */
            goto ignore;
        _tcc_warning(s1, "ignoring unknown preprocessing directive #%s", get_tok_str(s1, s1->pp->token, &s1->pp->value));
    ignore:
        skip_to_eol(s1, 0);
        goto the_end;
    }
    skip_to_eol(s1, 1);
 the_end:
    s1->pp->parse_flags = saved_parse_flags;
}

/* evaluate escape codes in a string. */
static void parse_escape_string(TCCState *s1, struct neocc_string *outstr, const uint8_t *buf, int is_long)
{
    int c, n, i;
    const uint8_t *p;

    p = buf;
    for(;;) {
        c = *p;
        if (c == '\0')
            break;
        if (c == '\\') {
            p++;
            /* escape */
            c = *p;
            switch(c) {
            case '0': case '1': case '2': case '3':
            case '4': case '5': case '6': case '7':
                /* at most three octal digits */
                n = c - '0';
                p++;
                c = *p;
                if (isoct(c)) {
                    n = n * 8 + c - '0';
                    p++;
                    c = *p;
                    if (isoct(c)) {
                        n = n * 8 + c - '0';
                        p++;
                    }
                }
                c = n;
                goto add_char_nonext;
            case 'x': i = 0; goto parse_hex_or_ucn;
            case 'u': i = 4; goto parse_hex_or_ucn;
            case 'U': i = 8; goto parse_hex_or_ucn;
    parse_hex_or_ucn:
                p++;
                n = 0;
                do {
                    c = *p;
                    if (c >= 'a' && c <= 'f')
                        c = c - 'a' + 10;
                    else if (c >= 'A' && c <= 'F')
                        c = c - 'A' + 10;
                    else if (isnum(c))
                        c = c - '0';
                    else if (i >= 0)
                        expect(s1, "more hex digits in universal-character-name");
                    else
                        goto add_hex_or_ucn;
                    n = (unsigned) n * 16 + c;
                    p++;
                } while (--i);
		if (is_long) {
    add_hex_or_ucn:
                    c = n;
		    goto add_char_nonext;
		}
                cstr_u8cat(s1, outstr, n);
                continue;
            case 'a':
                c = '\a';
                break;
            case 'b':
                c = '\b';
                break;
            case 'f':
                c = '\f';
                break;
            case 'n':
                c = '\n';
                break;
            case 'r':
                c = '\r';
                break;
            case 't':
                c = '\t';
                break;
            case 'v':
                c = '\v';
                break;
            case 'e':
                if (!s1->gnu_ext)
                    goto invalid_escape;
                c = 27;
                break;
            case '\'':
            case '\"':
            case '\\': 
            case '?':
                break;
            default:
            invalid_escape:
                if (c >= '!' && c <= '~')
                    _tcc_warning(s1, "unknown escape sequence: \'\\%c\'", c);
                else
                    _tcc_warning(s1, "unknown escape sequence: \'\\x%x\'", c);
                break;
            }
        } else if (is_long && c >= 0x80) {
            /* assume we are processing UTF-8 sequence */
            /* reference: The Unicode Standard, Version 10.0, ch3.9 */

            int cont; /* count of continuation bytes */
            int skip; /* how many bytes should skip when error occurred */
            int i;

            /* decode leading byte */
            if (c < 0xC2) {
	            skip = 1; goto invalid_utf8_sequence;
            } else if (c <= 0xDF) {
	            cont = 1; n = c & 0x1f;
            } else if (c <= 0xEF) {
	            cont = 2; n = c & 0xf;
            } else if (c <= 0xF4) {
	            cont = 3; n = c & 0x7;
            } else {
	            skip = 1; goto invalid_utf8_sequence;
            }

            /* decode continuation bytes */
            for (i = 1; i <= cont; i++) {
                int l = 0x80, h = 0xBF;

                /* adjust limit for second byte */
                if (i == 1) {
                    switch (c) {
                    case 0xE0: l = 0xA0; break;
                    case 0xED: h = 0x9F; break;
                    case 0xF0: l = 0x90; break;
                    case 0xF4: h = 0x8F; break;
                    }
                }

                if (p[i] < l || p[i] > h) {
                    skip = i; goto invalid_utf8_sequence;
                }

                n = (n << 6) | (p[i] & 0x3f);
            }

            /* advance pointer */
            p += 1 + cont;
            c = n;
            goto add_char_nonext;

            /* error handling */
        invalid_utf8_sequence:
            _tcc_warning(s1, "ill-formed UTF-8 subsequence starting with: \'\\x%x\'", c);
            c = 0xFFFD;
            p += skip;
            goto add_char_nonext;

        }
        p++;
    add_char_nonext:
        if (!is_long)
            cstr_ccat(s1, outstr, c);
        else {
#ifdef TCC_TARGET_PE
            /* store as UTF-16 */
            if (c < 0x10000) {
                cstr_wccat(s1, outstr, c);
            } else {
                c -= 0x10000;
                cstr_wccat(s1, outstr, (c >> 10) + 0xD800);
                cstr_wccat(s1, outstr, (c & 0x3FF) + 0xDC00);
            }
#else
            cstr_wccat(s1, outstr, c);
#endif
        }
    }
    /* add a trailing '\0' */
    if (!is_long)
        cstr_ccat(s1, outstr, '\0');
    else
        cstr_wccat(s1, outstr, '\0');
}

static void parse_string(TCCState *s1, const char *s, int len)
{
    uint8_t buf[1000], *p = buf;
    struct neocc_string *scratch = NULL;
    int is_long, sep;

    if ((is_long = *s == 'L'))
        ++s, --len;
    sep = *s++;
    len -= 2;
    if (len >= sizeof buf) {
        scratch = neocc_temp_new(s1);
        cstr_cat(s1, scratch, s, len);
        cstr_ccat(s1, scratch, 0);
        p = (uint8_t *)scratch->data;
    } else {
        memcpy(p, s, len);
        p[len] = 0;
    }

    cstr_reset(&s1->pp->parsed);
    parse_escape_string(s1, &s1->pp->parsed, p, is_long);
    if (scratch)
        neocc_temp_free(s1, scratch);

    if (sep == '\'') {
        int char_size, i, n, c;
        /* XXX: make it portable */
        if (!is_long)
            s1->pp->token = TOK_CCHAR, char_size = 1;
        else
            s1->pp->token = TOK_LCHAR, char_size = sizeof(nwchar_t);
        n = s1->pp->parsed.size / char_size - 1;
        if (n < 1)
            _tcc_error(s1, "empty character constant");
        if (n > 1)
            tcc_warning_c(s1, warn_all, "multi-character character constant");
        for (c = i = 0; i < n; ++i) {
            if (is_long)
                c = ((nwchar_t *)s1->pp->parsed.data)[i];
            else
                c = (c << 8) | ((char *)s1->pp->parsed.data)[i];
        }
        s1->pp->value.i = c;
    } else {
        s1->pp->value.str.size = s1->pp->parsed.size;
        s1->pp->value.str.data = s1->pp->parsed.data;
        if (!is_long)
            s1->pp->token = TOK_STR;
        else
            s1->pp->token = TOK_LSTR;
    }
}

/* we use 128 bit (64/112 needed) numbers */
#define BN_SIZE 4

/* bn = (bn << shift) | or_val */
static int bn_lshift(unsigned int *bn, int shift, int or_val)
{
    int i;
    unsigned int v;
    if (bn[BN_SIZE - 1] >> (32 - shift))
	return shift;
    for(i=0;i<BN_SIZE;i++) {
        v = bn[i];
        bn[i] = (v << shift) | or_val;
        or_val = v >> (32 - shift);
    }
    return 0;
}

static void bn_zero(unsigned int *bn)
{
    int i;
    for(i=0;i<BN_SIZE;i++) {
        bn[i] = 0;
    }
}

/* parse number in null terminated string 'p' and return it in the
   current token */
static void parse_number(TCCState *s1, const char *p)
{
    int b, t, shift, frac_bits, s, exp_val, ch;
    char *q;
    unsigned int bn[BN_SIZE];
    long double d;

    /* number */
    q = s1->pp->number;
    ch = *p++;
    t = ch;
    ch = *p++;
    *q++ = t;
    b = 10;
    if (t == '.') {
        goto float_frac_parse;
    } else if (t == '0') {
        if (ch == 'x' || ch == 'X') {
            q--;
            ch = *p++;
            b = 16;
        } else if (s1->tcc_ext && (ch == 'b' || ch == 'B')) {
            q--;
            ch = *p++;
            b = 2;
        }
    }
    /* parse all digits. cannot check octal numbers at this stage
       because of floating point constants */
    while (1) {
        if (ch >= 'a' && ch <= 'f')
            t = ch - 'a' + 10;
        else if (ch >= 'A' && ch <= 'F')
            t = ch - 'A' + 10;
        else if (isnum(ch))
            t = ch - '0';
        else
            break;
        if (t >= b)
            break;
        if (q >= s1->pp->number + STRING_MAX_SIZE) {
        num_too_long:
            _tcc_error(s1, "number too long");
        }
        *q++ = ch;
        ch = *p++;
    }
    if (ch == '.' ||
        ((ch == 'e' || ch == 'E') && b == 10) ||
        ((ch == 'p' || ch == 'P') && (b == 16 || b == 2))) {
        if (b != 10) {
            /* NOTE: strtox should support that for hexa numbers, but
               non ISOC99 libcs do not support it, so we prefer to do
               it by hand */
            /* hexadecimal or binary floats */
            /* XXX: handle overflows */
            frac_bits = 0;
            *q = '\0';
            if (b == 16)
                shift = 4;
            else 
                shift = 1;
            bn_zero(bn);
            q = s1->pp->number;
            while (1) {
                t = *q++;
                if (t == '\0') {
                    break;
                } else if (t >= 'a') {
                    t = t - 'a' + 10;
                } else if (t >= 'A') {
                    t = t - 'A' + 10;
                } else {
                    t = t - '0';
                }
                frac_bits -= bn_lshift(bn, shift, t);
            }
            if (ch == '.') {
                ch = *p++;
                while (1) {
                    t = ch;
                    if (t >= 'a' && t <= 'f') {
                        t = t - 'a' + 10;
                    } else if (t >= 'A' && t <= 'F') {
                        t = t - 'A' + 10;
                    } else if (t >= '0' && t <= '9') {
                        t = t - '0';
                    } else {
                        break;
                    }
                    if (t >= b)
                        _tcc_error(s1, "invalid digit");
                    frac_bits -= bn_lshift(bn, shift, t);
                    frac_bits += shift;
                    ch = *p++;
                }
            }
            if (ch != 'p' && ch != 'P')
                expect(s1, "exponent");
            ch = *p++;
            s = 1;
            exp_val = 0;
            if (ch == '+') {
                ch = *p++;
            } else if (ch == '-') {
                s = -1;
                ch = *p++;
            }
            if (ch < '0' || ch > '9')
                expect(s1, "exponent digits");
            while (ch >= '0' && ch <= '9') {
		/* If exp_val is this large ldexp will return HUGE_VAL */
		if (exp_val < 100000000)
                    exp_val = exp_val * 10 + ch - '0';
                ch = *p++;
            }
            exp_val = exp_val * s;

            /* now we can generate the number */
            /* XXX: should patch directly float number */
            d = (long double)bn[3] * 79228162514264337593543950336.0L +
	        (long double)bn[2] * 18446744073709551616.0L +
	        (long double)bn[1] * 4294967296.0L +
	        (long double)bn[0];
            d = ldexpl(d, exp_val - frac_bits);
            t = toup(ch);
            if (t == 'F') {
                ch = *p++;
                s1->pp->token = TOK_CFLOAT;
                /* float : should handle overflow */
                s1->pp->value.f = (float)d;
            } else if (t == 'L') {
                ch = *p++;
                s1->pp->token = TOK_CLDOUBLE;
                s1->pp->value.ld = d;
            } else {
                s1->pp->token = TOK_CDOUBLE;
                s1->pp->value.d = (double)d;
            }
        } else {
            /* decimal floats */
            if (ch == '.') {
                if (q >= s1->pp->number + STRING_MAX_SIZE)
                    goto num_too_long;
                *q++ = ch;
                ch = *p++;
            float_frac_parse:
                while (ch >= '0' && ch <= '9') {
                    if (q >= s1->pp->number + STRING_MAX_SIZE)
                        goto num_too_long;
                    *q++ = ch;
                    ch = *p++;
                }
            }
            if (ch == 'e' || ch == 'E') {
                if (q >= s1->pp->number + STRING_MAX_SIZE)
                    goto num_too_long;
                *q++ = ch;
                ch = *p++;
                if (ch == '-' || ch == '+') {
                    if (q >= s1->pp->number + STRING_MAX_SIZE)
                        goto num_too_long;
                    *q++ = ch;
                    ch = *p++;
                }
                if (ch < '0' || ch > '9')
                    expect(s1, "exponent digits");
                while (ch >= '0' && ch <= '9') {
                    if (q >= s1->pp->number + STRING_MAX_SIZE)
                        goto num_too_long;
                    *q++ = ch;
                    ch = *p++;
                }
            }
            *q = '\0';
            t = toup(ch);
            errno = 0;
            if (t == 'F') {
                ch = *p++;
                s1->pp->token = TOK_CFLOAT;
                s1->pp->value.f = strtof(s1->pp->number, NULL);
            } else if (t == 'L') {
                ch = *p++;
                s1->pp->token = TOK_CLDOUBLE;
                s1->pp->value.ld = strtold(s1->pp->number, NULL);
            } else {
                s1->pp->token = TOK_CDOUBLE;
                s1->pp->value.d = strtod(s1->pp->number, NULL);
            }
        }
    } else {
        unsigned long long n, n1;
        int lcount, ucount, ov = 0;
        const char *p1;

        /* integer number */
        *q = '\0';
        q = s1->pp->number;
        if (b == 10 && *q == '0') {
            b = 8;
            q++;
        }
        n = 0;
        while(1) {
            t = *q++;
            /* no need for checks except for base 10 / 8 errors */
            if (t == '\0')
                break;
            else if (t >= 'a')
                t = t - 'a' + 10;
            else if (t >= 'A')
                t = t - 'A' + 10;
            else
                t = t - '0';
            if (t >= b)
                _tcc_error(s1, "invalid digit");
            n1 = n;
            n = n * b + t;
            /* detect overflow */
            if (n1 >= 0x1000000000000000ULL && n / b != n1)
                ov = 1;
        }

        /* Determine the characteristics (unsigned and/or 64bit) the type of
           the constant must have according to the constant suffix(es) */
        lcount = ucount = 0;
        p1 = p;
        for(;;) {
            t = toup(ch);
            if (t == 'L') {
                if (lcount >= 2)
                    _tcc_error(s1, "three 'l's in integer constant");
                if (lcount && *(p - 1) != ch)
                    _tcc_error(s1, "incorrect integer suffix: %s", p1);
                lcount++;
                ch = *p++;
            } else if (t == 'U') {
                if (ucount >= 1)
                    _tcc_error(s1, "two 'u's in integer constant");
                ucount++;
                ch = *p++;
            } else {
                break;
            }
        }

        /* in #if/#elif expressions, all numbers have type (u)intmax_t anyway */
        if (s1->pp->expression)
            lcount = 2;

        /* Determine if it needs 64 bits and/or unsigned in order to fit */
        if (ucount == 0 && b == 10) {
            if (lcount <= (LONG_SIZE == 4)) {
                if (n >= 0x80000000U)
                    lcount = (LONG_SIZE == 4) + 1;
            }
            if (n >= 0x8000000000000000ULL)
                ov = 1, ucount = 1;
        } else {
            if (lcount <= (LONG_SIZE == 4)) {
                if (n >= 0x100000000ULL)
                    lcount = (LONG_SIZE == 4) + 1;
                else if (n >= 0x80000000U)
                    ucount = 1;
            }
            if (n >= 0x8000000000000000ULL)
                ucount = 1;
        }

        if (ov)
            _tcc_warning(s1, "integer constant overflow");

        s1->pp->token = TOK_CINT;
	if (lcount) {
            s1->pp->token = TOK_CLONG;
            if (lcount == 2)
                s1->pp->token = TOK_CLLONG;
	}
	if (ucount)
	    ++s1->pp->token; /* TOK_CU... */
        s1->pp->value.i = n;
    }
    if (ch)
        _tcc_error(s1, "invalid number");
}

#define PARSE2(s1, c1, tok1, c2, tok2)              \
    case c1:                                    \
        PEEKC(s1, c, p);                            \
        if (c == c2) {                          \
            p++;                                \
            s1->pp->token = tok2;                         \
        } else {                                \
            s1->pp->token = tok1;                         \
        }                                       \
        break;

/* return next token without macro substitution */
static void next_nomacro(TCCState *s1)
{
    int t, c, is_long, len;
    struct neocc_token *ts;
    uint8_t *p, *p1;

    p = s1->pp->source->buf_ptr;
 redo_no_start:
    c = *p;
    switch(c) {
    case ' ':
    case '\t':
        s1->pp->token = c;
        p++;
 maybe_space:
        if (s1->pp->parse_flags & PARSE_FLAG_SPACES)
            goto keep_tok_flags;
        while (s1->pp->character[*p - CH_EOF] & NEOCC_CHAR_SPACE)
            ++p;
        goto redo_no_start;
    case '\f':
    case '\v':
    case '\r':
        p++;
        goto redo_no_start;
    case '\\':
        /* first look if it is in fact an end of buffer */
        c = handle_stray(s1, &p);
        if (c == '\\')
            goto parse_simple;
        if (c == CH_EOF) {
                    if (!(s1->pp->token_flags & TOK_FLAG_BOL)) {
                /* add implicit newline */
                goto maybe_newline;
            } else if (!(s1->pp->parse_flags & PARSE_FLAG_PREPROCESS)) {
                s1->pp->token = TOK_EOF;
            } else if (s1->pp->ifdef_stack_ptr != s1->pp->source->ifdef_stack_ptr) {
                _tcc_error(s1, "missing #endif");
            } else if (s1->pp->include_stack_ptr == s1->pp->include_stack) {
                /* no include left : end of file. */
                s1->pp->token = TOK_EOF;
            } else {
                /* pop include file */

                /* test if previous '#endif' was after a #ifdef at
                   start of file */
                if (s1->pp->token_flags & TOK_FLAG_ENDIF) {
#ifdef INC_DEBUG
                    printf("#endif %s\n", get_tok_str(s1, s1->pp->source->ifndef_macro_saved, NULL));
#endif
                    search_cached_include(s1, s1->pp->source->true_filename, 1)
                        ->ifndef_macro = s1->pp->source->ifndef_macro_saved;
                    s1->pp->token_flags &= ~TOK_FLAG_ENDIF;
                }

                /* add end of include file debug info */
                tcc_debug_eincl(s1);
                /* pop include stack */
                tcc_close(s1);
                s1->pp->include_stack_ptr--;
                p = s1->pp->source->buf_ptr;
                goto maybe_newline;
            }
        } else {
            goto redo_no_start;
        }
        break;

    case '\n':
        s1->pp->source->line_num++;
        p++;
maybe_newline:
        s1->pp->token_flags |= TOK_FLAG_BOL;
        if (0 == (s1->pp->parse_flags & PARSE_FLAG_LINEFEED))
            goto redo_no_start;
        s1->pp->token = TOK_LINEFEED;
        goto keep_tok_flags;

    case '#':
        /* XXX: simplify */
        PEEKC(s1, c, p);
        if ((s1->pp->token_flags & TOK_FLAG_BOL) &&
            (s1->pp->parse_flags & PARSE_FLAG_PREPROCESS)) {
            s1->pp->token_flags &= ~TOK_FLAG_BOL;
            s1->pp->source->buf_ptr = p;
            preprocess(s1, s1->pp->token_flags & TOK_FLAG_BOF);
            p = s1->pp->source->buf_ptr;
            goto maybe_newline;
        } else {
            if (c == '#') {
                p++;
                s1->pp->token = TOK_TWOSHARPS;
            } else {
#if !defined(TCC_TARGET_ARM) && !defined(TCC_TARGET_ARM64)
                if (s1->pp->parse_flags & PARSE_FLAG_ASM_FILE) {
                    p = parse_line_comment(s1, p - 1);
                    goto redo_no_start;
                } else
#endif
                {
                    s1->pp->token = '#';
                }
            }
        }
        break;
    
    /* dollar is allowed to start identifiers when not parsing asm */
    case '$':
        if (!(s1->pp->character['$' - CH_EOF] & NEOCC_CHAR_ID)
         || (s1->pp->parse_flags & PARSE_FLAG_ASM_FILE))
            goto parse_simple;

    case 'a': case 'b': case 'c': case 'd':
    case 'e': case 'f': case 'g': case 'h':
    case 'i': case 'j': case 'k': case 'l':
    case 'm': case 'n': case 'o': case 'p':
    case 'q': case 'r': case 's': case 't':
    case 'u': case 'v': case 'w': case 'x':
    case 'y': case 'z': 
    case 'A': case 'B': case 'C': case 'D':
    case 'E': case 'F': case 'G': case 'H':
    case 'I': case 'J': case 'K': 
    case 'M': case 'N': case 'O': case 'P':
    case 'Q': case 'R': case 'S': case 'T':
    case 'U': case 'V': case 'W': case 'X':
    case 'Y': case 'Z': 
    case '_':
    parse_ident_fast:
        p1 = p;
        while (c = *++p, s1->pp->character[c - CH_EOF] & (NEOCC_CHAR_ID|NEOCC_CHAR_NUMBER))
            ;
        len = p - p1;
        if (c != '\\') {
            ts = tok_alloc(s1, (char *)p1, len);
        } else {
            /* slower case */
            cstr_reset(&s1->pp->parsed);
            cstr_cat(s1, &s1->pp->parsed, (char *) p1, len);
            p--;
            PEEKC(s1, c, p);
            while (s1->pp->character[c - CH_EOF] & (NEOCC_CHAR_ID|NEOCC_CHAR_NUMBER))
            {
                cstr_ccat(s1, &s1->pp->parsed, c);
                PEEKC(s1, c, p);
            }
            ts = tok_alloc(s1, s1->pp->parsed.data, s1->pp->parsed.size);
        }
        s1->pp->token = ts->tok;
        break;
    case 'L':
        t = p[1];
        if (t == '\'' || t == '\"' || t == '\\') {
            PEEKC(s1, c, p);
            if (c == '\'' || c == '\"') {
                is_long = 1;
                goto str_const;
            }
            *--p = c = 'L';
        }
        goto parse_ident_fast;

    case '0': case '1': case '2': case '3':
    case '4': case '5': case '6': case '7':
    case '8': case '9':
        t = c;
        PEEKC(s1, c, p);
        /* after the first digit, accept digits, alpha, '.' or sign if
           prefixed by 'eEpP' */
    parse_num:
        cstr_reset(&s1->pp->parsed);
        for(;;) {
            cstr_ccat(s1, &s1->pp->parsed, t);
            if (!((s1->pp->character[c - CH_EOF] & (NEOCC_CHAR_ID|NEOCC_CHAR_NUMBER))
                  || c == '.'
                  || ((c == '+' || c == '-')
                      && (((t == 'e' || t == 'E')
                            && !(s1->pp->parse_flags & PARSE_FLAG_ASM_FILE
                                /* 0xe+1 is 3 tokens in asm */
                                && ((char*)s1->pp->parsed.data)[0] == '0'
                                && toup(((char*)s1->pp->parsed.data)[1]) == 'X'))
                          || t == 'p' || t == 'P'))))
                break;
            t = c;
            PEEKC(s1, c, p);
        }
        /* We add a trailing '\0' to ease parsing */
        cstr_ccat(s1, &s1->pp->parsed, '\0');
        s1->pp->value.str.size = s1->pp->parsed.size;
        s1->pp->value.str.data = s1->pp->parsed.data;
        s1->pp->token = TOK_PPNUM;
        break;

    case '.':
        /* special dot handling because it can also start a number */
        PEEKC(s1, c, p);
        if (isnum(c)) {
            t = '.';
            goto parse_num;
        } else if ((s1->pp->character['.' - CH_EOF] & NEOCC_CHAR_ID)
                   && (s1->pp->character[c - CH_EOF] & (NEOCC_CHAR_ID|NEOCC_CHAR_NUMBER))) {
            *--p = c = '.';
            goto parse_ident_fast;
        } else if (c == '.') {
            PEEKC(s1, c, p);
            if (c == '.') {
                p++;
                s1->pp->token = TOK_DOTS;
            } else {
                *--p = '.'; /* may underflow into file->unget[] */
                s1->pp->token = '.';
            }
        } else {
            s1->pp->token = '.';
        }
        break;
    case '\'':
    case '\"':
        is_long = 0;
    str_const:
        cstr_reset(&s1->pp->parsed);
        if (is_long)
            cstr_ccat(s1, &s1->pp->parsed, 'L');
        cstr_ccat(s1, &s1->pp->parsed, c);
        p = parse_pp_string(s1, p, c, &s1->pp->parsed);
        cstr_ccat(s1, &s1->pp->parsed, c);
        cstr_ccat(s1, &s1->pp->parsed, '\0');
        s1->pp->value.str.size = s1->pp->parsed.size;
        s1->pp->value.str.data = s1->pp->parsed.data;
        s1->pp->token = TOK_PPSTR;
        break;

    case '<':
        PEEKC(s1, c, p);
        if (c == '=') {
            p++;
            s1->pp->token = TOK_LE;
        } else if (c == '<') {
            PEEKC(s1, c, p);
            if (c == '=') {
                p++;
                s1->pp->token = TOK_A_SHL;
            } else {
                s1->pp->token = TOK_SHL;
            }
        } else {
            s1->pp->token = TOK_LT;
        }
        break;
    case '>':
        PEEKC(s1, c, p);
        if (c == '=') {
            p++;
            s1->pp->token = TOK_GE;
        } else if (c == '>') {
            PEEKC(s1, c, p);
            if (c == '=') {
                p++;
                s1->pp->token = TOK_A_SAR;
            } else {
                s1->pp->token = TOK_SAR;
            }
        } else {
            s1->pp->token = TOK_GT;
        }
        break;
        
    case '&':
        PEEKC(s1, c, p);
        if (c == '&') {
            p++;
            s1->pp->token = TOK_LAND;
        } else if (c == '=') {
            p++;
            s1->pp->token = TOK_A_AND;
        } else {
            s1->pp->token = '&';
        }
        break;
        
    case '|':
        PEEKC(s1, c, p);
        if (c == '|') {
            p++;
            s1->pp->token = TOK_LOR;
        } else if (c == '=') {
            p++;
            s1->pp->token = TOK_A_OR;
        } else {
            s1->pp->token = '|';
        }
        break;

    case '+':
        PEEKC(s1, c, p);
        if (c == '+') {
            p++;
            s1->pp->token = TOK_INC;
        } else if (c == '=') {
            p++;
            s1->pp->token = TOK_A_ADD;
        } else {
            s1->pp->token = '+';
        }
        break;
        
    case '-':
        PEEKC(s1, c, p);
        if (c == '-') {
            p++;
            s1->pp->token = TOK_DEC;
        } else if (c == '=') {
            p++;
            s1->pp->token = TOK_A_SUB;
        } else if (c == '>') {
            p++;
            s1->pp->token = TOK_ARROW;
        } else {
            s1->pp->token = '-';
        }
        break;

    PARSE2(s1, '!', '!', '=', TOK_NE)
    PARSE2(s1, '=', '=', '=', TOK_EQ)
    PARSE2(s1, '*', '*', '=', TOK_A_MUL)
    PARSE2(s1, '%', '%', '=', TOK_A_MOD)
    PARSE2(s1, '^', '^', '=', TOK_A_XOR)
        
        /* comments or operator */
    case '/':
        PEEKC(s1, c, p);
        if (c == '*') {
            p = parse_comment(s1, p);
            /* comments replaced by a blank */
            s1->pp->token = ' ';
            goto maybe_space;
        } else if (c == '/') {
            p = parse_line_comment(s1, p);
            s1->pp->token = ' ';
            goto maybe_space;
        } else if (c == '=') {
            p++;
            s1->pp->token = TOK_A_DIV;
        } else {
            s1->pp->token = '/';
        }
        break;
        
        /* simple tokens */
    case '@': /* only used in assembler */
#ifdef TCC_TARGET_ARM /* comment on arm asm */
        if (s1->pp->parse_flags & PARSE_FLAG_ASM_FILE) {
            p = parse_line_comment(s1, p);
            goto redo_no_start;
        }
#endif
    case '(':
    case ')':
    case '[':
    case ']':
    case '{':
    case '}':
    case ',':
    case ';':
    case ':':
    case '?':
    case '~':
    parse_simple:
        s1->pp->token = c;
        p++;
        break;
    case 0xEF: /* UTF8 BOM ? */
        if (p[1] == 0xBB && p[2] == 0xBF && p == s1->pp->source->buffer) {
            p += 3;
            goto redo_no_start;
        }
    default:
        if (c >= 0x80 && c <= 0xFF) /* utf8 identifiers */
	    goto parse_ident_fast;
        if (s1->pp->parse_flags & PARSE_FLAG_ASM_FILE)
            goto parse_simple;
        _tcc_error(s1, "unrecognized character \\x%02x", c);
        break;
    }
    s1->pp->token_flags = 0;
keep_tok_flags:
    s1->pp->source->buf_ptr = p;
#if defined(PARSE_DEBUG)
    printf("token = %d %s\n", s1->pp->token, get_tok_str(s1, s1->pp->token, &s1->pp->value));
#endif
}

#ifdef PP_DEBUG
static void define_print(TCCState *s1, int v);
static void pp_print(TCCState *s1, const char *msg, int v, const int *str)
{
    FILE *fp = s1->ppfp;

    if (msg[0] == '#' && s1->pp->debug_indent == 0)
        fprintf(fp, "\n");
    else if (msg[0] == '+')
         ++s1->pp->debug_indent, ++msg;
    else if (msg[0] == '-')
        --s1->pp->debug_indent, ++msg;

    fprintf(fp, "%*s", s1->pp->debug_indent, "");
    if (msg[0] == '#') {
        define_print(s1, v);
    } else {
        tok_print(s1, str, v ? "%s %s" : "%s", msg, get_tok_str(s1, v, 0));
    }
}
#define PP_PRINT(x) pp_print x
#else
#define PP_PRINT(x)
#endif

static int macro_subst(TCCState *s1,
    struct neocc_token_string *tok_str,
    struct neocc_symbol **nested_list,
    const int *macro_str
    );

/* substitute arguments in replacement lists in macro_str by the values in
   args (field d) and return allocated string */
static int *macro_arg_subst(TCCState *s1, struct neocc_symbol **nested_list, const int *macro_str, struct neocc_symbol *args)
{
    int t, t0, t1, t2, n;
    const int *st;
    struct neocc_symbol *s;
    union neocc_value cval;
    struct neocc_token_string str;

#ifdef PP_DEBUG
    PP_PRINT((s1, "asubst:", 0, macro_str));
    for (s = args, n = 0; s; s = s->prev, ++n);
    while (n--) {
        for (s = args, t = 0; t < n; s = s->prev, ++t);
        tok_print(s1, s->d, "%*s - arg: %s:", s1->pp->debug_indent, "", get_tok_str(s1, s->v, 0));
    }
#endif

    tok_str_new(&str);
    t0 = t1 = 0;
    while(1) {
        TOK_GET(&t, &macro_str, &cval);
        if (!t)
            break;
        if (t == '#') {
            /* stringize */
            do t = *macro_str++; while (t == ' ');
            s = sym_find2(args, t);
            if (s) {
                cstr_reset(&s1->pp->parsed);
                cstr_ccat(s1, &s1->pp->parsed, '\"');
                st = s->d;
                while (*st != TOK_EOF) {
                    const char *s;
                    TOK_GET(&t, &st, &cval);
                    s = get_tok_str(s1, t, &cval);
                    while (*s) {
                        if (t == TOK_PPSTR && *s != '\'')
                            add_char(s1, &s1->pp->parsed, *s);
                        else
                            cstr_ccat(s1, &s1->pp->parsed, *s);
                        ++s;
                    }
                }
                cstr_ccat(s1, &s1->pp->parsed, '\"');
                cstr_ccat(s1, &s1->pp->parsed, '\0');
                //printf("\nstringize: <%s>\n", (char *)s1->pp->parsed.data);
                /* add string */
                cval.str.size = s1->pp->parsed.size;
                cval.str.data = s1->pp->parsed.data;
                tok_str_add2(s1, &str, TOK_PPSTR, &cval);
#ifdef TCC_TARGET_ARM
            } else if ((s1->pp->parse_flags & PARSE_FLAG_ASM_FILE) && t == TOK_PPNUM) {
                /* for example: mov r1,#0 */
                --macro_str, tok_str_add(s1, &str, '#');
#endif
            } else {
                expect(s1, "macro parameter after '#'");
            }
        } else if (t >= TOK_IDENT) {
            s = sym_find2(args, t);
            if (s) {
                st = s->d;
                n = 0;
                while ((t2 = macro_str[n]) == ' ')
                    ++n;
                /* if '##' is present before or after, no arg substitution */
                if (t2 == TOK_PPJOIN || t1 == TOK_PPJOIN) {
                    /* special case for var arg macros : ## eats the ','
                       if empty VA_ARGS variable. */
                    if (t1 == TOK_PPJOIN && t0 == ',' && s1->gnu_ext && s->type.t) {
                        int c = str.str[str.len - 1];
                        while (str.str[--str.len] != ',')
                            ;
                        if (*st == TOK_EOF) {
                            /* suppress ',' '##' */
                        } else {
                            /* suppress '##' and add variable */
                            str.len++;
                            if (c == ' ')
                                str.str[str.len++] = c;
                            goto add_var;
                        }
                    } else {
                        if (*st == TOK_EOF)
                            tok_str_add(s1, &str, TOK_PLCHLDR);
                    }
                } else {
            add_var:
		    if (!s->e) {
			/* Expand arguments tokens and store them.  In most
			   cases we could also re-expand each argument if
			   used multiple times, but not if the argument
			   contains the __COUNTER__ macro.  */
			struct neocc_token_string str2;
			tok_str_new(&str2);
			macro_subst(s1, &str2, nested_list, st);
			tok_str_add(s1, &str2, TOK_EOF);
			s->e = str2.str;
		    }
		    st = s->e;
                }
                while (*st != TOK_EOF) {
                    TOK_GET(&t2, &st, &cval);
                    tok_str_add2(s1, &str, t2, &cval);
                }
            } else {
                tok_str_add(s1, &str, t);
            }
        } else {
            tok_str_add2(s1, &str, t, &cval);
        }
        if (t != ' ')
            t0 = t1, t1 = t;
    }
    tok_str_add(s1, &str, 0);
    PP_PRINT((s1, "areslt:", 0, str.str));
    return str.str;
}

/* handle the '##' operator. return the resulting string (which must be freed). */
static inline int *macro_twosharps(TCCState *s1, const int *ptr0)
{
    int t1, t2, n, l;
    union neocc_value cv1, cv2;
    struct neocc_token_string macro_str1;
    const int *ptr;

    tok_str_new(&macro_str1);
    cstr_reset(&s1->pp->parsed);
    for (ptr = ptr0;;) {
        TOK_GET(&t1, &ptr, &cv1);
        if (t1 == 0)
            break;
        for (;;) {
            n = 0;
            while ((t2 = ptr[n]) == ' ')
                ++n;
            if (t2 != TOK_PPJOIN)
                break;
            ptr += n;
            while ((t2 = *++ptr) == ' ' || t2 == TOK_PPJOIN)
                ;
            TOK_GET(&t2, &ptr, &cv2);
            if (t2 == TOK_PLCHLDR)
                continue;
            if (t1 != TOK_PLCHLDR) {
                cstr_cat(s1, &s1->pp->parsed, get_tok_str(s1, t1, &cv1), -1);
                t1 = TOK_PLCHLDR;
            }
            cstr_cat(s1, &s1->pp->parsed, get_tok_str(s1, t2, &cv2), -1);
        }
        if (s1->pp->parsed.size) {
            cstr_ccat(s1, &s1->pp->parsed, 0);
            tcc_open_bf(s1, ":paste:", s1->pp->parsed.size);
            memcpy(s1->pp->source->buffer, s1->pp->parsed.data, s1->pp->parsed.size);
            s1->pp->token_flags = 0; /* don't interpret '#' */
            for (n = 0;;n = l) {
                next_nomacro(s1);
                tok_str_add2(s1, &macro_str1, s1->pp->token, &s1->pp->value);
                if (*s1->pp->source->buf_ptr == 0)
                    break;
                tok_str_add(s1, &macro_str1, ' ');
                l = s1->pp->source->buf_ptr - s1->pp->source->buffer;
                _tcc_warning(s1, "pasting \"%.*s\" and \"%s\" does not give a valid"
                    " preprocessing token", l - n, s1->pp->source->buffer + n, s1->pp->source->buf_ptr);
            }
            tcc_close(s1);
            cstr_reset(&s1->pp->parsed);
        }
        if (t1 != TOK_PLCHLDR)
            tok_str_add2(s1, &macro_str1, t1, &cv1);
    }
    tok_str_add(s1, &macro_str1, 0);
    PP_PRINT((s1, "pasted:", 0, macro_str1.str));
    return macro_str1.str;
}

static int peek_file (TCCState *s1, struct neocc_token_string *ws_str)
{
    uint8_t *p = s1->pp->source->buf_ptr - 1;
    int c;
    for (;;) {
        PEEKC(s1, c, p);
        switch (c) {
        case '/':
            PEEKC(s1, c, p);
            if (c == '*')
                p = parse_comment(s1, p);
            else if (c == '/')
                p = parse_line_comment(s1, p);
            else {
                c = *--p = '/';
                goto leave;
            }
            --p, c = ' ';
            break;
        case ' ': case '\t':
            break;
        case '\f': case '\v': case '\r':
            continue;
        case '\n':
            s1->pp->source->line_num++, s1->pp->token_flags |= TOK_FLAG_BOL;
            break;
        default: leave:
            s1->pp->source->buf_ptr = p;
            return c;
        }
        if (ws_str)
            tok_str_add(s1, ws_str, c);
    }
}

/* peek or read [ws_str == NULL] next token from function macro call,
   walking up macro levels up to the file if necessary */
static int next_argstream(TCCState *s1, struct neocc_symbol **nested_list, struct neocc_token_string *ws_str)
{
    int t;
    struct neocc_symbol *sa;

    while (s1->pp->macro_ptr) {
        const int *m = s1->pp->macro_ptr;
        while ((t = *m) != 0) {
            if (ws_str) {
                if (t != ' ')
                    return t;
                ++m;
            } else {
                TOK_GET(&s1->pp->token, &s1->pp->macro_ptr, &s1->pp->value);
                return s1->pp->token;
            }
        }
        end_macro(s1);
        /* also, end of scope for nested defined symbol */
        sa = *nested_list;
        if (sa)
            *nested_list = sa->prev, sym_free(s1, sa);
    }
    if (ws_str) {
        return peek_file(s1, ws_str);
    } else {
        next_nomacro(s1);
        if (s1->pp->token == '\t' || s1->pp->token == TOK_LINEFEED)
            s1->pp->token = ' ';
        return s1->pp->token;
    }
}

/* do macro substitution of current token with macro 's' and add
   result to (tok_str,tok_len). 'nested_list' is the list of all
   macros we got inside to avoid recursing. Return non zero if no
   substitution needs to be done */
static int macro_subst_tok(TCCState *s1,
    struct neocc_token_string *tok_str,
    struct neocc_symbol **nested_list,
    struct neocc_symbol *s)
{
    int t;
    int v = s->v;

    PP_PRINT((s1, "#", v, s->d));
    if (s->d) {
        int *mstr = s->d;
        int *jstr;
        struct neocc_symbol *sa;
        int ret;

        if (s->type.t & MACRO_FUNC) {
            int saved_parse_flags = s1->pp->parse_flags;
            struct neocc_token_string str;
            int parlevel, i;
            struct neocc_symbol *sa1, *args;

            s1->pp->parse_flags |= PARSE_FLAG_SPACES | PARSE_FLAG_LINEFEED
                | PARSE_FLAG_ACCEPT_STRAYS;

            tok_str_new(&str);
            /* peek next token from argument stream */
            t = next_argstream(s1, nested_list, &str);
            if (t != '(') {
                /* not a macro substitution after all, restore the
                 * macro token plus all whitespace we've read.
                 * whitespace is intentionally not merged to preserve
                 * newlines. */
                s1->pp->parse_flags = saved_parse_flags;
                tok_str_add2_spc(s1, tok_str, v, 0);
                if (s1->pp->parse_flags & PARSE_FLAG_SPACES)
                    for (i = 0; i < str.len; i++)
                        tok_str_add(s1, tok_str, str.str[i]);
                tok_str_free_str(s1, str.str);
                return 0;
            } else {
                tok_str_free_str(s1, str.str);
            }

            /* argument macro */
            args = NULL;
            sa = s->next;
            /* NOTE: empty args are allowed, except if no args */
            i = 2; /* eat '(' */
            for(;;) {
                do {
                    t = next_argstream(s1, nested_list, NULL);
                } while (t == ' ' || --i);

                if (!sa) {
                    if (t == ')') /* handle '()' case */
                        break;
                    _tcc_error(s1, "macro '%s' used with too many args",
                        get_tok_str(s1, v, 0));
                }
            empty_arg:
                tok_str_new(&str);
                parlevel = 0;
                /* NOTE: non zero sa->type.t indicates VA_ARGS */
                while (parlevel > 0
                        || (t != ')' && (t != ',' || sa->type.t))) {
                    if (t == TOK_EOF)
                        _tcc_error(s1, "EOF in invocation of macro '%s'",
                            get_tok_str(s1, v, 0));
                    if (t == '(')
                        parlevel++;
                    if (t == ')')
                        parlevel--;
                    if (t == ' ')
                        str.need_spc |= 1;
                    else
                        tok_str_add2_spc(s1, &str, t, &s1->pp->value);
                    t = next_argstream(s1, nested_list, NULL);
                }
                tok_str_add(s1, &str, TOK_EOF);
                sa1 = sym_push2(s1, &args, sa->v & ~SYM_FIELD, sa->type.t, 0);
                sa1->d = str.str;
                sa = sa->next;
                if (t == ')') {
                    if (!sa)
                        break;
                    /* special case for gcc var args: add an empty
                       var arg argument if it is omitted */
                    if (sa->type.t && s1->gnu_ext)
                        goto empty_arg;
                    _tcc_error(s1, "macro '%s' used with too few args",
                        get_tok_str(s1, v, 0));
                }
                i = 1;
            }

            /* now subst each arg */
            mstr = macro_arg_subst(s1, nested_list, mstr, args);
            /* free memory */
            sa = args;
            while (sa) {
                sa1 = sa->prev;
                tok_str_free_str(s1, sa->d);
                tok_str_free_str(s1, sa->e);
                sym_free(s1, sa);
                sa = sa1;
            }
            s1->pp->parse_flags = saved_parse_flags;
        }

        /* process '##'s (if present) */
        jstr = mstr;
        if (s->type.t & MACRO_JOIN)
            jstr = macro_twosharps(s1, mstr);

        sa = sym_push2(s1, nested_list, v, 0, 0);
        ret = macro_subst(s1, tok_str, nested_list, jstr);
        /* pop nested defined symbol */
        if (sa == *nested_list)
            *nested_list = sa->prev, sym_free(s1, sa);

        if (jstr != mstr)
            tok_str_free_str(s1, jstr);
        if (mstr != s->d)
            tok_str_free_str(s1, mstr);
        return ret;

    } else {
        union neocc_value cval;
        char buf[32], *cstrval = buf;

        /* special macros */
        if (v == TOK___LINE__ || v == TOK___COUNTER__) {
            t = v == TOK___LINE__ ? s1->pp->source->line_num : s1->pp->counter++;
            snprintf(buf, sizeof(buf), "%d", t);
            t = TOK_PPNUM;
            goto add_cstr1;

        } else if (v == TOK___FILE__) {
            cstrval = s1->pp->source->filename;
            goto add_cstr;

        } else if (v == TOK___DATE__ || v == TOK___TIME__) {
            time_t ti;
            struct tm *tm;
            time(&ti);
            tm = localtime(&ti);
            if (v == TOK___DATE__) {
                static char const ab_month_name[12][4] = {
                    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
                };
                snprintf(buf, sizeof(buf), "%s %2d %d",
                    ab_month_name[tm->tm_mon], tm->tm_mday, tm->tm_year + 1900);
            } else {
                snprintf(buf, sizeof(buf), "%02d:%02d:%02d",
                    tm->tm_hour, tm->tm_min, tm->tm_sec);
            }
        add_cstr:
            t = TOK_STR;
        add_cstr1:
            cval.str.size = strlen(cstrval) + 1;
            cval.str.data = cstrval;
            tok_str_add2_spc(s1, tok_str, t, &cval);
        }
        return 0;
    }
}

/* do macro substitution of macro_str and add result to
   (tok_str,tok_len). 'nested_list' is the list of all macros we got
   inside to avoid recursing. */
static int macro_subst(TCCState *s1,
    struct neocc_token_string *tok_str,
    struct neocc_symbol **nested_list,
    const int *macro_str
    )
{
    struct neocc_symbol *s;
    int t, nosubst = 0;
    union neocc_value cval;
    struct neocc_token_string *str;

#ifdef PP_DEBUG
    int tlen = tok_str->len;
    PP_PRINT((s1, "+expand:", 0, macro_str));
#endif

    while (1) {
        TOK_GET(&t, &macro_str, &cval);
        if (t == 0 || t == TOK_EOF)
            break;
        if (t >= TOK_IDENT) {
            s = define_find(s1, t);
            if (s == NULL || nosubst)
                goto no_subst;
            /* if nested substitution, do nothing */
            if (sym_find2(*nested_list, t)) {
                /* and mark so it doesn't get subst'd again */
                t |= SYM_FIELD;
                goto no_subst;
            }
            str = tok_str_alloc(s1);
            str->str = (int*)macro_str; /* setup stream for possible arguments */
            begin_macro(s1, str, 2);
            nosubst = macro_subst_tok(s1, tok_str, nested_list, s);
            if (s1->pp->macro_stack != str) {
                /* already finished by reading function macro arguments */
                break;
            }
            macro_str = s1->pp->macro_ptr;
            end_macro (s1);
        } else if (t == ' ') {
            if (s1->pp->parse_flags & PARSE_FLAG_SPACES)
                tok_str->need_spc |= 1;
        } else {
    no_subst:
            tok_str_add2_spc(s1, tok_str, t, &cval);
            if (nosubst && t != '(')
                nosubst = 0;
            /* GCC supports 'defined' as result of a macro substitution */
            if (t == TOK_DEFINED && s1->pp->expression)
                nosubst = 1;
        }
    }

#ifdef PP_DEBUG
    tok_str_add(s1, tok_str, 0), --tok_str->len;
    PP_PRINT((s1, "-result:", 0, tok_str->str + tlen));
#endif
    return nosubst;
}

/* return next token with macro substitution */
ST_FUNC void next(TCCState *s1)
{
    int t;
    while (s1->pp->macro_ptr) {
redo:
        t = *s1->pp->macro_ptr;
        if (TOK_HAS_VALUE(t)) {
            tok_get(&s1->pp->token, &s1->pp->macro_ptr, &s1->pp->value);
            if (t == TOK_LINENUM) {
                s1->pp->source->line_num = s1->pp->value.i;
                goto redo;
            }
            goto convert;
        } else if (t == 0) {
            /* end of macro or unget token string */
            end_macro(s1);
            continue;
        } else if (t == TOK_EOF) {
            /* do nothing */
        } else {
            ++s1->pp->macro_ptr;
            t &= ~SYM_FIELD; /* remove 'nosubst' marker */
            if (t == '\\') {
                if (!(s1->pp->parse_flags & PARSE_FLAG_ACCEPT_STRAYS))
                    _tcc_error(s1, "stray '\\' in program");
            }
        }
        s1->pp->token = t;
        return;
    }

    next_nomacro(s1);
    t = s1->pp->token;
    if (t >= TOK_IDENT && (s1->pp->parse_flags & PARSE_FLAG_PREPROCESS)) {
        /* if reading from file, try to substitute macros */
        struct neocc_symbol *s = define_find(s1, t);
        if (s) {
            struct neocc_symbol *nested_list = NULL;
            macro_subst_tok(s1, &s1->pp->expansion, &nested_list, s);
            tok_str_add(s1, &s1->pp->expansion, 0);
            begin_macro(s1, &s1->pp->expansion, 0);
            goto redo;
        }
        return;
    }

convert:
    /* convert preprocessor tokens into C tokens */
    if (t == TOK_PPNUM) {
        if  (s1->pp->parse_flags & PARSE_FLAG_TOK_NUM)
            parse_number(s1, s1->pp->value.str.data);
    } else if (t == TOK_PPSTR) {
        if (s1->pp->parse_flags & PARSE_FLAG_TOK_STR)
            parse_string(s1, s1->pp->value.str.data, s1->pp->value.str.size - 1);
    }
}

/* push back current token and set current token to 'last_tok'. Only
   identifier case handled for labels. */
ST_INLN void unget_tok(TCCState *s1, int last_tok)
{
    struct neocc_token_string *str = &s1->pp->unget;
    int alloc = 0;
    if (str->len) /* use static buffer except if already in use */
        str = tok_str_alloc(s1), alloc = 1;
    if (s1->pp->token != TOK_EOF)
        tok_str_add2(s1, str, s1->pp->token, &s1->pp->value);
    tok_str_add(s1, str, 0);
    begin_macro(s1, str, alloc);
    s1->pp->token = last_tok;
}

/* ------------------------------------------------------------------------- */
/* init preprocessor */

static const char * const target_os_defs =
#ifdef TCC_TARGET_PE
    "_WIN32\0"
# if PTR_SIZE == 8
    "_WIN64\0"
# endif
#else
# if defined TCC_TARGET_MACHO
    "__APPLE__\0"
# elif TARGETOS_FreeBSD
    "__FreeBSD__ 12\0"
# elif TARGETOS_FreeBSD_kernel
    "__FreeBSD_kernel__\0"
# elif TARGETOS_NetBSD
    "__NetBSD__\0"
# elif TARGETOS_OpenBSD
    "__OpenBSD__\0"
# else
    "__linux__\0"
    "__linux\0"
#  if TARGETOS_ANDROID
    "__ANDROID__\0"
#  endif
# endif
    "__unix__\0"
    "__unix\0"
#endif
    ;

static void putdef(TCCState *s1, struct neocc_string *cs, const char *p)
{
    cstr_printf(s1, cs, "#define %s%s\n", p, &" 1"[!!strchr(p, ' ')*2]);
}

static void putdefs(TCCState *s1, struct neocc_string *cs, const char *p)
{
    while (*p)
        putdef(s1, cs, p), p = strchr(p, 0) + 1;
}

static void tcc_predefs(TCCState *s1, struct neocc_string *cs, int is_asm)
{
    cstr_printf(s1, cs, "#define __TINYC__ 9%.2s\n", &TCC_VERSION[4]);
    putdefs(s1, cs, target_machine_defs);
    putdefs(s1, cs, target_os_defs);

#ifdef TCC_TARGET_ARM
    if (s1->float_abi == ARM_HARD_FLOAT)
      putdef(s1, cs, "__ARM_PCS_VFP");
#endif
    if (is_asm)
      putdef(s1, cs, "__ASSEMBLER__");
    if (s1->output_type == TCC_OUTPUT_PREPROCESS)
      putdef(s1, cs, "__TCC_PP__");
    if (s1->output_type == TCC_OUTPUT_MEMORY)
      putdef(s1, cs, "__TCC_RUN__");
#ifdef CONFIG_TCC_BACKTRACE
    if (s1->do_backtrace)
      putdef(s1, cs, "__TCC_BACKTRACE__");
#endif
#ifdef CONFIG_TCC_BCHECK
    if (s1->do_bounds_check)
      putdef(s1, cs, "__TCC_BCHECK__");
#endif
    if (s1->char_is_unsigned)
      putdef(s1, cs, "__CHAR_UNSIGNED__");
    if (s1->optimize > 0)
      putdef(s1, cs, "__OPTIMIZE__");
    if (s1->option_pthread)
      putdef(s1, cs, "_REENTRANT");
    if (s1->leading_underscore)
      putdef(s1, cs, "__leading_underscore");
    cstr_printf(s1, cs, "#define __SIZEOF_POINTER__ %d\n", PTR_SIZE);
    cstr_printf(s1, cs, "#define __SIZEOF_LONG__ %d\n", LONG_SIZE);
    if (!is_asm) {
      putdef(s1, cs, "__STDC__");
      cstr_printf(s1, cs, "#define __STDC_HOSTED__ %d\n", s1->nostdlib ? 0 : 1);
      cstr_printf(s1, cs, "#define __STDC_VERSION__ %dL\n", s1->cversion);
      cstr_cat(s1, cs,
        /* load more predefs and __builtins */
#if CONFIG_TCC_PREDEFS
        #include "tccdefs_.h" /* include as strings */
#else
        "#include <tccdefs.h>\n" /* load at runtime */
#endif
        , -1);
    }
    cstr_printf(s1, cs, "#define __BASE_FILE__ \"%s\"\n", s1->pp->source->filename);
}

ST_FUNC void preprocess_start(TCCState *s1, int filetype)
{
    int is_asm = !!(filetype & (AFF_TYPE_ASM|AFF_TYPE_ASMPP));

    s1->pp->source->ifdef_stack_ptr = s1->pp->ifdef_stack_ptr;
    s1->pp->expression = 0;
    s1->pp->counter = 0;
    s1->pp->debug_token = s1->pp->debug_symbol = 0;

    set_idnum(s1, '$', s1->dollars_in_identifiers ? NEOCC_CHAR_ID : 0);
    set_idnum(s1, '.', is_asm ? NEOCC_CHAR_ID : 0);

    if (!(filetype & AFF_TYPE_ASM)) {
        struct neocc_string *cstr = &s1->pp->predefs;
        cstr_reset(cstr);
        tcc_predefs(s1, cstr, is_asm);
        if (s1->cmdline_defs.size)
          cstr_cat(s1, cstr, s1->cmdline_defs.data, s1->cmdline_defs.size);
        if (s1->cmdline_incl.size)
          cstr_cat(s1, cstr, s1->cmdline_incl.data, s1->cmdline_incl.size);
        *s1->pp->include_stack_ptr++ = s1->pp->source;
        tcc_open_bf(s1, "<command line>", cstr->size);
        memcpy(s1->pp->source->buffer, cstr->data, cstr->size);
        cstr_reset(cstr);
    }
    s1->pp->parse_flags = is_asm ? PARSE_FLAG_ASM_FILE : 0;
}

/* cleanup from error/setjmp */
ST_FUNC void preprocess_end(TCCState *s1)
{
    /* tccgen_finish already detached streams before inline/symbol cleanup. */
    while (s1->pp->source)
        tcc_close(s1);
    tccpp_delete(s1);
}

ST_FUNC int set_idnum(TCCState *s1, int c, int val)
{
    int prev = s1->pp->character[c - CH_EOF];
    s1->pp->character[c - CH_EOF] = val;
    return prev;
}

ST_FUNC void tccpp_new(TCCState *s1)
{
    int c;
    const char *p, *r;

    cstr_realloc(s1, &s1->pp->spelling, STRING_MAX_SIZE);
    tok_str_realloc(s1, &s1->pp->unget, TOKSTR_MAX_SIZE);
    tok_str_realloc(s1, &s1->pp->expansion, TOKSTR_MAX_SIZE);

    p = tcc_keywords;
    while (*p) {
        r = p;
        for(;;) {
            c = *r++;
            if (c == '\0')
                break;
        }
        tok_alloc(s1, p, r - p - 1);
        p = r;
    }

    /* we add dummy defines for some special macros to speed up tests
       and to have working defined() */
    define_push(s1, TOK___LINE__, MACRO_OBJ, NULL, NULL);
    define_push(s1, TOK___FILE__, MACRO_OBJ, NULL, NULL);
    define_push(s1, TOK___DATE__, MACRO_OBJ, NULL, NULL);
    define_push(s1, TOK___TIME__, MACRO_OBJ, NULL, NULL);
    define_push(s1, TOK___COUNTER__, MACRO_OBJ, NULL, NULL);
}

ST_FUNC void tccpp_delete(TCCState *s1)
{
    int n;

    /* free tokens */
    n = neocc_token_next(s1) - TOK_IDENT;
    if (n > s1->total_idents)
        s1->total_idents = n;
    /* The preprocessor drops arena references before tokens releases them. */
    if (!RP(vo_interact0(s1->preprocessor, NEOCC_AC0_PREPROCESSOR_RESET,
                         &neocc_host_caller)))
        _tcc_error(s1, "cannot reset preprocessor");

    if (!RP(vo_interact0(s1->tokens, NEOCC_AC0_TOKENS_RESET,
                         &neocc_host_caller)))
        _tcc_error(s1, "cannot reset tokens");
}

/* ------------------------------------------------------------------------- */
/* tcc -E [-P[1]] [-dD} support */

static int pp_need_space(int a, int b);

static void tok_print(TCCState *s1, const int *str, const char *msg, ...)
{
    FILE *fp = s1->ppfp;
    va_list ap;
    int t, t0, s;
    union neocc_value cval;

    va_start(ap, msg);
    vfprintf(fp, msg, ap);
    va_end(ap);

    s = t0 = 0;
    while (str) {
	TOK_GET(&t, &str, &cval);
	if (t == 0 || t == TOK_EOF)
	    break;
        if (pp_need_space(t0, t))
            s = 0;
	fprintf(fp, &" %s"[s], t == TOK_PLCHLDR ? "<>" : get_tok_str(s1, t, &cval));
        s = 1, t0 = t;
    }
    fprintf(fp, "\n");
}

static void pp_line(TCCState *s1, struct neocc_source_buffer *f, int level)
{
    int d = f->line_num - f->line_ref;

    if (s1->dflag & 4)
	return;

    if (s1->Pflag == LINE_MACRO_OUTPUT_FORMAT_NONE) {
        ;
    } else if (level == 0 && f->line_ref && d < 8) {
	while (d > 0)
	    fputs("\n", s1->ppfp), --d;
    } else if (s1->Pflag == LINE_MACRO_OUTPUT_FORMAT_STD) {
	fprintf(s1->ppfp, "#line %d \"%s\"\n", f->line_num, f->filename);
    } else {
	fprintf(s1->ppfp, "# %d \"%s\"%s\n", f->line_num, f->filename,
	    level > 0 ? " 1" : level < 0 ? " 2" : "");
    }
    f->line_ref = f->line_num;
}

static void define_print(TCCState *s1, int v)
{
    FILE *fp;
    struct neocc_symbol *s;

    s = define_find(s1, v);
    if (NULL == s || NULL == s->d)
        return;

    fp = s1->ppfp;
    fprintf(fp, "#define %s", get_tok_str(s1, v, NULL));
    if (s->type.t & MACRO_FUNC) {
        struct neocc_symbol *a = s->next;
        fprintf(fp,"(");
        if (a)
            for (;;) {
                fprintf(fp,"%s", get_tok_str(s1, a->v, NULL));
                if (!(a = a->next))
                    break;
                fprintf(fp,",");
            }
        fprintf(fp,")");
    }
    tok_print(s1, s->d, "");
}

static void pp_debug_defines(TCCState *s1)
{
    int v, t;
    const char *vs;
    FILE *fp;

    t = s1->pp->debug_token;
    if (t == 0)
        return;

    s1->pp->source->line_num--;
    pp_line(s1, s1->pp->source, 0);
    s1->pp->source->line_ref = ++s1->pp->source->line_num;

    fp = s1->ppfp;
    v = s1->pp->debug_symbol;
    vs = get_tok_str(s1, v, NULL);
    if (t == TOK_DEFINE) {
        define_print(s1, v);
    } else if (t == TOK_UNDEF) {
        fprintf(fp, "#undef %s\n", vs);
    } else if (t == TOK_push_macro) {
        fprintf(fp, "#pragma push_macro(\"%s\")\n", vs);
    } else if (t == TOK_pop_macro) {
        fprintf(fp, "#pragma pop_macro(\"%s\")\n", vs);
    }
    s1->pp->debug_token = 0;
}

/* Add a space between tokens a and b to avoid unwanted textual pasting */
static int pp_need_space(int a, int b)
{
    return 'E' == a ? '+' == b || '-' == b
        : '+' == a ? TOK_INC == b || '+' == b
        : '-' == a ? TOK_DEC == b || '-' == b
        : a >= TOK_IDENT || a == TOK_PPNUM ? b >= TOK_IDENT || b == TOK_PPNUM
        : 0;
}

/* maybe hex like 0x1e */
static int pp_check_he0xE(int t, const char *p)
{
    if (t == TOK_PPNUM && toup(strchr(p, 0)[-1]) == 'E')
        return 'E';
    return t;
}

/* Preprocess the current file */
ST_FUNC int tcc_preprocess(TCCState *s1)
{
    struct neocc_source_buffer **iptr;
    int token_seen, spcs, level;
    const char *p;
    char white[400];

    s1->pp->parse_flags = PARSE_FLAG_PREPROCESS
                | (s1->pp->parse_flags & PARSE_FLAG_ASM_FILE)
                | PARSE_FLAG_LINEFEED
                | PARSE_FLAG_SPACES
                | PARSE_FLAG_ACCEPT_STRAYS
                ;
    /* Credits to Fabrice Bellard's initial revision to demonstrate its
       capability to compile and run itself, provided all numbers are
       given as decimals. tcc -E -P10 will do. */
    if (s1->Pflag == LINE_MACRO_OUTPUT_FORMAT_P10)
        s1->pp->parse_flags |= PARSE_FLAG_TOK_NUM, s1->Pflag = 1;

    if (s1->do_bench) {
	/* for PP benchmarks */
	do next(s1); while (s1->pp->token != TOK_EOF);
	return 0;
    }

    token_seen = TOK_LINEFEED, spcs = 0, level = 0;
    if (s1->pp->source->prev)
        pp_line(s1, s1->pp->source->prev, level++);
    pp_line(s1, s1->pp->source, level);

    for (;;) {
        iptr = s1->pp->include_stack_ptr;
        next(s1);
        if (s1->pp->token == TOK_EOF)
            break;

        level = s1->pp->include_stack_ptr - iptr;
        if (level) {
            if (level > 0)
                pp_line(s1, *iptr, 0);
            pp_line(s1, s1->pp->source, level);
        }
        if (s1->dflag & 7) {
            pp_debug_defines(s1);
            if (s1->dflag & 4)
                continue;
        }

        if (is_space(s1->pp->token)) {
            if (spcs < sizeof white - 1)
                white[spcs++] = s1->pp->token;
            continue;
        } else if (s1->pp->token == TOK_LINEFEED) {
            spcs = 0;
            if (token_seen == TOK_LINEFEED)
                continue;
            ++s1->pp->source->line_ref;
        } else if (token_seen == TOK_LINEFEED) {
            pp_line(s1, s1->pp->source, 0);
        } else if (spcs == 0 && pp_need_space(token_seen, s1->pp->token)) {
            white[spcs++] = ' ';
        }

        white[spcs] = 0, fputs(white, s1->ppfp), spcs = 0;
        fputs(p = get_tok_str(s1, s1->pp->token, &s1->pp->value), s1->ppfp);
        token_seen = pp_check_he0xE(s1->pp->token, p);
    }
    return 0;
}

/* ------------------------------------------------------------------------- */
