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

#define USING_GLOBALS
#include "tcc.h"

/* #define to 1 to enable (see parse_pp_string()) */
#define ACCEPT_LF_IN_STRINGS 0

static void tok_print(const int *str, const char *msg, ...);
static void next_nomacro(void);
static void parse_number(const char *p);
static void parse_string(const char *p, int len);


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

ST_FUNC void skip(int c)
{
    if (tcc_state->pp->token != c) {
        char tmp[40];
        pstrcpy(tmp, sizeof tmp, get_tok_str(c, &tcc_state->pp->value));
        tcc_error("'%s' expected (got '%s')", tmp, get_tok_str(tcc_state->pp->token, &tcc_state->pp->value));
	}
    next();
}

ST_FUNC void expect(const char *msg)
{
    tcc_error("%s expected", msg);
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
		tcc_error("cannot resize token stream");
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
		tcc_error("cannot query token");
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
static void cstr_realloc(struct neocc_string *cstr, int new_size)
{
    if (!neocc_string_reserve(cstr, new_size))
        tcc_error("cannot grow compiler string");
}

/* add a byte */
ST_INLN void cstr_ccat(struct neocc_string *cstr, int ch)
{
    int size;
    size = cstr->size + 1;
    if (size > cstr->size_allocated)
        cstr_realloc(cstr, size);
    cstr->data[size - 1] = ch;
    cstr->size = size;
}

ST_INLN char *unicode_to_utf8 (char *b, uint32_t Uc)
{
    if (Uc<0x80) *b++=Uc;
    else if (Uc<0x800) *b++=192+Uc/64, *b++=128+Uc%64;
    else if (Uc-0xd800u<0x800) goto error;
    else if (Uc<0x10000) *b++=224+Uc/4096, *b++=128+Uc/64%64, *b++=128+Uc%64;
    else if (Uc<0x110000) *b++=240+Uc/262144, *b++=128+Uc/4096%64, *b++=128+Uc/64%64, *b++=128+Uc%64;
    else error: tcc_error("0x%x is not a valid universal character", Uc);
    return b;
}

/* add a unicode character expanded into utf8 */
ST_INLN void cstr_u8cat(struct neocc_string *cstr, int ch)
{
    char buf[4], *e;
    e = unicode_to_utf8(buf, (uint32_t)ch);
    cstr_cat(cstr, buf, e - buf);
}

/* add string of 'len', or of its len/len+1 when 'len' == -1/0 */
ST_FUNC void cstr_cat(struct neocc_string *cstr, const char *str, int len)
{
    int size;
    if (len <= 0)
        len = strlen(str) + 1 + len;
    size = cstr->size + len;
    if (size > cstr->size_allocated)
        cstr_realloc(cstr, size);
    memmove(cstr->data + cstr->size, str, len);
    cstr->size = size;
}

/* add a wide char */
ST_FUNC void cstr_wccat(struct neocc_string *cstr, int ch)
{
    int size;
    size = cstr->size + sizeof(nwchar_t);
    if (size > cstr->size_allocated)
        cstr_realloc(cstr, size);
    *(nwchar_t *)(cstr->data + size - sizeof(nwchar_t)) = ch;
    cstr->size = size;
}

/* reset string to empty */
ST_FUNC void cstr_reset(struct neocc_string *cstr)
{
    cstr->size = 0;
}

ST_FUNC int cstr_vprintf(struct neocc_string *cstr, const char *fmt, va_list ap)
{
    int length = neocc_string_vprintf(cstr, fmt, ap);
    if (length < 0)
        tcc_error("cannot grow compiler string");
    return length;
}

ST_FUNC int cstr_printf(struct neocc_string *cstr, const char *fmt, ...)
{
    va_list ap; int len;
    va_start(ap, fmt);
    len = cstr_vprintf(cstr, fmt, ap);
    va_end(ap);
    return len;
}

/* XXX: unicode ? */
static void add_char(struct neocc_string *cstr, int c)
{
    if (c == '\'' || c == '\"' || c == '\\') {
        /* XXX: could be more precise if char or string */
        cstr_ccat(cstr, '\\');
    }
    if (c >= 32 && c <= 126) {
        cstr_ccat(cstr, c);
    } else {
        cstr_ccat(cstr, '\\');
        if (c == '\n') {
            cstr_ccat(cstr, 'n');
        } else {
            cstr_ccat(cstr, '0' + ((c >> 6) & 7));
            cstr_ccat(cstr, '0' + ((c >> 3) & 7));
            cstr_ccat(cstr, '0' + (c & 7));
        }
    }
}

/* ------------------------------------------------------------------------- */
/* Find or intern a token; its spelling and ID are owned by tokens. */
ST_FUNC struct neocc_token *tok_alloc(const char *str, int len)
{
	struct neocc_token_intern request = { str, (size_t)len };
	struct neocc_token_result result;
	unsigned int quantity;
	int status = vo_interact3(tcc_state->tokens, NEOCC_AC3_TOKENS_INTERN,
		(unsigned char *)&request, sizeof(request),
		(unsigned char *)&result, sizeof(result), &quantity,
		&neocc_host_caller);

	if (!RP(status) || quantity != sizeof(result))
		tcc_error("cannot intern token");
	return result.token;
}

#define TOK_HASH_INIT 1
#define TOK_HASH_FUNC(h, c) ((h) + ((h) << 5) + ((h) >> 27) + (c))

ST_FUNC int tok_alloc_const(const char *str)
{
    return tok_alloc(str, strlen(str))->tok;
}

/* XXX: buffer overflow */
/* XXX: float tokens */
ST_FUNC const char *get_tok_str(int v, union neocc_value *cv)
{
    char *p;
    int i, len;

    cstr_reset(&tcc_state->pp->spelling);
    p = tcc_state->pp->spelling.data;

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
        cstr_ccat(&tcc_state->pp->spelling, 'L');
    case TOK_CCHAR:
        cstr_ccat(&tcc_state->pp->spelling, '\'');
        add_char(&tcc_state->pp->spelling, cv->i);
        cstr_ccat(&tcc_state->pp->spelling, '\'');
        cstr_ccat(&tcc_state->pp->spelling, '\0');
        break;
    case TOK_PPNUM:
    case TOK_PPSTR:
        return (char*)cv->str.data;
    case TOK_LSTR:
        cstr_ccat(&tcc_state->pp->spelling, 'L');
    case TOK_STR:
        cstr_ccat(&tcc_state->pp->spelling, '\"');
        if (v == TOK_STR) {
            len = cv->str.size - 1;
            for(i=0;i<len;i++)
                add_char(&tcc_state->pp->spelling, ((unsigned char *)cv->str.data)[i]);
        } else {
            len = (cv->str.size / sizeof(nwchar_t)) - 1;
            for(i=0;i<len;i++)
                add_char(&tcc_state->pp->spelling, ((nwchar_t *)cv->str.data)[i]);
        }
        cstr_ccat(&tcc_state->pp->spelling, '\"');
        cstr_ccat(&tcc_state->pp->spelling, '\0');
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
                    return tcc_state->pp->spelling.data;
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
        } else if (v < neocc_token_next(tcc_state)) {
            return neocc_token_at(tcc_state, (v - TOK_IDENT) + TOK_IDENT)->str;
        } else if (v >= SYM_FIRST_ANOM) {
            /* special name for anonymous symbol */
            sprintf(p, "L.%u", v - SYM_FIRST_ANOM);
        } else {
            /* should never happen */
            return NULL;
        }
        break;
    }
    return tcc_state->pp->spelling.data;
}

/* return the current character, handling end of block if necessary
   (but not stray) */
static int handle_eob(void)
{
    struct neocc_source_buffer *bf = tcc_state->pp->source;
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
        total_bytes += len;
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
static int next_c(void)
{
    int ch = *++tcc_state->pp->source->buf_ptr;
    /* end of buffer/file handling */
    if (ch == CH_EOB && tcc_state->pp->source->buf_ptr >= tcc_state->pp->source->buf_end)
        ch = handle_eob();
    return ch;
}

/* input with '\[\r]\n' handling. */
static int handle_stray_noerror(int err)
{
    int ch;
    while ((ch = next_c()) == '\\') {
        ch = next_c();
        if (ch == '\n') {
    newl:
            tcc_state->pp->source->line_num++;
        } else {
            if (ch == '\r') {
                ch = next_c();
                if (ch == '\n')
                    goto newl;
                *--tcc_state->pp->source->buf_ptr = '\r';
            }
            if (err)
                tcc_error("stray '\\' in program");
            /* may take advantage of 'struct neocc_source_buffer.unget[4}' */
            return *--tcc_state->pp->source->buf_ptr = '\\';
        }
    }
    return ch;
}

#define ninp() handle_stray_noerror(0)

/* handle '\\' in strings, comments and skipped regions */
static int handle_bs(uint8_t **p)
{
    int c;
    tcc_state->pp->source->buf_ptr = *p - 1;
    c = ninp();
    *p = tcc_state->pp->source->buf_ptr;
    return c;
}

/* skip the stray and handle the \\n case. Output an error if
   incorrect char after the stray */
static int handle_stray(uint8_t **p)
{
    int c;
    tcc_state->pp->source->buf_ptr = *p - 1;
    c = handle_stray_noerror(!(tcc_state->pp->parse_flags & PARSE_FLAG_ACCEPT_STRAYS));
    *p = tcc_state->pp->source->buf_ptr;
    return c;
}

/* handle the complicated stray case */
#define PEEKC(c, p)\
{\
    c = *++p;\
    if (c == '\\')\
        c = handle_stray(&p); \
}

static int skip_spaces(void)
{
    int ch;
    --tcc_state->pp->source->buf_ptr;
    do {
        ch = ninp();
    } while (tcc_state->pp->character[ch - CH_EOF] & NEOCC_CHAR_SPACE);
    return ch;
}

/* single line C++ comments */
static uint8_t *parse_line_comment(uint8_t *p)
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
        c = handle_bs(&p);
        if (c == CH_EOF)
            break;
        if (c != '\\')
            goto redo;
    }
    return p;
}

/* C comments */
static uint8_t *parse_comment(uint8_t *p)
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
            tcc_state->pp->source->line_num++;
        } else if (c == '*') {
            do {
                c = *++p;
            } while (c == '*');
            if (c == '\\')
                c = handle_bs(&p);
            if (c == '/')
                break;
            goto check_eof;
        } else {
            c = handle_bs(&p);
        check_eof:
            if (c == CH_EOF)
                tcc_error("unexpected end of file in comment");
            if (c != '\\')
                goto redo;
        }
    }
    return p + 1;
}

/* parse a string without interpreting escapes */
static uint8_t *parse_pp_string(uint8_t *p, int sep, struct neocc_string *str)
{
    int c;
    for(;;) {
        c = *++p;
    redo:
        if (c == sep) {
            break;
        } else if (c == '\\') {
            c = handle_bs(&p);
            if (c == CH_EOF) {
        unterminated_string:
                /* XXX: indicate line number of start of string */
                tcc_state->pp->token_flags &= ~TOK_FLAG_BOL;
                tcc_error("missing terminating %c character", sep);
            } else if (c == '\\') {
                if (str)
                    cstr_ccat(str, c);
                c = *++p;
                /* add char after '\\' unconditionally */
                if (c == '\\') {
                    c = handle_bs(&p);
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
                tcc_state->pp->source->line_num++;
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
                c = handle_bs(&p);
            if (c == '\n')
                goto add_lf;
            if (c == CH_EOF)
                goto unterminated_string;
            if (str)
                cstr_ccat(str, '\r');
            goto redo;
        } else {
        add_char:
            if (str)
                cstr_ccat(str, c);
        }
    }
    p++;
    return p;
}

/* skip block of text until #else, #elif or #endif. skip also pairs of
   #if/#endif */
static void preprocess_skip(void)
{
    int a, start_of_line, c, in_warn_or_error;
    uint8_t *p;

    p = tcc_state->pp->source->buf_ptr;
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
            tcc_state->pp->source->line_num++;
            p++;
            goto redo_start;
        case '\\':
            c = handle_bs(&p);
            if (c == CH_EOF)
                expect("#endif");
            if (c == '\\')
                ++p;
            continue;
        /* skip strings */
        case '\"':
        case '\'':
            if (in_warn_or_error)
                goto _default;
            tcc_state->pp->token_flags &= ~TOK_FLAG_BOL;
            p = parse_pp_string(p, c, NULL);
            break;
        /* skip comments */
        case '/':
            if (in_warn_or_error)
                goto _default;
            ++p;
            c = handle_bs(&p);
            if (c == '*') {
                p = parse_comment(p);
            } else if (c == '/') {
                p = parse_line_comment(p);
            }
            continue;
        case '#':
            p++;
            if (start_of_line) {
                tcc_state->pp->source->buf_ptr = p;
                next_nomacro();
                p = tcc_state->pp->source->buf_ptr;
                if (a == 0 && 
                    (tcc_state->pp->token == TOK_ELSE || tcc_state->pp->token == TOK_ELIF || tcc_state->pp->token == TOK_ENDIF))
                    goto the_end;
                if (tcc_state->pp->token == TOK_IF || tcc_state->pp->token == TOK_IFDEF || tcc_state->pp->token == TOK_IFNDEF)
                    a++;
                else if (tcc_state->pp->token == TOK_ENDIF)
                    a--;
                else if( tcc_state->pp->token == TOK_ERROR || tcc_state->pp->token == TOK_WARNING)
                    in_warn_or_error = 1;
                else if (tcc_state->pp->token == TOK_LINEFEED)
                    goto redo_start;
                else if (tcc_state->pp->parse_flags & PARSE_FLAG_ASM_FILE)
                    p = parse_line_comment(p - 1);
            }
#if !defined(TCC_TARGET_ARM) && !defined(TCC_TARGET_ARM64)
            else if (tcc_state->pp->parse_flags & PARSE_FLAG_ASM_FILE)
                p = parse_line_comment(p - 1);
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
    tcc_state->pp->source->buf_ptr = p;
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

ST_FUNC struct neocc_token_string *tok_str_alloc(void)
{
    struct neocc_token_string *str = neocc_token_resize(tcc_state, 0, sizeof *str);
    tok_str_new(str);
    return str;
}

ST_FUNC void tok_str_free_str(int *str)
{
    neocc_token_resize(tcc_state, str, 0);
}

ST_FUNC void tok_str_free(struct neocc_token_string *str)
{
    tok_str_free_str(str->str);
    neocc_token_resize(tcc_state, str, 0);
}

ST_FUNC int *tok_str_realloc(struct neocc_token_string *s, int new_size)
{
    int *str, size;

    size = s->allocated_len;
    if (size < 16)
        size = 16;
    while (size < new_size)
        size = size * 2;
    if (size > s->allocated_len) {
        str = neocc_token_resize(tcc_state, s->str, size * sizeof(int));
        s->allocated_len = size;
        s->str = str;
    }
    return s->str;
}

ST_FUNC void tok_str_add(struct neocc_token_string *s, int t)
{
    int len, *str;

    len = s->len;
    str = s->str;
    if (len >= s->allocated_len)
        str = tok_str_realloc(s, len + 1);
    str[len++] = t;
    s->len = len;
}

ST_FUNC void begin_macro(struct neocc_token_string *str, int alloc)
{
	struct neocc_macro_push request = { str, alloc, tcc_state->pp->source->line_num };
	int result = vo_interact2(tcc_state->preprocessor,
		NEOCC_AC2_PREPROCESSOR_PUSH, (unsigned char *)&request,
		sizeof(request), &neocc_host_caller);

	if (!RP(result))
		tcc_error("cannot enter macro stream");
}

ST_FUNC void end_macro(void)
{
	struct neocc_macro_pop output;
	unsigned int quantity;
	int result = vo_interact1(tcc_state->preprocessor,
		NEOCC_AC1_PREPROCESSOR_POP, (unsigned char *)&output,
		sizeof(output), &quantity, &neocc_host_caller);

	if (!RP(result) || quantity != sizeof(output))
		tcc_error("cannot leave macro stream");
	tcc_state->pp->source->line_num = output.saved_line;
	if (output.release)
		tok_str_free(output.release);
}

static void tok_str_add2(struct neocc_token_string *s, int t, union neocc_value *cv)
{
    int len, *str;

    len = s->len;
    str = s->str;

    /* allocate space for worst case */
    if (len + TOK_MAX_SIZE >= s->allocated_len)
        str = tok_str_realloc(s, len + TOK_MAX_SIZE + 1);
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
                str = tok_str_realloc(s, len + nb_words + 1);
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
ST_FUNC void tok_str_add_tok(struct neocc_token_string *s)
{
    union neocc_value cval;

    /* save line number info */
    if (tcc_state->pp->source->line_num != s->last_line_num) {
        s->last_line_num = tcc_state->pp->source->line_num;
        cval.i = s->last_line_num;
        tok_str_add2(s, TOK_LINENUM, &cval);
    }
    tok_str_add2(s, tcc_state->pp->token, &tcc_state->pp->value);
}

/* like tok_str_add2(), add a space if needed */
static void tok_str_add2_spc(struct neocc_token_string *s, int t, union neocc_value *cv)
{
    if (s->need_spc == 3)
        tok_str_add(s, ' ');
    s->need_spc = 2;
    tok_str_add2(s, t, cv);
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

static int macro_is_equal(const int *a, const int *b)
{
    union neocc_value cv;
    int t;

    if (!a || !b)
        return 1;

    while (*a && *b) {
        cstr_reset(&tcc_state->pp->parsed);
        TOK_GET(&t, &a, &cv);
        cstr_cat(&tcc_state->pp->parsed, get_tok_str(t, &cv), 0);
        TOK_GET(&t, &b, &cv);
        if (strcmp(tcc_state->pp->parsed.data, get_tok_str(t, &cv)))
            return 0;
    }
    return !(*a || *b);
}

/* defines handling */
ST_INLN void define_push(int v, int macro_type, int *str, struct neocc_symbol *first_arg)
{
    struct neocc_symbol *s, *o;

    o = define_find(v);
    s = sym_stack_push(NEOCC_SYMBOL_STACK_DEFINE, v, macro_type, 0);
    s->d = str;
    s->next = first_arg;
    neocc_token_at(tcc_state, (v - TOK_IDENT) + TOK_IDENT)->sym_define = s;

    if (o && !macro_is_equal(o->d, s->d))
	tcc_warning("%s redefined", get_tok_str(v, NULL));
}

/* undefined a define symbol. Its name is just set to zero */
ST_FUNC void define_undef(struct neocc_symbol *s)
{
    int v = s->v;
    if (v >= TOK_IDENT && v < neocc_token_next(tcc_state))
        neocc_token_at(tcc_state, (v - TOK_IDENT) + TOK_IDENT)->sym_define = NULL;
}

ST_INLN struct neocc_symbol *define_find(int v)
{
    v -= TOK_IDENT;
    if ((unsigned)v >= (unsigned)(neocc_token_next(tcc_state) - TOK_IDENT))
        return NULL;
    return neocc_token_at(tcc_state, (v) + TOK_IDENT)->sym_define;
}

/* free define stack until top reaches 'b' */
ST_FUNC void free_defines(struct neocc_symbol *b)
{
    struct neocc_symbol *top;

    while ((top = sym_stack_top(NEOCC_SYMBOL_STACK_DEFINE)) != b) {
        tok_str_free_str(top->d);
        define_undef(top);
        sym_stack_drop(NEOCC_SYMBOL_STACK_DEFINE, top);
    }
}

/* fake the nth "#if defined test_..." for tcc -dt -run */
static void maybe_run_test(TCCState *s)
{
    const char *p;
    if (s->include_stack_ptr != s->include_stack)
        return;
    p = get_tok_str(tcc_state->pp->token, NULL);
    if (0 != memcmp(p, "test_", 5))
        return;
    if (0 != --s->run_test)
        return;
    fprintf(s->ppfp, &"\n[%s]\n"[!(s->dflag & 32)], p), fflush(s->ppfp);
    define_push(tcc_state->pp->token, MACRO_OBJ, NULL, NULL);
}

ST_FUNC void skip_to_eol(int warn)
{
    if (tcc_state->pp->token == TOK_LINEFEED)
        return;
    if (warn)
        tcc_warning("extra tokens after directive");
    while (tcc_state->pp->macro_stack)
        end_macro();
    tcc_state->pp->source->buf_ptr = parse_line_comment(tcc_state->pp->source->buf_ptr - 1);
    next_nomacro();
}

static CachedInclude *
search_cached_include(TCCState *s1, const char *filename, int add);

static int parse_include(TCCState *s1, int do_next, int test)
{
    int c, i;
    char name[1024], buf[1024], *p;
    CachedInclude *e;

    c = skip_spaces();
    if (c == '<' || c == '\"') {
        cstr_reset(&s1->pp->parsed);
        s1->pp->source->buf_ptr = parse_pp_string(s1->pp->source->buf_ptr, c == '<' ? '>' : c, &s1->pp->parsed);
        i = s1->pp->parsed.size;
        pstrncpy(name, sizeof name, s1->pp->parsed.data, i);
        next_nomacro();
    } else {
        /* computed #include : concatenate tokens until result is one of
           the two accepted forms.  Don't convert pp-tokens to tokens here. */
	s1->pp->parse_flags = PARSE_FLAG_PREPROCESS
                    | PARSE_FLAG_LINEFEED
                    | (s1->pp->parse_flags & PARSE_FLAG_ASM_FILE);
        name[0] = 0;
        for (;;) {
            next();
            p = name, i = strlen(p) - 1;
            if (i > 0
                && ((p[0] == '"' && p[i] == '"')
                 || (p[0] == '<' && p[i] == '>')))
                break;
            if (s1->pp->token == TOK_LINEFEED)
                tcc_error("'#include' expects \"FILENAME\" or <FILENAME>");
            pstrcat(name, sizeof name, get_tok_str(s1->pp->token, &s1->pp->value));
	}
        c = p[0];
        /* remove '<>|""' */
        memmove(p, p + 1, i - 1), p[i - 1] = 0;
    }

    if (!test)
        skip_to_eol(1);

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
                tcc_error("include file '%s' not found", name);
            pstrcpy(buf, sizeof buf, p);
            pstrcat(buf, sizeof buf, "/");
        }
        pstrcat(buf, sizeof buf, name);
        e = search_cached_include(s1, buf, 0);
        if (e && (define_find(e->ifndef_macro) || e->once)) {
            /* no need to parse the include because the 'ifndef macro'
               is defined (or had #pragma once) */
#ifdef INC_DEBUG
            printf("%s: skipping cached %s\n", s1->pp->source->filename, buf);
#endif
            if ((s1->verbose | 1) == 3) /* -vv[v] */
                printf("=> %*s%s\n",
                   (int)(s1->include_stack_ptr - s1->include_stack), "", buf);
            return 1;
        }
        if (tcc_open(s1, buf) >= 0)
            break;
    }

    if (test) {
        tcc_close();
    } else {
        if (s1->include_stack_ptr >= s1->include_stack + INCLUDE_STACK_SIZE)
            tcc_error("#include recursion too deep");
        /* push previous file on stack */
        *s1->include_stack_ptr++ = s1->pp->source->prev;
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
            if (s1->include_sys_deps || i - 2 < s1->nb_include_paths)
                dynarray_add(&s1->target_deps, &s1->nb_target_deps,
                    tcc_strdup(buf));
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
    
    str = tok_str_alloc();
    s1->pp->expression = 1;
    while (1) {
        next(); /* do macro subst */
        t = s1->pp->token;
        if (s1->pp->token < TOK_IDENT) {
            if (s1->pp->token == TOK_LINEFEED || s1->pp->token == TOK_EOF)
                break;
            if (s1->pp->token >= TOK_STR && s1->pp->token <= TOK_CLDOUBLE)
                tcc_error("invalid constant in preprocessor expression");

        } else if (s1->pp->token == TOK_DEFINED) {
            s1->pp->parse_flags &= ~PARSE_FLAG_PREPROCESS; /* no macro subst */
            next();
            t = s1->pp->token;
            if (t == '(') 
                next();
            s1->pp->parse_flags |= PARSE_FLAG_PREPROCESS;
            if (s1->pp->token < TOK_IDENT)
                expect("identifier after 'defined'");
            if (s1->run_test)
                maybe_run_test(s1);
            c = 0;
            if (define_find(s1->pp->token)
                || s1->pp->token == TOK___HAS_INCLUDE
                || s1->pp->token == TOK___HAS_INCLUDE_NEXT)
                c = 1;
            if (t == '(') {
                next();
                if (s1->pp->token != ')')
                    expect("')'");
            }
            goto c_number;
        } else if (s1->pp->token == TOK___HAS_INCLUDE ||
                   s1->pp->token == TOK___HAS_INCLUDE_NEXT) {
            t = s1->pp->token;
            next();
	    if (s1->pp->token != '(')
		expect("'('");
            c = parse_include(s1, t - TOK___HAS_INCLUDE, 1);
            if (s1->pp->token != ')')
                expect("')'");
            goto c_number;
        } else {
            /* if undefined macro, replace with zero */
            c = 0;
        c_number:
            s1->pp->token = TOK_CLLONG; /* type intmax_t */
            s1->pp->value.i = c;
        }
        tok_str_add_tok(str);
    }
    if (0 == str->len)
        tcc_error("#%s with no expression", get_tok_str(t0, 0));
    tok_str_add(str, TOK_EOF); /* simulate end of file */
    s1->pp->expression = t0; /* redirect pre-processor expression error messages */
    t = s1->pp->token;
    /* now evaluate C constant expression */
    begin_macro(str, 1);
    next();
    c = expr_const();
    if (s1->pp->token != TOK_EOF)
        tcc_error("...");
    s1->pp->expression = 0;
    end_macro();
    s1->pp->token = t; /* restore LF or EOF */
    return c != 0;
}

ST_FUNC int pp_error(struct neocc_string *cs)
{
    int expression = tcc_state->pp->expression;
    int success;

    /* A failure while expanding the diagnostic must not recurse here. */
    tcc_state->pp->expression = 0;
    success = neocc_string_printf(cs, "bad preprocessor expression: #%s",
        get_tok_str(expression, 0)) >= 0;
    tcc_state->pp->macro_ptr = tcc_state->pp->macro_stack->str;
    while (success && (next(), tcc_state->pp->token != TOK_EOF))
        success = neocc_string_printf(cs, " %s",
            get_tok_str(tcc_state->pp->token, &tcc_state->pp->value)) >= 0;
    return success;
}

/* parse after #define */
ST_FUNC void parse_define(void)
{
    struct neocc_symbol *s, *first, **ps;
    int v, t, varg, is_vaargs, t0;
    int saved_parse_flags = tcc_state->pp->parse_flags;
    struct neocc_token_string str;

    v = tcc_state->pp->token;
    if (v < TOK_IDENT || v == TOK_DEFINED)
        tcc_error("invalid macro name '%s'", get_tok_str(tcc_state->pp->token, &tcc_state->pp->value));
    first = NULL;
    t = MACRO_OBJ;
    /* We have to parse the whole define as if not in asm mode, in particular
       no line comment with '#' must be ignored.  Also for function
       macros the argument list must be parsed without '.' being an ID
       character.  */
    tcc_state->pp->parse_flags = ((tcc_state->pp->parse_flags & ~PARSE_FLAG_ASM_FILE) | PARSE_FLAG_SPACES);
    /* '(' must be just after macro definition for MACRO_FUNC */
    next_nomacro();
    tcc_state->pp->parse_flags &= ~PARSE_FLAG_SPACES;
    is_vaargs = 0;
    if (tcc_state->pp->token == '(') {
        int dotid = set_idnum('.', 0);
        next_nomacro();
        ps = &first;
        if (tcc_state->pp->token != ')') for (;;) {
            varg = tcc_state->pp->token;
            next_nomacro();
            is_vaargs = 0;
            if (varg == TOK_DOTS) {
                varg = TOK___VA_ARGS__;
                is_vaargs = 1;
            } else if (tcc_state->pp->token == TOK_DOTS && gnu_ext) {
                is_vaargs = 1;
                next_nomacro();
            }
            if (varg < TOK_IDENT)
        bad_list:
                tcc_error("bad macro parameter list");
            s = sym_stack_push(NEOCC_SYMBOL_STACK_DEFINE,
                               varg | SYM_FIELD, is_vaargs, 0);
            *ps = s;
            ps = &s->next;
            if (tcc_state->pp->token == ')')
                break;
            if (tcc_state->pp->token != ',' || is_vaargs)
                goto bad_list;
            next_nomacro();
        }
        tcc_state->pp->parse_flags |= PARSE_FLAG_SPACES;
        next_nomacro();
        t = MACRO_FUNC;
        set_idnum('.', dotid);
    }

    /* The body of a macro definition should be parsed such that identifiers
       are parsed like the file mode determines (i.e. with '.' being an
       ID character in asm mode).  But '#' should be retained instead of
       regarded as line comment leader, so still don't set ASM_FILE
       in tcc_state->pp->parse_flags. */
    tcc_state->pp->parse_flags |= PARSE_FLAG_ACCEPT_STRAYS | PARSE_FLAG_SPACES | PARSE_FLAG_LINEFEED;
    tok_str_new(&str);
    t0 = 0;
    while (tcc_state->pp->token != TOK_LINEFEED && tcc_state->pp->token != TOK_EOF) {
        if (is_space(tcc_state->pp->token)) {
            str.need_spc |= 1;
        } else {
            if (TOK_TWOSHARPS == tcc_state->pp->token) {
                if (0 == t0)
                    goto bad_twosharp;
                tcc_state->pp->token = TOK_PPJOIN;
                t |= MACRO_JOIN;
            }
            tok_str_add2_spc(&str, tcc_state->pp->token, &tcc_state->pp->value);
            t0 = tcc_state->pp->token;
        }
        next_nomacro();
    }
    tcc_state->pp->parse_flags = saved_parse_flags;
    tok_str_add(&str, 0);
    if (t0 == TOK_PPJOIN)
bad_twosharp:
        tcc_error("'##' cannot appear at either end of macro");
    define_push(v, t, str.str, first);
    //tok_print(str.str, "#define (%d) %s %d:", t | is_vaargs * 4, get_tok_str(v, 0));
}

static CachedInclude *search_cached_include(TCCState *s1, const char *filename, int add)
{
    const char *s, *basename;
    unsigned int h;
    CachedInclude *e;
    int c, i, len;

    s = basename = tcc_basename(filename);
    h = TOK_HASH_INIT;
    while ((c = (unsigned char)*s) != 0) {
#ifdef _WIN32
        h = TOK_HASH_FUNC(h, toup(c));
#else
        h = TOK_HASH_FUNC(h, c);
#endif
        s++;
    }
    h &= (CACHED_INCLUDES_HASH_SIZE - 1);

    i = s1->cached_includes_hash[h];
    for(;;) {
        if (i == 0)
            break;
        e = s1->cached_includes[i - 1];
        if (0 == PATHCMP(filename, e->filename))
            return e;
        if (e->once
            && 0 == PATHCMP(basename, tcc_basename(e->filename))
            && 0 == normalized_PATHCMP(filename, e->filename)
            )
            return e;
        i = e->hash_next;
    }
    if (!add)
        return NULL;

    e = tcc_malloc(sizeof(CachedInclude) + (len = strlen(filename)));
    memcpy(e->filename, filename, len + 1);
    e->ifndef_macro = e->once = 0;
    dynarray_add(&s1->cached_includes, &s1->nb_cached_includes, e);
    /* add in hash table */
    e->hash_next = s1->cached_includes_hash[h];
    s1->cached_includes_hash[h] = s1->nb_cached_includes;
#ifdef INC_DEBUG
    printf("adding cached '%s'\n", filename);
#endif
    return e;
}

static int pragma_parse(TCCState *s1)
{
    next_nomacro();
    if (s1->pp->token == TOK_push_macro || s1->pp->token == TOK_pop_macro) {
        int t = s1->pp->token, v;
        struct neocc_symbol *s;

        if (next(), s1->pp->token != '(')
            goto pragma_err;
        if (next(), s1->pp->token != TOK_STR)
            goto pragma_err;
        v = tok_alloc(s1->pp->value.str.data, s1->pp->value.str.size - 1)->tok;
        if (next(), s1->pp->token != ')')
            goto pragma_err;
        if (t == TOK_push_macro) {
            while (NULL == (s = define_find(v)))
                define_push(v, 0, NULL, NULL);
            s->type.ref = s; /* set push boundary */
        } else {
            for (s = sym_stack_top(NEOCC_SYMBOL_STACK_DEFINE); s;
                 s = s->prev)
                if (s->v == v && s->type.ref == s) {
                    s->type.ref = NULL;
                    break;
                }
        }
        if (s)
            neocc_token_at(tcc_state, (v - TOK_IDENT) + TOK_IDENT)->sym_define = s->d ? s : NULL;
        else
            tcc_warning("unbalanced #pragma pop_macro");
        s1->pp->debug_token = t, s1->pp->debug_symbol = v;

    } else if (s1->pp->token == TOK_once) {
        search_cached_include(s1, s1->pp->source->true_filename, 1)->once = 1;

    } else if (s1->output_type == TCC_OUTPUT_PREPROCESS) {
        /* tcc -E: keep pragmas below unchanged */
        unget_tok(' ');
        unget_tok(TOK_PRAGMA);
        unget_tok('#');
        unget_tok(TOK_LINEFEED);
        return 1;

    } else if (s1->pp->token == TOK_pack) {
        /* This may be:
           #pragma pack(1) // set
           #pragma pack() // reset to default
           #pragma pack(push) // push current
           #pragma pack(push,1) // push & set
           #pragma pack(pop) // restore previous */
        next();
        skip('(');
        if (s1->pp->token == TOK_ASM_pop) {
            next();
            if (s1->pack_stack_ptr <= s1->pack_stack) {
            stk_error:
                tcc_error("out of pack stack");
            }
            s1->pack_stack_ptr--;
        } else {
            int val = 0;
            if (s1->pp->token != ')') {
                if (s1->pp->token == TOK_ASM_push) {
                    next();
                    if (s1->pack_stack_ptr >= s1->pack_stack + PACK_STACK_SIZE - 1)
                        goto stk_error;
                    val = *s1->pack_stack_ptr++;
                    if (s1->pp->token != ',')
                        goto pack_set;
                    next();
                }
                if (s1->pp->token != TOK_CINT)
                    goto pragma_err;
                val = s1->pp->value.i;
                if (val < 1 || val > 16 || (val & (val - 1)) != 0)
                    goto pragma_err;
                next();
            }
        pack_set:
            *s1->pack_stack_ptr = val;
        }
        if (s1->pp->token != ')')
            goto pragma_err;

    } else if (s1->pp->token == TOK_comment) {
        char *p; int t;
        next();
        skip('(');
        t = s1->pp->token;
        next();
        skip(',');
        if (s1->pp->token != TOK_STR)
            goto pragma_err;
        p = tcc_strdup(s1->pp->value.str.data);
        next();
        if (s1->pp->token != ')')
            goto pragma_err;
        if (t == TOK_lib) {
            dynarray_add(&s1->pragma_libs, &s1->nb_pragma_libs, p);
        } else {
            if (t == TOK_option)
                tcc_set_options(s1, p);
            tcc_free(p);
        }

    } else {
        tcc_warning_c(warn_all)("#pragma %s ignored", get_tok_str(s1->pp->token, &s1->pp->value));
        return 0;
    }
    next();
    return 1;
pragma_err:
    tcc_error("malformed #pragma directive");
}

/* put alternative filename */
ST_FUNC void tccpp_putfile(const char *filename)
{
    char buf[1024];
    buf[0] = 0;
    if (!IS_ABSPATH(filename)) {
        /* prepend directory from real file */
        pstrcpy(buf, sizeof buf, tcc_state->pp->source->true_filename);
        *tcc_basename(buf) = 0;
    }
    pstrcat(buf, sizeof buf, filename);
#ifdef _WIN32
    normalize_slashes(buf);
#endif
    if (0 == strcmp(tcc_state->pp->source->filename, buf))
        return;
    //printf("new file '%s'\n", buf);
    if (!RP(vo_interact0(tcc_state->sources, NEOCC_AC0_SOURCE_SAVE_NAME,
            &neocc_host_caller)))
        tcc_error("cannot preserve source filename");
    pstrcpy(tcc_state->pp->source->filename, sizeof tcc_state->pp->source->filename, buf);
    tcc_debug_newfile(tcc_state);
}

/* is_bof is true if first non space token at beginning of file */
ST_FUNC void preprocess(int is_bof)
{
    TCCState *s1 = tcc_state;
    int c, n, saved_parse_flags;
    char buf[1024], *q;
    struct neocc_symbol *s;

    saved_parse_flags = tcc_state->pp->parse_flags;
    tcc_state->pp->parse_flags = PARSE_FLAG_PREPROCESS
        | PARSE_FLAG_TOK_NUM
        | PARSE_FLAG_TOK_STR
        | PARSE_FLAG_LINEFEED
        | (tcc_state->pp->parse_flags & PARSE_FLAG_ASM_FILE)
        ;

    next_nomacro();
 redo:
    switch(tcc_state->pp->token) {
    case TOK_DEFINE:
        tcc_state->pp->debug_token = tcc_state->pp->token;
        next_nomacro();
        tcc_state->pp->debug_symbol = tcc_state->pp->token;
        parse_define();
        break;
    case TOK_UNDEF:
        tcc_state->pp->debug_token = tcc_state->pp->token;
        next_nomacro();
        tcc_state->pp->debug_symbol = tcc_state->pp->token;
        s = define_find(tcc_state->pp->token);
        /* undefine symbol by putting an invalid name */
        if (s)
            define_undef(s);
        next_nomacro();
        break;
    case TOK_INCLUDE:
    case TOK_INCLUDE_NEXT:
        parse_include(s1, tcc_state->pp->token - TOK_INCLUDE, 0);
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
        next_nomacro();
        if (tcc_state->pp->token < TOK_IDENT)
            tcc_error("invalid argument for '#if%sdef'", c ? "n" : "");
        if (is_bof) {
            if (c) {
#ifdef INC_DEBUG
                printf("#ifndef %s\n", get_tok_str(tcc_state->pp->token, NULL));
#endif
                tcc_state->pp->source->ifndef_macro = tcc_state->pp->token;
            }
        }
        if (define_find(tcc_state->pp->token)
            || tcc_state->pp->token == TOK___HAS_INCLUDE
            || tcc_state->pp->token == TOK___HAS_INCLUDE_NEXT)
            c ^= 1;
        next_nomacro();
    do_if:
        if (s1->ifdef_stack_ptr >= s1->ifdef_stack + IFDEF_STACK_SIZE)
            tcc_error("memory full (ifdef)");
        *s1->ifdef_stack_ptr++ = c;
        goto test_skip;
    case TOK_ELSE:
        next_nomacro();
        if (s1->ifdef_stack_ptr == s1->ifdef_stack)
            tcc_error("#else without matching #if");
        if (s1->ifdef_stack_ptr[-1] & 2)
            tcc_error("#else after #else");
        c = (s1->ifdef_stack_ptr[-1] ^= 3);
        goto test_else;
    case TOK_ELIF:
        if (s1->ifdef_stack_ptr == s1->ifdef_stack)
            tcc_error("#elif without matching #if");
        c = s1->ifdef_stack_ptr[-1];
        if (c > 1)
            tcc_error("#elif after #else");
        /* last #if/#elif expression was true: we skip */
        if (c == 1) {
            skip_to_eol(0);
            c = 0;
        } else {
            c = expr_preprocess(s1);
            s1->ifdef_stack_ptr[-1] = c;
        }
    test_else:
        if (s1->ifdef_stack_ptr == tcc_state->pp->source->ifdef_stack_ptr + 1)
            tcc_state->pp->source->ifndef_macro = 0;
    test_skip:
        if (!(c & 1)) {
            skip_to_eol(1);
            preprocess_skip();
            is_bof = 0;
            goto redo;
        }
        break;
    case TOK_ENDIF:
        next_nomacro();
        if (s1->ifdef_stack_ptr <= tcc_state->pp->source->ifdef_stack_ptr)
            tcc_error("#endif without matching #if");
        s1->ifdef_stack_ptr--;
        /* '#ifndef macro' was at the start of file. Now we check if
           an '#endif' is exactly at the end of file */
        if (tcc_state->pp->source->ifndef_macro &&
            s1->ifdef_stack_ptr == tcc_state->pp->source->ifdef_stack_ptr) {
            tcc_state->pp->source->ifndef_macro_saved = tcc_state->pp->source->ifndef_macro;
            /* need to set to zero to avoid false matches if another
               #ifndef at middle of file */
            tcc_state->pp->source->ifndef_macro = 0;
            tcc_state->pp->token_flags |= TOK_FLAG_ENDIF;
        }
        break;

    case TOK_LINE:
        tcc_state->pp->parse_flags &= ~PARSE_FLAG_TOK_NUM;
        next();
        if (tcc_state->pp->token != TOK_PPNUM) {
    _line_err:
            tcc_error("wrong #line format");
        }
        c = 1;
        goto _line_num;
    case TOK_PPNUM:
        if (tcc_state->pp->parse_flags & PARSE_FLAG_ASM_FILE)
            goto ignore;
        c = 0; /* no error with extra tokens */
    _line_num:
        for (n = 0, q = tcc_state->pp->value.str.data; *q; ++q) {
            if (!isnum(*q))
                goto _line_err;
            n = n * 10 + *q - '0';
        }
        tcc_state->pp->parse_flags &= ~PARSE_FLAG_TOK_STR; /* don't parse escape sequences */
        next();
        if (tcc_state->pp->token != TOK_LINEFEED) {
            if (tcc_state->pp->token != TOK_PPSTR || tcc_state->pp->value.str.data[0] != '"')
                goto _line_err;
            tcc_state->pp->value.str.data[tcc_state->pp->value.str.size - 2] = 0;
            tccpp_putfile(tcc_state->pp->value.str.data + 1);
            next();
            /* skip optional level number & advance to next line */
            skip_to_eol(c);
        }
        if (tcc_state->pp->source->fd > 0)
            total_lines += tcc_state->pp->source->line_num - n;
        tcc_state->pp->source->line_num = n;
        break;

    case TOK_ERROR:
    case TOK_WARNING:
    {
        q = buf;
        c = skip_spaces();
        while (c != '\n' && c != CH_EOF) {
            if ((q - buf) < sizeof(buf) - 1)
                *q++ = c;
            c = ninp();
        }
        *q = '\0';
        if (tcc_state->pp->token == TOK_ERROR)
            tcc_error("#error %s", buf);
        else
            tcc_warning("#warning %s", buf);
        next_nomacro();
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
        if (tcc_state->pp->token == '!' && is_bof)
            /* '#!' is ignored at beginning to allow C scripts. */
            goto ignore;
        tcc_warning("ignoring unknown preprocessing directive #%s", get_tok_str(tcc_state->pp->token, &tcc_state->pp->value));
    ignore:
        skip_to_eol(0);
        goto the_end;
    }
    skip_to_eol(1);
 the_end:
    tcc_state->pp->parse_flags = saved_parse_flags;
}

/* evaluate escape codes in a string. */
static void parse_escape_string(struct neocc_string *outstr, const uint8_t *buf, int is_long)
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
                        expect("more hex digits in universal-character-name");
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
                cstr_u8cat(outstr, n);
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
                if (!gnu_ext)
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
                    tcc_warning("unknown escape sequence: \'\\%c\'", c);
                else
                    tcc_warning("unknown escape sequence: \'\\x%x\'", c);
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
            tcc_warning("ill-formed UTF-8 subsequence starting with: \'\\x%x\'", c);
            c = 0xFFFD;
            p += skip;
            goto add_char_nonext;

        }
        p++;
    add_char_nonext:
        if (!is_long)
            cstr_ccat(outstr, c);
        else {
#ifdef TCC_TARGET_PE
            /* store as UTF-16 */
            if (c < 0x10000) {
                cstr_wccat(outstr, c);
            } else {
                c -= 0x10000;
                cstr_wccat(outstr, (c >> 10) + 0xD800);
                cstr_wccat(outstr, (c & 0x3FF) + 0xDC00);
            }
#else
            cstr_wccat(outstr, c);
#endif
        }
    }
    /* add a trailing '\0' */
    if (!is_long)
        cstr_ccat(outstr, '\0');
    else
        cstr_wccat(outstr, '\0');
}

static void parse_string(const char *s, int len)
{
    uint8_t buf[1000], *p = buf;
    int is_long, sep;

    if ((is_long = *s == 'L'))
        ++s, --len;
    sep = *s++;
    len -= 2;
    if (len >= sizeof buf)
        p = tcc_malloc(len + 1);
    memcpy(p, s, len);
    p[len] = 0;

    cstr_reset(&tcc_state->pp->parsed);
    parse_escape_string(&tcc_state->pp->parsed, p, is_long);
    if (p != buf)
        tcc_free(p);

    if (sep == '\'') {
        int char_size, i, n, c;
        /* XXX: make it portable */
        if (!is_long)
            tcc_state->pp->token = TOK_CCHAR, char_size = 1;
        else
            tcc_state->pp->token = TOK_LCHAR, char_size = sizeof(nwchar_t);
        n = tcc_state->pp->parsed.size / char_size - 1;
        if (n < 1)
            tcc_error("empty character constant");
        if (n > 1)
            tcc_warning_c(warn_all)("multi-character character constant");
        for (c = i = 0; i < n; ++i) {
            if (is_long)
                c = ((nwchar_t *)tcc_state->pp->parsed.data)[i];
            else
                c = (c << 8) | ((char *)tcc_state->pp->parsed.data)[i];
        }
        tcc_state->pp->value.i = c;
    } else {
        tcc_state->pp->value.str.size = tcc_state->pp->parsed.size;
        tcc_state->pp->value.str.data = tcc_state->pp->parsed.data;
        if (!is_long)
            tcc_state->pp->token = TOK_STR;
        else
            tcc_state->pp->token = TOK_LSTR;
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
static void parse_number(const char *p)
{
    int b, t, shift, frac_bits, s, exp_val, ch;
    char *q;
    unsigned int bn[BN_SIZE];
    long double d;

    /* number */
    q = tcc_state->pp->number;
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
        } else if (tcc_state->tcc_ext && (ch == 'b' || ch == 'B')) {
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
        if (q >= tcc_state->pp->number + STRING_MAX_SIZE) {
        num_too_long:
            tcc_error("number too long");
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
            q = tcc_state->pp->number;
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
                        tcc_error("invalid digit");
                    frac_bits -= bn_lshift(bn, shift, t);
                    frac_bits += shift;
                    ch = *p++;
                }
            }
            if (ch != 'p' && ch != 'P')
                expect("exponent");
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
                expect("exponent digits");
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
                tcc_state->pp->token = TOK_CFLOAT;
                /* float : should handle overflow */
                tcc_state->pp->value.f = (float)d;
            } else if (t == 'L') {
                ch = *p++;
                tcc_state->pp->token = TOK_CLDOUBLE;
                tcc_state->pp->value.ld = d;
            } else {
                tcc_state->pp->token = TOK_CDOUBLE;
                tcc_state->pp->value.d = (double)d;
            }
        } else {
            /* decimal floats */
            if (ch == '.') {
                if (q >= tcc_state->pp->number + STRING_MAX_SIZE)
                    goto num_too_long;
                *q++ = ch;
                ch = *p++;
            float_frac_parse:
                while (ch >= '0' && ch <= '9') {
                    if (q >= tcc_state->pp->number + STRING_MAX_SIZE)
                        goto num_too_long;
                    *q++ = ch;
                    ch = *p++;
                }
            }
            if (ch == 'e' || ch == 'E') {
                if (q >= tcc_state->pp->number + STRING_MAX_SIZE)
                    goto num_too_long;
                *q++ = ch;
                ch = *p++;
                if (ch == '-' || ch == '+') {
                    if (q >= tcc_state->pp->number + STRING_MAX_SIZE)
                        goto num_too_long;
                    *q++ = ch;
                    ch = *p++;
                }
                if (ch < '0' || ch > '9')
                    expect("exponent digits");
                while (ch >= '0' && ch <= '9') {
                    if (q >= tcc_state->pp->number + STRING_MAX_SIZE)
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
                tcc_state->pp->token = TOK_CFLOAT;
                tcc_state->pp->value.f = strtof(tcc_state->pp->number, NULL);
            } else if (t == 'L') {
                ch = *p++;
                tcc_state->pp->token = TOK_CLDOUBLE;
                tcc_state->pp->value.ld = strtold(tcc_state->pp->number, NULL);
            } else {
                tcc_state->pp->token = TOK_CDOUBLE;
                tcc_state->pp->value.d = strtod(tcc_state->pp->number, NULL);
            }
        }
    } else {
        unsigned long long n, n1;
        int lcount, ucount, ov = 0;
        const char *p1;

        /* integer number */
        *q = '\0';
        q = tcc_state->pp->number;
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
                tcc_error("invalid digit");
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
                    tcc_error("three 'l's in integer constant");
                if (lcount && *(p - 1) != ch)
                    tcc_error("incorrect integer suffix: %s", p1);
                lcount++;
                ch = *p++;
            } else if (t == 'U') {
                if (ucount >= 1)
                    tcc_error("two 'u's in integer constant");
                ucount++;
                ch = *p++;
            } else {
                break;
            }
        }

        /* in #if/#elif expressions, all numbers have type (u)intmax_t anyway */
        if (tcc_state->pp->expression)
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
            tcc_warning("integer constant overflow");

        tcc_state->pp->token = TOK_CINT;
	if (lcount) {
            tcc_state->pp->token = TOK_CLONG;
            if (lcount == 2)
                tcc_state->pp->token = TOK_CLLONG;
	}
	if (ucount)
	    ++tcc_state->pp->token; /* TOK_CU... */
        tcc_state->pp->value.i = n;
    }
    if (ch)
        tcc_error("invalid number");
}

#define PARSE2(c1, tok1, c2, tok2)              \
    case c1:                                    \
        PEEKC(c, p);                            \
        if (c == c2) {                          \
            p++;                                \
            tcc_state->pp->token = tok2;                         \
        } else {                                \
            tcc_state->pp->token = tok1;                         \
        }                                       \
        break;

/* return next token without macro substitution */
static void next_nomacro(void)
{
    int t, c, is_long, len;
    struct neocc_token *ts;
    uint8_t *p, *p1;

    p = tcc_state->pp->source->buf_ptr;
 redo_no_start:
    c = *p;
    switch(c) {
    case ' ':
    case '\t':
        tcc_state->pp->token = c;
        p++;
 maybe_space:
        if (tcc_state->pp->parse_flags & PARSE_FLAG_SPACES)
            goto keep_tok_flags;
        while (tcc_state->pp->character[*p - CH_EOF] & NEOCC_CHAR_SPACE)
            ++p;
        goto redo_no_start;
    case '\f':
    case '\v':
    case '\r':
        p++;
        goto redo_no_start;
    case '\\':
        /* first look if it is in fact an end of buffer */
        c = handle_stray(&p);
        if (c == '\\')
            goto parse_simple;
        if (c == CH_EOF) {
            TCCState *s1 = tcc_state;
            if (!(tcc_state->pp->token_flags & TOK_FLAG_BOL)) {
                /* add implicit newline */
                goto maybe_newline;
            } else if (!(tcc_state->pp->parse_flags & PARSE_FLAG_PREPROCESS)) {
                tcc_state->pp->token = TOK_EOF;
            } else if (s1->ifdef_stack_ptr != tcc_state->pp->source->ifdef_stack_ptr) {
                tcc_error("missing #endif");
            } else if (s1->include_stack_ptr == s1->include_stack) {
                /* no include left : end of file. */
                tcc_state->pp->token = TOK_EOF;
            } else {
                /* pop include file */

                /* test if previous '#endif' was after a #ifdef at
                   start of file */
                if (tcc_state->pp->token_flags & TOK_FLAG_ENDIF) {
#ifdef INC_DEBUG
                    printf("#endif %s\n", get_tok_str(tcc_state->pp->source->ifndef_macro_saved, NULL));
#endif
                    search_cached_include(s1, tcc_state->pp->source->true_filename, 1)
                        ->ifndef_macro = tcc_state->pp->source->ifndef_macro_saved;
                    tcc_state->pp->token_flags &= ~TOK_FLAG_ENDIF;
                }

                /* add end of include file debug info */
                tcc_debug_eincl(tcc_state);
                /* pop include stack */
                tcc_close();
                s1->include_stack_ptr--;
                p = tcc_state->pp->source->buf_ptr;
                goto maybe_newline;
            }
        } else {
            goto redo_no_start;
        }
        break;

    case '\n':
        tcc_state->pp->source->line_num++;
        p++;
maybe_newline:
        tcc_state->pp->token_flags |= TOK_FLAG_BOL;
        if (0 == (tcc_state->pp->parse_flags & PARSE_FLAG_LINEFEED))
            goto redo_no_start;
        tcc_state->pp->token = TOK_LINEFEED;
        goto keep_tok_flags;

    case '#':
        /* XXX: simplify */
        PEEKC(c, p);
        if ((tcc_state->pp->token_flags & TOK_FLAG_BOL) &&
            (tcc_state->pp->parse_flags & PARSE_FLAG_PREPROCESS)) {
            tcc_state->pp->token_flags &= ~TOK_FLAG_BOL;
            tcc_state->pp->source->buf_ptr = p;
            preprocess(tcc_state->pp->token_flags & TOK_FLAG_BOF);
            p = tcc_state->pp->source->buf_ptr;
            goto maybe_newline;
        } else {
            if (c == '#') {
                p++;
                tcc_state->pp->token = TOK_TWOSHARPS;
            } else {
#if !defined(TCC_TARGET_ARM) && !defined(TCC_TARGET_ARM64)
                if (tcc_state->pp->parse_flags & PARSE_FLAG_ASM_FILE) {
                    p = parse_line_comment(p - 1);
                    goto redo_no_start;
                } else
#endif
                {
                    tcc_state->pp->token = '#';
                }
            }
        }
        break;
    
    /* dollar is allowed to start identifiers when not parsing asm */
    case '$':
        if (!(tcc_state->pp->character['$' - CH_EOF] & NEOCC_CHAR_ID)
         || (tcc_state->pp->parse_flags & PARSE_FLAG_ASM_FILE))
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
        while (c = *++p, tcc_state->pp->character[c - CH_EOF] & (NEOCC_CHAR_ID|NEOCC_CHAR_NUMBER))
            ;
        len = p - p1;
        if (c != '\\') {
            ts = tok_alloc((char *)p1, len);
        } else {
            /* slower case */
            cstr_reset(&tcc_state->pp->parsed);
            cstr_cat(&tcc_state->pp->parsed, (char *) p1, len);
            p--;
            PEEKC(c, p);
            while (tcc_state->pp->character[c - CH_EOF] & (NEOCC_CHAR_ID|NEOCC_CHAR_NUMBER))
            {
                cstr_ccat(&tcc_state->pp->parsed, c);
                PEEKC(c, p);
            }
            ts = tok_alloc(tcc_state->pp->parsed.data, tcc_state->pp->parsed.size);
        }
        tcc_state->pp->token = ts->tok;
        break;
    case 'L':
        t = p[1];
        if (t == '\'' || t == '\"' || t == '\\') {
            PEEKC(c, p);
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
        PEEKC(c, p);
        /* after the first digit, accept digits, alpha, '.' or sign if
           prefixed by 'eEpP' */
    parse_num:
        cstr_reset(&tcc_state->pp->parsed);
        for(;;) {
            cstr_ccat(&tcc_state->pp->parsed, t);
            if (!((tcc_state->pp->character[c - CH_EOF] & (NEOCC_CHAR_ID|NEOCC_CHAR_NUMBER))
                  || c == '.'
                  || ((c == '+' || c == '-')
                      && (((t == 'e' || t == 'E')
                            && !(tcc_state->pp->parse_flags & PARSE_FLAG_ASM_FILE
                                /* 0xe+1 is 3 tokens in asm */
                                && ((char*)tcc_state->pp->parsed.data)[0] == '0'
                                && toup(((char*)tcc_state->pp->parsed.data)[1]) == 'X'))
                          || t == 'p' || t == 'P'))))
                break;
            t = c;
            PEEKC(c, p);
        }
        /* We add a trailing '\0' to ease parsing */
        cstr_ccat(&tcc_state->pp->parsed, '\0');
        tcc_state->pp->value.str.size = tcc_state->pp->parsed.size;
        tcc_state->pp->value.str.data = tcc_state->pp->parsed.data;
        tcc_state->pp->token = TOK_PPNUM;
        break;

    case '.':
        /* special dot handling because it can also start a number */
        PEEKC(c, p);
        if (isnum(c)) {
            t = '.';
            goto parse_num;
        } else if ((tcc_state->pp->character['.' - CH_EOF] & NEOCC_CHAR_ID)
                   && (tcc_state->pp->character[c - CH_EOF] & (NEOCC_CHAR_ID|NEOCC_CHAR_NUMBER))) {
            *--p = c = '.';
            goto parse_ident_fast;
        } else if (c == '.') {
            PEEKC(c, p);
            if (c == '.') {
                p++;
                tcc_state->pp->token = TOK_DOTS;
            } else {
                *--p = '.'; /* may underflow into file->unget[] */
                tcc_state->pp->token = '.';
            }
        } else {
            tcc_state->pp->token = '.';
        }
        break;
    case '\'':
    case '\"':
        is_long = 0;
    str_const:
        cstr_reset(&tcc_state->pp->parsed);
        if (is_long)
            cstr_ccat(&tcc_state->pp->parsed, 'L');
        cstr_ccat(&tcc_state->pp->parsed, c);
        p = parse_pp_string(p, c, &tcc_state->pp->parsed);
        cstr_ccat(&tcc_state->pp->parsed, c);
        cstr_ccat(&tcc_state->pp->parsed, '\0');
        tcc_state->pp->value.str.size = tcc_state->pp->parsed.size;
        tcc_state->pp->value.str.data = tcc_state->pp->parsed.data;
        tcc_state->pp->token = TOK_PPSTR;
        break;

    case '<':
        PEEKC(c, p);
        if (c == '=') {
            p++;
            tcc_state->pp->token = TOK_LE;
        } else if (c == '<') {
            PEEKC(c, p);
            if (c == '=') {
                p++;
                tcc_state->pp->token = TOK_A_SHL;
            } else {
                tcc_state->pp->token = TOK_SHL;
            }
        } else {
            tcc_state->pp->token = TOK_LT;
        }
        break;
    case '>':
        PEEKC(c, p);
        if (c == '=') {
            p++;
            tcc_state->pp->token = TOK_GE;
        } else if (c == '>') {
            PEEKC(c, p);
            if (c == '=') {
                p++;
                tcc_state->pp->token = TOK_A_SAR;
            } else {
                tcc_state->pp->token = TOK_SAR;
            }
        } else {
            tcc_state->pp->token = TOK_GT;
        }
        break;
        
    case '&':
        PEEKC(c, p);
        if (c == '&') {
            p++;
            tcc_state->pp->token = TOK_LAND;
        } else if (c == '=') {
            p++;
            tcc_state->pp->token = TOK_A_AND;
        } else {
            tcc_state->pp->token = '&';
        }
        break;
        
    case '|':
        PEEKC(c, p);
        if (c == '|') {
            p++;
            tcc_state->pp->token = TOK_LOR;
        } else if (c == '=') {
            p++;
            tcc_state->pp->token = TOK_A_OR;
        } else {
            tcc_state->pp->token = '|';
        }
        break;

    case '+':
        PEEKC(c, p);
        if (c == '+') {
            p++;
            tcc_state->pp->token = TOK_INC;
        } else if (c == '=') {
            p++;
            tcc_state->pp->token = TOK_A_ADD;
        } else {
            tcc_state->pp->token = '+';
        }
        break;
        
    case '-':
        PEEKC(c, p);
        if (c == '-') {
            p++;
            tcc_state->pp->token = TOK_DEC;
        } else if (c == '=') {
            p++;
            tcc_state->pp->token = TOK_A_SUB;
        } else if (c == '>') {
            p++;
            tcc_state->pp->token = TOK_ARROW;
        } else {
            tcc_state->pp->token = '-';
        }
        break;

    PARSE2('!', '!', '=', TOK_NE)
    PARSE2('=', '=', '=', TOK_EQ)
    PARSE2('*', '*', '=', TOK_A_MUL)
    PARSE2('%', '%', '=', TOK_A_MOD)
    PARSE2('^', '^', '=', TOK_A_XOR)
        
        /* comments or operator */
    case '/':
        PEEKC(c, p);
        if (c == '*') {
            p = parse_comment(p);
            /* comments replaced by a blank */
            tcc_state->pp->token = ' ';
            goto maybe_space;
        } else if (c == '/') {
            p = parse_line_comment(p);
            tcc_state->pp->token = ' ';
            goto maybe_space;
        } else if (c == '=') {
            p++;
            tcc_state->pp->token = TOK_A_DIV;
        } else {
            tcc_state->pp->token = '/';
        }
        break;
        
        /* simple tokens */
    case '@': /* only used in assembler */
#ifdef TCC_TARGET_ARM /* comment on arm asm */
        if (tcc_state->pp->parse_flags & PARSE_FLAG_ASM_FILE) {
            p = parse_line_comment(p);
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
        tcc_state->pp->token = c;
        p++;
        break;
    case 0xEF: /* UTF8 BOM ? */
        if (p[1] == 0xBB && p[2] == 0xBF && p == tcc_state->pp->source->buffer) {
            p += 3;
            goto redo_no_start;
        }
    default:
        if (c >= 0x80 && c <= 0xFF) /* utf8 identifiers */
	    goto parse_ident_fast;
        if (tcc_state->pp->parse_flags & PARSE_FLAG_ASM_FILE)
            goto parse_simple;
        tcc_error("unrecognized character \\x%02x", c);
        break;
    }
    tcc_state->pp->token_flags = 0;
keep_tok_flags:
    tcc_state->pp->source->buf_ptr = p;
#if defined(PARSE_DEBUG)
    printf("token = %d %s\n", tcc_state->pp->token, get_tok_str(tcc_state->pp->token, &tcc_state->pp->value));
#endif
}

#ifdef PP_DEBUG
static void define_print(TCCState *s1, int v);
static void pp_print(const char *msg, int v, const int *str)
{
    FILE *fp = tcc_state->ppfp;

    if (msg[0] == '#' && tcc_state->pp->debug_indent == 0)
        fprintf(fp, "\n");
    else if (msg[0] == '+')
         ++tcc_state->pp->debug_indent, ++msg;
    else if (msg[0] == '-')
        --tcc_state->pp->debug_indent, ++msg;

    fprintf(fp, "%*s", tcc_state->pp->debug_indent, "");
    if (msg[0] == '#') {
        define_print(tcc_state, v);
    } else {
        tok_print(str, v ? "%s %s" : "%s", msg, get_tok_str(v, 0));
    }
}
#define PP_PRINT(x) pp_print x
#else
#define PP_PRINT(x)
#endif

static int macro_subst(
    struct neocc_token_string *tok_str,
    struct neocc_symbol **nested_list,
    const int *macro_str
    );

/* substitute arguments in replacement lists in macro_str by the values in
   args (field d) and return allocated string */
static int *macro_arg_subst(struct neocc_symbol **nested_list, const int *macro_str, struct neocc_symbol *args)
{
    int t, t0, t1, t2, n;
    const int *st;
    struct neocc_symbol *s;
    union neocc_value cval;
    struct neocc_token_string str;

#ifdef PP_DEBUG
    PP_PRINT(("asubst:", 0, macro_str));
    for (s = args, n = 0; s; s = s->prev, ++n);
    while (n--) {
        for (s = args, t = 0; t < n; s = s->prev, ++t);
        tok_print(s->d, "%*s - arg: %s:", tcc_state->pp->debug_indent, "", get_tok_str(s->v, 0));
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
                cstr_reset(&tcc_state->pp->parsed);
                cstr_ccat(&tcc_state->pp->parsed, '\"');
                st = s->d;
                while (*st != TOK_EOF) {
                    const char *s;
                    TOK_GET(&t, &st, &cval);
                    s = get_tok_str(t, &cval);
                    while (*s) {
                        if (t == TOK_PPSTR && *s != '\'')
                            add_char(&tcc_state->pp->parsed, *s);
                        else
                            cstr_ccat(&tcc_state->pp->parsed, *s);
                        ++s;
                    }
                }
                cstr_ccat(&tcc_state->pp->parsed, '\"');
                cstr_ccat(&tcc_state->pp->parsed, '\0');
                //printf("\nstringize: <%s>\n", (char *)tcc_state->pp->parsed.data);
                /* add string */
                cval.str.size = tcc_state->pp->parsed.size;
                cval.str.data = tcc_state->pp->parsed.data;
                tok_str_add2(&str, TOK_PPSTR, &cval);
#ifdef TCC_TARGET_ARM
            } else if ((tcc_state->pp->parse_flags & PARSE_FLAG_ASM_FILE) && t == TOK_PPNUM) {
                /* for example: mov r1,#0 */
                --macro_str, tok_str_add(&str, '#');
#endif
            } else {
                expect("macro parameter after '#'");
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
                    if (t1 == TOK_PPJOIN && t0 == ',' && gnu_ext && s->type.t) {
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
                            tok_str_add(&str, TOK_PLCHLDR);
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
			macro_subst(&str2, nested_list, st);
			tok_str_add(&str2, TOK_EOF);
			s->e = str2.str;
		    }
		    st = s->e;
                }
                while (*st != TOK_EOF) {
                    TOK_GET(&t2, &st, &cval);
                    tok_str_add2(&str, t2, &cval);
                }
            } else {
                tok_str_add(&str, t);
            }
        } else {
            tok_str_add2(&str, t, &cval);
        }
        if (t != ' ')
            t0 = t1, t1 = t;
    }
    tok_str_add(&str, 0);
    PP_PRINT(("areslt:", 0, str.str));
    return str.str;
}

/* handle the '##' operator. return the resulting string (which must be freed). */
static inline int *macro_twosharps(const int *ptr0)
{
    int t1, t2, n, l;
    union neocc_value cv1, cv2;
    struct neocc_token_string macro_str1;
    const int *ptr;

    tok_str_new(&macro_str1);
    cstr_reset(&tcc_state->pp->parsed);
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
                cstr_cat(&tcc_state->pp->parsed, get_tok_str(t1, &cv1), -1);
                t1 = TOK_PLCHLDR;
            }
            cstr_cat(&tcc_state->pp->parsed, get_tok_str(t2, &cv2), -1);
        }
        if (tcc_state->pp->parsed.size) {
            cstr_ccat(&tcc_state->pp->parsed, 0);
            tcc_open_bf(tcc_state, ":paste:", tcc_state->pp->parsed.size);
            memcpy(tcc_state->pp->source->buffer, tcc_state->pp->parsed.data, tcc_state->pp->parsed.size);
            tcc_state->pp->token_flags = 0; /* don't interpret '#' */
            for (n = 0;;n = l) {
                next_nomacro();
                tok_str_add2(&macro_str1, tcc_state->pp->token, &tcc_state->pp->value);
                if (*tcc_state->pp->source->buf_ptr == 0)
                    break;
                tok_str_add(&macro_str1, ' ');
                l = tcc_state->pp->source->buf_ptr - tcc_state->pp->source->buffer;
                tcc_warning("pasting \"%.*s\" and \"%s\" does not give a valid"
                    " preprocessing token", l - n, tcc_state->pp->source->buffer + n, tcc_state->pp->source->buf_ptr);
            }
            tcc_close();
            cstr_reset(&tcc_state->pp->parsed);
        }
        if (t1 != TOK_PLCHLDR)
            tok_str_add2(&macro_str1, t1, &cv1);
    }
    tok_str_add(&macro_str1, 0);
    PP_PRINT(("pasted:", 0, macro_str1.str));
    return macro_str1.str;
}

static int peek_file (struct neocc_token_string *ws_str)
{
    uint8_t *p = tcc_state->pp->source->buf_ptr - 1;
    int c;
    for (;;) {
        PEEKC(c, p);
        switch (c) {
        case '/':
            PEEKC(c, p);
            if (c == '*')
                p = parse_comment(p);
            else if (c == '/')
                p = parse_line_comment(p);
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
            tcc_state->pp->source->line_num++, tcc_state->pp->token_flags |= TOK_FLAG_BOL;
            break;
        default: leave:
            tcc_state->pp->source->buf_ptr = p;
            return c;
        }
        if (ws_str)
            tok_str_add(ws_str, c);
    }
}

/* peek or read [ws_str == NULL] next token from function macro call,
   walking up macro levels up to the file if necessary */
static int next_argstream(struct neocc_symbol **nested_list, struct neocc_token_string *ws_str)
{
    int t;
    struct neocc_symbol *sa;

    while (tcc_state->pp->macro_ptr) {
        const int *m = tcc_state->pp->macro_ptr;
        while ((t = *m) != 0) {
            if (ws_str) {
                if (t != ' ')
                    return t;
                ++m;
            } else {
                TOK_GET(&tcc_state->pp->token, &tcc_state->pp->macro_ptr, &tcc_state->pp->value);
                return tcc_state->pp->token;
            }
        }
        end_macro();
        /* also, end of scope for nested defined symbol */
        sa = *nested_list;
        if (sa)
            *nested_list = sa->prev, sym_free(sa);
    }
    if (ws_str) {
        return peek_file(ws_str);
    } else {
        next_nomacro();
        if (tcc_state->pp->token == '\t' || tcc_state->pp->token == TOK_LINEFEED)
            tcc_state->pp->token = ' ';
        return tcc_state->pp->token;
    }
}

/* do macro substitution of current token with macro 's' and add
   result to (tok_str,tok_len). 'nested_list' is the list of all
   macros we got inside to avoid recursing. Return non zero if no
   substitution needs to be done */
static int macro_subst_tok(
    struct neocc_token_string *tok_str,
    struct neocc_symbol **nested_list,
    struct neocc_symbol *s)
{
    int t;
    int v = s->v;

    PP_PRINT(("#", v, s->d));
    if (s->d) {
        int *mstr = s->d;
        int *jstr;
        struct neocc_symbol *sa;
        int ret;

        if (s->type.t & MACRO_FUNC) {
            int saved_parse_flags = tcc_state->pp->parse_flags;
            struct neocc_token_string str;
            int parlevel, i;
            struct neocc_symbol *sa1, *args;

            tcc_state->pp->parse_flags |= PARSE_FLAG_SPACES | PARSE_FLAG_LINEFEED
                | PARSE_FLAG_ACCEPT_STRAYS;

            tok_str_new(&str);
            /* peek next token from argument stream */
            t = next_argstream(nested_list, &str);
            if (t != '(') {
                /* not a macro substitution after all, restore the
                 * macro token plus all whitespace we've read.
                 * whitespace is intentionally not merged to preserve
                 * newlines. */
                tcc_state->pp->parse_flags = saved_parse_flags;
                tok_str_add2_spc(tok_str, v, 0);
                if (tcc_state->pp->parse_flags & PARSE_FLAG_SPACES)
                    for (i = 0; i < str.len; i++)
                        tok_str_add(tok_str, str.str[i]);
                tok_str_free_str(str.str);
                return 0;
            } else {
                tok_str_free_str(str.str);
            }

            /* argument macro */
            args = NULL;
            sa = s->next;
            /* NOTE: empty args are allowed, except if no args */
            i = 2; /* eat '(' */
            for(;;) {
                do {
                    t = next_argstream(nested_list, NULL);
                } while (t == ' ' || --i);

                if (!sa) {
                    if (t == ')') /* handle '()' case */
                        break;
                    tcc_error("macro '%s' used with too many args",
                        get_tok_str(v, 0));
                }
            empty_arg:
                tok_str_new(&str);
                parlevel = 0;
                /* NOTE: non zero sa->type.t indicates VA_ARGS */
                while (parlevel > 0
                        || (t != ')' && (t != ',' || sa->type.t))) {
                    if (t == TOK_EOF)
                        tcc_error("EOF in invocation of macro '%s'",
                            get_tok_str(v, 0));
                    if (t == '(')
                        parlevel++;
                    if (t == ')')
                        parlevel--;
                    if (t == ' ')
                        str.need_spc |= 1;
                    else
                        tok_str_add2_spc(&str, t, &tcc_state->pp->value);
                    t = next_argstream(nested_list, NULL);
                }
                tok_str_add(&str, TOK_EOF);
                sa1 = sym_push2(&args, sa->v & ~SYM_FIELD, sa->type.t, 0);
                sa1->d = str.str;
                sa = sa->next;
                if (t == ')') {
                    if (!sa)
                        break;
                    /* special case for gcc var args: add an empty
                       var arg argument if it is omitted */
                    if (sa->type.t && gnu_ext)
                        goto empty_arg;
                    tcc_error("macro '%s' used with too few args",
                        get_tok_str(v, 0));
                }
                i = 1;
            }

            /* now subst each arg */
            mstr = macro_arg_subst(nested_list, mstr, args);
            /* free memory */
            sa = args;
            while (sa) {
                sa1 = sa->prev;
                tok_str_free_str(sa->d);
                tok_str_free_str(sa->e);
                sym_free(sa);
                sa = sa1;
            }
            tcc_state->pp->parse_flags = saved_parse_flags;
        }

        /* process '##'s (if present) */
        jstr = mstr;
        if (s->type.t & MACRO_JOIN)
            jstr = macro_twosharps(mstr);

        sa = sym_push2(nested_list, v, 0, 0);
        ret = macro_subst(tok_str, nested_list, jstr);
        /* pop nested defined symbol */
        if (sa == *nested_list)
            *nested_list = sa->prev, sym_free(sa);

        if (jstr != mstr)
            tok_str_free_str(jstr);
        if (mstr != s->d)
            tok_str_free_str(mstr);
        return ret;

    } else {
        union neocc_value cval;
        char buf[32], *cstrval = buf;

        /* special macros */
        if (v == TOK___LINE__ || v == TOK___COUNTER__) {
            t = v == TOK___LINE__ ? tcc_state->pp->source->line_num : tcc_state->pp->counter++;
            snprintf(buf, sizeof(buf), "%d", t);
            t = TOK_PPNUM;
            goto add_cstr1;

        } else if (v == TOK___FILE__) {
            cstrval = tcc_state->pp->source->filename;
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
            tok_str_add2_spc(tok_str, t, &cval);
        }
        return 0;
    }
}

/* do macro substitution of macro_str and add result to
   (tok_str,tok_len). 'nested_list' is the list of all macros we got
   inside to avoid recursing. */
static int macro_subst(
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
    PP_PRINT(("+expand:", 0, macro_str));
#endif

    while (1) {
        TOK_GET(&t, &macro_str, &cval);
        if (t == 0 || t == TOK_EOF)
            break;
        if (t >= TOK_IDENT) {
            s = define_find(t);
            if (s == NULL || nosubst)
                goto no_subst;
            /* if nested substitution, do nothing */
            if (sym_find2(*nested_list, t)) {
                /* and mark so it doesn't get subst'd again */
                t |= SYM_FIELD;
                goto no_subst;
            }
            str = tok_str_alloc();
            str->str = (int*)macro_str; /* setup stream for possible arguments */
            begin_macro(str, 2);
            nosubst = macro_subst_tok(tok_str, nested_list, s);
            if (tcc_state->pp->macro_stack != str) {
                /* already finished by reading function macro arguments */
                break;
            }
            macro_str = tcc_state->pp->macro_ptr;
            end_macro ();
        } else if (t == ' ') {
            if (tcc_state->pp->parse_flags & PARSE_FLAG_SPACES)
                tok_str->need_spc |= 1;
        } else {
    no_subst:
            tok_str_add2_spc(tok_str, t, &cval);
            if (nosubst && t != '(')
                nosubst = 0;
            /* GCC supports 'defined' as result of a macro substitution */
            if (t == TOK_DEFINED && tcc_state->pp->expression)
                nosubst = 1;
        }
    }

#ifdef PP_DEBUG
    tok_str_add(tok_str, 0), --tok_str->len;
    PP_PRINT(("-result:", 0, tok_str->str + tlen));
#endif
    return nosubst;
}

/* return next token with macro substitution */
ST_FUNC void next(void)
{
    int t;
    while (tcc_state->pp->macro_ptr) {
redo:
        t = *tcc_state->pp->macro_ptr;
        if (TOK_HAS_VALUE(t)) {
            tok_get(&tcc_state->pp->token, &tcc_state->pp->macro_ptr, &tcc_state->pp->value);
            if (t == TOK_LINENUM) {
                tcc_state->pp->source->line_num = tcc_state->pp->value.i;
                goto redo;
            }
            goto convert;
        } else if (t == 0) {
            /* end of macro or unget token string */
            end_macro();
            continue;
        } else if (t == TOK_EOF) {
            /* do nothing */
        } else {
            ++tcc_state->pp->macro_ptr;
            t &= ~SYM_FIELD; /* remove 'nosubst' marker */
            if (t == '\\') {
                if (!(tcc_state->pp->parse_flags & PARSE_FLAG_ACCEPT_STRAYS))
                    tcc_error("stray '\\' in program");
            }
        }
        tcc_state->pp->token = t;
        return;
    }

    next_nomacro();
    t = tcc_state->pp->token;
    if (t >= TOK_IDENT && (tcc_state->pp->parse_flags & PARSE_FLAG_PREPROCESS)) {
        /* if reading from file, try to substitute macros */
        struct neocc_symbol *s = define_find(t);
        if (s) {
            struct neocc_symbol *nested_list = NULL;
            macro_subst_tok(&tcc_state->pp->expansion, &nested_list, s);
            tok_str_add(&tcc_state->pp->expansion, 0);
            begin_macro(&tcc_state->pp->expansion, 0);
            goto redo;
        }
        return;
    }

convert:
    /* convert preprocessor tokens into C tokens */
    if (t == TOK_PPNUM) {
        if  (tcc_state->pp->parse_flags & PARSE_FLAG_TOK_NUM)
            parse_number(tcc_state->pp->value.str.data);
    } else if (t == TOK_PPSTR) {
        if (tcc_state->pp->parse_flags & PARSE_FLAG_TOK_STR)
            parse_string(tcc_state->pp->value.str.data, tcc_state->pp->value.str.size - 1);
    }
}

/* push back current token and set current token to 'last_tok'. Only
   identifier case handled for labels. */
ST_INLN void unget_tok(int last_tok)
{
    struct neocc_token_string *str = &tcc_state->pp->unget;
    int alloc = 0;
    if (str->len) /* use static buffer except if already in use */
        str = tok_str_alloc(), alloc = 1;
    if (tcc_state->pp->token != TOK_EOF)
        tok_str_add2(str, tcc_state->pp->token, &tcc_state->pp->value);
    tok_str_add(str, 0);
    begin_macro(str, alloc);
    tcc_state->pp->token = last_tok;
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

static void putdef(struct neocc_string *cs, const char *p)
{
    cstr_printf(cs, "#define %s%s\n", p, &" 1"[!!strchr(p, ' ')*2]);
}

static void putdefs(struct neocc_string *cs, const char *p)
{
    while (*p)
        putdef(cs, p), p = strchr(p, 0) + 1;
}

static void tcc_predefs(TCCState *s1, struct neocc_string *cs, int is_asm)
{
    cstr_printf(cs, "#define __TINYC__ 9%.2s\n", &TCC_VERSION[4]);
    putdefs(cs, target_machine_defs);
    putdefs(cs, target_os_defs);

#ifdef TCC_TARGET_ARM
    if (s1->float_abi == ARM_HARD_FLOAT)
      putdef(cs, "__ARM_PCS_VFP");
#endif
    if (is_asm)
      putdef(cs, "__ASSEMBLER__");
    if (s1->output_type == TCC_OUTPUT_PREPROCESS)
      putdef(cs, "__TCC_PP__");
    if (s1->output_type == TCC_OUTPUT_MEMORY)
      putdef(cs, "__TCC_RUN__");
#ifdef CONFIG_TCC_BACKTRACE
    if (s1->do_backtrace)
      putdef(cs, "__TCC_BACKTRACE__");
#endif
#ifdef CONFIG_TCC_BCHECK
    if (s1->do_bounds_check)
      putdef(cs, "__TCC_BCHECK__");
#endif
    if (s1->char_is_unsigned)
      putdef(cs, "__CHAR_UNSIGNED__");
    if (s1->optimize > 0)
      putdef(cs, "__OPTIMIZE__");
    if (s1->option_pthread)
      putdef(cs, "_REENTRANT");
    if (s1->leading_underscore)
      putdef(cs, "__leading_underscore");
    cstr_printf(cs, "#define __SIZEOF_POINTER__ %d\n", PTR_SIZE);
    cstr_printf(cs, "#define __SIZEOF_LONG__ %d\n", LONG_SIZE);
    if (!is_asm) {
      putdef(cs, "__STDC__");
      cstr_printf(cs, "#define __STDC_HOSTED__ %d\n", s1->nostdlib ? 0 : 1);
      cstr_printf(cs, "#define __STDC_VERSION__ %dL\n", s1->cversion);
      cstr_cat(cs,
        /* load more predefs and __builtins */
#if CONFIG_TCC_PREDEFS
        #include "tccdefs_.h" /* include as strings */
#else
        "#include <tccdefs.h>\n" /* load at runtime */
#endif
        , -1);
    }
    cstr_printf(cs, "#define __BASE_FILE__ \"%s\"\n", s1->pp->source->filename);
}

ST_FUNC void preprocess_start(TCCState *s1, int filetype)
{
    int is_asm = !!(filetype & (AFF_TYPE_ASM|AFF_TYPE_ASMPP));

    s1->include_stack_ptr = s1->include_stack;
    s1->ifdef_stack_ptr = s1->ifdef_stack;
    s1->pp->source->ifdef_stack_ptr = s1->ifdef_stack_ptr;
    s1->pp->expression = 0;
    s1->pp->counter = 0;
    s1->pp->debug_token = s1->pp->debug_symbol = 0;
    s1->pack_stack[0] = 0;
    s1->pack_stack_ptr = s1->pack_stack;

    set_idnum('$', s1->dollars_in_identifiers ? NEOCC_CHAR_ID : 0);
    set_idnum('.', is_asm ? NEOCC_CHAR_ID : 0);

    if (!(filetype & AFF_TYPE_ASM)) {
        struct neocc_string *cstr = &s1->pp->predefs;
        cstr_reset(cstr);
        tcc_predefs(s1, cstr, is_asm);
        if (s1->cmdline_defs.size)
          cstr_cat(cstr, s1->cmdline_defs.data, s1->cmdline_defs.size);
        if (s1->cmdline_incl.size)
          cstr_cat(cstr, s1->cmdline_incl.data, s1->cmdline_incl.size);
        *s1->include_stack_ptr++ = s1->pp->source;
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
        tcc_close();
    tccpp_delete(s1);
}

ST_FUNC int set_idnum(int c, int val)
{
    int prev = tcc_state->pp->character[c - CH_EOF];
    tcc_state->pp->character[c - CH_EOF] = val;
    return prev;
}

ST_FUNC void tccpp_new(TCCState *s)
{
    int c;
    const char *p, *r;

    memset(s->cached_includes_hash, 0, sizeof s->cached_includes_hash);

    cstr_realloc(&tcc_state->pp->spelling, STRING_MAX_SIZE);
    tok_str_realloc(&tcc_state->pp->unget, TOKSTR_MAX_SIZE);
    tok_str_realloc(&tcc_state->pp->expansion, TOKSTR_MAX_SIZE);

    p = tcc_keywords;
    while (*p) {
        r = p;
        for(;;) {
            c = *r++;
            if (c == '\0')
                break;
        }
        tok_alloc(p, r - p - 1);
        p = r;
    }

    /* we add dummy defines for some special macros to speed up tests
       and to have working defined() */
    define_push(TOK___LINE__, MACRO_OBJ, NULL, NULL);
    define_push(TOK___FILE__, MACRO_OBJ, NULL, NULL);
    define_push(TOK___DATE__, MACRO_OBJ, NULL, NULL);
    define_push(TOK___TIME__, MACRO_OBJ, NULL, NULL);
    define_push(TOK___COUNTER__, MACRO_OBJ, NULL, NULL);
}

ST_FUNC void tccpp_delete(TCCState *s)
{
    int n;

    dynarray_reset(&s->cached_includes, &s->nb_cached_includes);

    /* free tokens */
    n = neocc_token_next(tcc_state) - TOK_IDENT;
    if (n > total_idents)
        total_idents = n;
    /* The preprocessor drops arena references before tokens releases them. */
    if (!RP(vo_interact0(s->preprocessor, NEOCC_AC0_PREPROCESSOR_RESET,
                         &neocc_host_caller)))
        tcc_error("cannot reset preprocessor");

    if (!RP(vo_interact0(s->tokens, NEOCC_AC0_TOKENS_RESET,
                         &neocc_host_caller)))
        tcc_error("cannot reset tokens");
}

/* ------------------------------------------------------------------------- */
/* tcc -E [-P[1]] [-dD} support */

static int pp_need_space(int a, int b);

static void tok_print(const int *str, const char *msg, ...)
{
    FILE *fp = tcc_state->ppfp;
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
	fprintf(fp, &" %s"[s], t == TOK_PLCHLDR ? "<>" : get_tok_str(t, &cval));
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

    s = define_find(v);
    if (NULL == s || NULL == s->d)
        return;

    fp = s1->ppfp;
    fprintf(fp, "#define %s", get_tok_str(v, NULL));
    if (s->type.t & MACRO_FUNC) {
        struct neocc_symbol *a = s->next;
        fprintf(fp,"(");
        if (a)
            for (;;) {
                fprintf(fp,"%s", get_tok_str(a->v, NULL));
                if (!(a = a->next))
                    break;
                fprintf(fp,",");
            }
        fprintf(fp,")");
    }
    tok_print(s->d, "");
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
    vs = get_tok_str(v, NULL);
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
	do next(); while (s1->pp->token != TOK_EOF);
	return 0;
    }

    token_seen = TOK_LINEFEED, spcs = 0, level = 0;
    if (s1->pp->source->prev)
        pp_line(s1, s1->pp->source->prev, level++);
    pp_line(s1, s1->pp->source, level);

    for (;;) {
        iptr = s1->include_stack_ptr;
        next();
        if (s1->pp->token == TOK_EOF)
            break;

        level = s1->include_stack_ptr - iptr;
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
        fputs(p = get_tok_str(s1->pp->token, &s1->pp->value), s1->ppfp);
        token_seen = pp_check_he0xE(s1->pp->token, p);
    }
    return 0;
}

/* ------------------------------------------------------------------------- */
