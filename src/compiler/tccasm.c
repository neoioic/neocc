/*
 *  GAS like assembler for TCC
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
#ifdef CONFIG_TCC_ASM

static Section *last_text_section; /* to handle .previous asm directive */
static int asmgoto_n;

static int tcc_assemble_internal(TCCState *s1, int do_preprocess, int global);
static struct neocc_symbol* asm_new_label(TCCState *s1, int label, int is_local);
static struct neocc_symbol* asm_new_label1(TCCState *s1, int label, int is_local, int sh_num, int value);

#if PTR_SIZE == 8
/* output constant with relocation if 'r & VT_SYM' is true */
ST_FUNC void gen_addr64(int r, struct neocc_symbol *sym, int64_t c)
{
    if (r & VT_SYM)
        greloca(tcc_state->unit->section, sym, tcc_state->unit->instruction, R_DATA_PTR, c), c=0;
    gen_le32(c);
    gen_le32(c>>32);
}

ST_FUNC void gen_expr64(ExprValue *pe)
{
    gen_addr64(pe->sym ? VT_SYM : 0, pe->sym, pe->v);
}
#endif

static int asm_get_prefix_name(TCCState *s1, const char *prefix, unsigned int n)
{
    char buf[64];
    snprintf(buf, sizeof(buf), "%s%u", prefix, n);
    return tok_alloc_const(s1, buf);
}

ST_FUNC int asm_get_local_label_name(TCCState *s1, unsigned int n)
{
    return asm_get_prefix_name(s1, "L..", n);
}

/* If a C name has an _ prepended then only asm labels that start
   with _ are representable in C, by removing the first _.  ASM names
   without _ at the beginning don't correspond to C names, but we use
   the global C symbol table to track ASM names as well, so we need to
   transform those into ones that don't conflict with a C name,
   so prepend a '.' for them, but force the ELF asm name to be set.  */
static int asm2cname(int v, int *addeddot)
{
    const char *name;
    *addeddot = 0;
    if (!tcc_state->leading_underscore)
      return v;
    name = get_tok_str(tcc_state, v, NULL);
    if (!name)
      return v;
    if (name[0] == '_') {
        v = tok_alloc_const(tcc_state, name + 1);
    } else if (!strchr(name, '.')) {
        char newname[256];
        snprintf(newname, sizeof newname, ".%s", name);
        v = tok_alloc_const(tcc_state, newname);
        *addeddot = 1;
    }
    return v;
}

static struct neocc_symbol *asm_label_find(int v)
{
    struct neocc_symbol *sym;
    int addeddot;
    v = asm2cname(v, &addeddot);
    sym = sym_find(v);
    while (sym && sym->sym_scope && !(sym->type.t & VT_STATIC))
        sym = sym->prev_tok;
    return sym;
}

static struct neocc_symbol *asm_label_push(int v)
{
    int addeddot, v2 = asm2cname(v, &addeddot);
    /* We always add VT_EXTERN, for sym definition that's tentative
       (for .set, removed for real defs), for mere references it's correct
       as is.  */
    struct neocc_symbol *sym = global_identifier_push(v2, VT_ASM | VT_EXTERN | VT_STATIC, 0);
    if (addeddot)
        sym->asm_label = v;
    return sym;
}

/* Return a symbol we can use inside the assembler, having name NAME.
   Symbols from asm and C source share a namespace.  If we generate
   an asm symbol it's also a (file-global) C symbol, but it's
   either not accessible by name (like "L.123"), or its type information
   is such that it's not usable without a proper C declaration.

   Sometimes we need symbols accessible by name from asm, which
   are anonymous in C, in this case CSYM can be used to transfer
   all information from that symbol to the (possibly newly created)
   asm symbol.  */
ST_FUNC struct neocc_symbol* get_asm_sym(int name, struct neocc_symbol *csym)
{
    struct neocc_symbol *sym = asm_label_find(name);
    if (!sym) {
	sym = asm_label_push(name);
	if (csym)
	  sym->c = csym->c;
    }
    return sym;
}

static struct neocc_symbol* asm_section_sym(TCCState *s1, Section *sec)
{
    char buf[100]; int label; struct neocc_symbol *sym;
    snprintf(buf, sizeof buf, "L.%s", sec->name);
    label = tok_alloc_const(s1, buf);
    sym = asm_label_find(label);
    return sym ? sym : asm_new_label1(s1, label, 1, sec->sh_num, 0);
}

/* We do not use the C expression parser to handle symbols. Maybe the
   C expression parser could be tweaked to do so. */

static void asm_expr_unary(TCCState *s1, ExprValue *pe)
{
    struct neocc_symbol *sym;
    int op, label;
    uint64_t n;
    const char *p;

    switch(s1->pp->token) {
    case TOK_PPNUM:
        p = s1->pp->value.str.data;
        n = strtoull(p, (char **)&p, 0);
        if (*p == 'b' || *p == 'f') {
            /* backward or forward label */
            label = asm_get_local_label_name(s1, n);
            sym = asm_label_find(label);
            if (*p == 'b') {
                /* backward : find the last corresponding defined label */
                if (sym && (!sym->c || elfsym(sym)->st_shndx == SHN_UNDEF))
                    sym = sym->prev_tok;
                if (!sym)
                    tcc_error("local label '%d' not found backward", (int)n);
            } else {
                /* forward */
                if (!sym || (sym->c && elfsym(sym)->st_shndx != SHN_UNDEF)) {
                    /* if the last label is defined, then define a new one */
		    sym = asm_label_push(label);
                }
            }
	    pe->v = 0;
	    pe->sym = sym;
	    pe->pcrel = 0;
        } else if (*p == '\0') {
            pe->v = n;
            pe->sym = NULL;
	    pe->pcrel = 0;
        } else {
            tcc_error("invalid number syntax");
        }
        next(s1);
        break;
    case '+':
        next(s1);
        asm_expr_unary(s1, pe);
        break;
    case '-':
    case '~':
        op = s1->pp->token;
        next(s1);
        asm_expr_unary(s1, pe);
        if (pe->sym)
            tcc_error("invalid operation with label");
        if (op == '-')
            pe->v = -pe->v;
        else
            pe->v = ~pe->v;
        break;
    case TOK_CCHAR:
    case TOK_LCHAR:
	pe->v = s1->pp->value.i;
	pe->sym = NULL;
	pe->pcrel = 0;
	next(s1);
	break;
    case '(':
        next(s1);
        asm_expr(s1, pe);
        skip(s1, ')');
        break;
    case '.':
        pe->v = tcc_state->unit->instruction;
        pe->sym = asm_section_sym(s1, tcc_state->unit->section);
        pe->pcrel = 0;
        next(s1);
        break;
    default:
        if (s1->pp->token >= TOK_IDENT) {
	    ElfSym *esym;
            /* label case : if the label was not found, add one */
	    sym = get_asm_sym(s1->pp->token, NULL);
	    esym = elfsym(sym);
            if (esym && esym->st_shndx == SHN_ABS) {
                /* if absolute symbol, no need to put a symbol value */
                pe->v = esym->st_value;
                pe->sym = NULL;
		pe->pcrel = 0;
            } else {
                pe->v = 0;
                pe->sym = sym;
		pe->pcrel = 0;
            }
            next(s1);
        } else {
            tcc_error("bad expression syntax [%s]", get_tok_str(s1, s1->pp->token, &s1->pp->value));
        }
        break;
    }
}
    
static void asm_expr_prod(TCCState *s1, ExprValue *pe)
{
    int op;
    ExprValue e2;

    asm_expr_unary(s1, pe);
    for(;;) {
        op = s1->pp->token;
        if (op != '*' && op != '/' && op != '%' && 
            op != TOK_SHL && op != TOK_SAR)
            break;
        next(s1);
        asm_expr_unary(s1, &e2);
        if (pe->sym || e2.sym)
            tcc_error("invalid operation with label");
        switch(op) {
        case '*':
            pe->v *= e2.v;
            break;
        case '/':  
            if (e2.v == 0) {
            div_error:
                tcc_error("division by zero");
            }
            pe->v /= e2.v;
            break;
        case '%':  
            if (e2.v == 0)
                goto div_error;
            pe->v %= e2.v;
            break;
        case TOK_SHL:
            pe->v <<= e2.v;
            break;
        default:
        case TOK_SAR:
            pe->v >>= e2.v;
            break;
        }
    }
}

static void asm_expr_logic(TCCState *s1, ExprValue *pe)
{
    int op;
    ExprValue e2;

    asm_expr_prod(s1, pe);
    for(;;) {
        op = s1->pp->token;
        if (op != '&' && op != '|' && op != '^')
            break;
        next(s1);
        asm_expr_prod(s1, &e2);
        if (pe->sym || e2.sym)
            tcc_error("invalid operation with label");
        switch(op) {
        case '&':
            pe->v &= e2.v;
            break;
        case '|':  
            pe->v |= e2.v;
            break;
        default:
        case '^':
            pe->v ^= e2.v;
            break;
        }
    }
}

static inline void asm_expr_sum(TCCState *s1, ExprValue *pe)
{
    int op;
    ExprValue e2;

    asm_expr_logic(s1, pe);
    for(;;) {
        op = s1->pp->token;
        if (op != '+' && op != '-')
            break;
        next(s1);
        asm_expr_logic(s1, &e2);
        if (op == '+') {
            if (pe->sym != NULL && e2.sym != NULL)
                goto cannot_relocate;
            pe->v += e2.v;
            if (pe->sym == NULL && e2.sym != NULL)
                pe->sym = e2.sym;
        } else {
            pe->v -= e2.v;
            /* NOTE: we are less powerful than gas in that case
               because we store only one symbol in the expression */
	    if (!e2.sym) {
		/* OK */
	    } else if (pe->sym == e2.sym) { 
		/* OK */
		pe->sym = NULL; /* same symbols can be subtracted to NULL */
	    } else {
		ElfSym *esym1, *esym2;
		esym1 = elfsym(pe->sym);
		esym2 = elfsym(e2.sym);
		if (!esym2)
		    goto cannot_relocate;
		if (esym1 && esym1->st_shndx == esym2->st_shndx
		    && esym1->st_shndx != SHN_UNDEF) {
		    /* we also accept defined symbols in the same section */
		    pe->v += (int)(esym1->st_value - esym2->st_value);
		    pe->sym = NULL;
		} else if (esym2->st_shndx == tcc_state->unit->section->sh_num) {
		    /* When subtracting a defined symbol in current section
		       this actually makes the value PC-relative.  */
		    pe->v += (int)(0 - esym2->st_value);
		    pe->pcrel = 1;
		    e2.sym = NULL;
		} else {
cannot_relocate:
		    tcc_error("invalid operation with label");
		}
	    }
        }
    }
}

static inline void asm_expr_cmp(TCCState *s1, ExprValue *pe)
{
    int op;
    ExprValue e2;

    asm_expr_sum(s1, pe);
    for(;;) {
        op = s1->pp->token;
	if (op != TOK_EQ && op != TOK_NE
	    && (op > TOK_GT || op < TOK_ULE))
            break;
        next(s1);
        asm_expr_sum(s1, &e2);
        if (pe->sym || e2.sym)
            tcc_error("invalid operation with label");
        switch(op) {
	case TOK_EQ:
	    pe->v = pe->v == e2.v;
	    break;
	case TOK_NE:
	    pe->v = pe->v != e2.v;
	    break;
	case TOK_LT:
	    pe->v = (int64_t)pe->v < (int64_t)e2.v;
	    break;
	case TOK_GE:
	    pe->v = (int64_t)pe->v >= (int64_t)e2.v;
	    break;
	case TOK_LE:
	    pe->v = (int64_t)pe->v <= (int64_t)e2.v;
	    break;
	case TOK_GT:
	    pe->v = (int64_t)pe->v > (int64_t)e2.v;
	    break;
        default:
            break;
        }
	/* GAS compare results are -1/0 not 1/0.  */
	pe->v = -(int64_t)pe->v;
    }
}

ST_FUNC void asm_expr(TCCState *s1, ExprValue *pe)
{
    asm_expr_cmp(s1, pe);
}

ST_FUNC int asm_int_expr(TCCState *s1)
{
    ExprValue e;
    asm_expr(s1, &e);
    if (e.sym)
        expect(s1, "constant");
    if ((int)e.v != e.v)
	tcc_error("integer out of range %lld", (long long)e.v);
    return e.v;
}

static struct neocc_symbol* asm_new_label1(TCCState *s1, int label, int is_local,
                           int sh_num, int value)
{
    struct neocc_symbol *sym;
    ElfSym *esym;

    sym = asm_label_find(label);
    if (sym) {
	esym = elfsym(sym);
	/* A VT_EXTERN symbol, even if it has a section is considered
	   overridable.  This is how we "define" .set targets.  Real
	   definitions won't have VT_EXTERN set.  */
        if (esym && esym->st_shndx != SHN_UNDEF) {
            /* the label is already defined */
            if (IS_ASM_SYM(sym)
                && (is_local == 1 || (sym->type.t & VT_EXTERN)))
                goto new_label;
            if (!(sym->type.t & VT_EXTERN))
                tcc_error("assembler label '%s' already defined",
                          get_tok_str(s1, label, NULL));
        }
    } else {
    new_label:
        sym = asm_label_push(label);
    }
    if (!sym->c)
      put_extern_sym2(sym, SHN_UNDEF, 0, 0, 1);
    esym = elfsym(sym);
    esym->st_shndx = sh_num;
    esym->st_value = value;
    if (is_local != 2)
        sym->type.t &= ~VT_EXTERN;
    return sym;
}

static struct neocc_symbol* asm_new_label(TCCState *s1, int label, int is_local)
{
    return asm_new_label1(s1, label, is_local, tcc_state->unit->section->sh_num, tcc_state->unit->instruction);
}

/* Set the value of LABEL to that of some expression (possibly
   involving other symbols).  LABEL can be overwritten later still.  */
static struct neocc_symbol* set_symbol(TCCState *s1, int label)
{
    long n;
    ExprValue e;
    struct neocc_symbol *sym;
    ElfSym *esym;
    next(s1);
    asm_expr(s1, &e);
    n = e.v;
    esym = elfsym(e.sym);
    if (esym)
	n += esym->st_value;
    sym = asm_new_label1(s1, label, 2, esym ? esym->st_shndx : SHN_ABS, n);
    elfsym(sym)->st_other |= ST_ASM_SET;
    return sym;
}

static void use_section1(TCCState *s1, Section *sec)
{
    tcc_state->unit->section->data_offset = tcc_state->unit->instruction;
    tcc_state->unit->section = sec;
    tcc_state->unit->instruction = tcc_state->unit->section->data_offset;
}

static void use_section(TCCState *s1, const char *name)
{
    Section *sec;
    sec = find_section(s1, name);
    use_section1(s1, sec);
}

static void push_section(TCCState *s1, const char *name)
{
    Section *sec = find_section(s1, name);
    sec->prev = tcc_state->unit->section;
    use_section1(s1, sec);
}

static void pop_section(TCCState *s1)
{
    Section *prev = tcc_state->unit->section->prev;
    if (!prev)
        tcc_error(".popsection without .pushsection");
    tcc_state->unit->section->prev = NULL;
    use_section1(s1, prev);
}

static void asm_parse_directive(TCCState *s1, int global)
{
    int n, offset, v, size, tok1, c;
    Section *sec;
    uint8_t *ptr;

    /* assembler directive */
    sec = tcc_state->unit->section;
    switch(s1->pp->token) {
    case TOK_ASMDIR_align:
    case TOK_ASMDIR_balign:
    case TOK_ASMDIR_p2align:
    case TOK_ASMDIR_skip:
    case TOK_ASMDIR_space:
        tok1 = s1->pp->token;
        next(s1);
        n = asm_int_expr(s1);
        if (tok1 == TOK_ASMDIR_p2align)
        {
            if (n < 0 || n > 30)
                tcc_error("invalid p2align, must be between 0 and 30");
            n = 1 << n;
            tok1 = TOK_ASMDIR_align;
        }
        if (tok1 == TOK_ASMDIR_align || tok1 == TOK_ASMDIR_balign) {
            if (n <= 0 || (n & (n-1)) != 0)
                tcc_error("alignment must be a positive power of two");
            offset = (tcc_state->unit->instruction + n - 1) & -n;
            size = offset - tcc_state->unit->instruction;
            /* the section must have a compatible alignment */
            if (sec->sh_addralign < n)
                sec->sh_addralign = n;
            c = sec->sh_flags & SHF_EXECINSTR;
        } else {
	    if (n < 0)
	        n = 0;
            size = n, c = 0;
        }
        v = 0;
        if (s1->pp->token == ',') {
            next(s1);
            v = asm_int_expr(s1), c = 0;
        }
    zero_pad:
	if ((uint64_t)tcc_state->unit->instruction + size >= 1<<30)
	    tcc_error("too much data");
        if (sec->sh_type != SHT_NOBITS) {
            if (c) {
                gen_fill_nops(size);
                break;
            }
            sec->data_offset = tcc_state->unit->instruction;
            ptr = section_ptr_add(sec, size);
            memset(ptr, v, size);
        }
        tcc_state->unit->instruction += size;
        break;
    case TOK_ASMDIR_quad:
#if PTR_SIZE == 8
	size = 8;
	goto asm_data;
#else
        next(s1);
        for(;;) {
            uint64_t vl;
            const char *p;

            p = s1->pp->value.str.data;
            if (s1->pp->token != TOK_PPNUM) {
            error_constant:
                tcc_error("64 bit constant");
            }
            vl = strtoll(p, (char **)&p, 0);
            if (*p != '\0')
                goto error_constant;
            next(s1);
            if (sec->sh_type != SHT_NOBITS) {
                /* XXX: endianness */
                gen_le32(vl);
                gen_le32(vl >> 32);
            } else {
                tcc_state->unit->instruction += 8;
            }
            if (s1->pp->token != ',')
                break;
            next(s1);
        }
        break;
#endif
    case TOK_ASMDIR_byte:
        size = 1;
        goto asm_data;
    case TOK_ASMDIR_word:
    case TOK_ASMDIR_short:
        size = 2;
        goto asm_data;
    case TOK_ASMDIR_long:
    case TOK_ASMDIR_int:
        size = 4;
    asm_data:
        next(s1);
        for(;;) {
            ExprValue e;
            asm_expr(s1, &e);
            if (sec->sh_type != SHT_NOBITS) {
                if (size == 4) {
                    gen_expr32(&e);
#if PTR_SIZE == 8
		} else if (size == 8) {
		    gen_expr64(&e);
#endif
                } else {
                    if (e.sym)
                        expect(s1, "constant");
                    if (size == 1)
                        g(e.v);
                    else
                        gen_le16(e.v);
                }
            } else {
                tcc_state->unit->instruction += size;
            }
            if (s1->pp->token != ',')
                break;
            next(s1);
        }
        break;
    case TOK_ASMDIR_fill:
        {
            int repeat, size, val, i, j;
            uint8_t repeat_buf[8];
            next(s1);
            repeat = asm_int_expr(s1);
            if (repeat < 0) {
                tcc_error("repeat < 0; .fill ignored");
                break;
            }
            size = 1;
            val = 0;
            if (s1->pp->token == ',') {
                next(s1);
                size = asm_int_expr(s1);
                if (size < 0) {
                    tcc_error("size < 0; .fill ignored");
                    break;
                }
                if (size > 8)
                    size = 8;
                if (s1->pp->token == ',') {
                    next(s1);
                    val = asm_int_expr(s1);
                }
            }
            /* XXX: endianness */
            repeat_buf[0] = val;
            repeat_buf[1] = val >> 8;
            repeat_buf[2] = val >> 16;
            repeat_buf[3] = val >> 24;
            repeat_buf[4] = 0;
            repeat_buf[5] = 0;
            repeat_buf[6] = 0;
            repeat_buf[7] = 0;
            for(i = 0; i < repeat; i++) {
                for(j = 0; j < size; j++) {
                    g(repeat_buf[j]);
                }
            }
        }
        break;
    case TOK_ASMDIR_rept:
        {
            int repeat;
            struct neocc_token_string *init_str;
            next(s1);
            repeat = asm_int_expr(s1);
            init_str = tok_str_alloc(s1);
            while (next(s1), s1->pp->token != TOK_ASMDIR_endr) {
                if (s1->pp->token == CH_EOF)
                    tcc_error("we at end of file, .endr not found");
                tok_str_add_tok(s1, init_str);
            }
            tok_str_add(s1, init_str, TOK_EOF);
            begin_macro(s1, init_str, 1);
            while (repeat-- > 0) {
                tcc_assemble_internal(s1, (s1->pp->parse_flags & PARSE_FLAG_PREPROCESS),
				      global);
                s1->pp->macro_ptr = init_str->str;
            }
            end_macro(s1);
            next(s1);
            break;
        }
    case TOK_ASMDIR_org:
        {
	    ExprValue e;
	    ElfSym *esym;
            next(s1);
	    asm_expr(s1, &e);
	    n = e.v;
	    esym = elfsym(e.sym);
	    if (esym) {
		if (esym->st_shndx != tcc_state->unit->section->sh_num)
		  expect(s1, "constant or same-section symbol");
		n += esym->st_value;
	    }
            if (n < tcc_state->unit->instruction)
                tcc_error("attempt to .org backwards");
            v = c = 0;
            size = n - tcc_state->unit->instruction;
            goto zero_pad;
        }
        break;
    case TOK_ASMDIR_set:
	next(s1);
	tok1 = s1->pp->token;
	next(s1);
	/* Also accept '.set stuff', but don't do anything with this.
	   It's used in GAS to set various features like '.set mips16'.  */
	if (s1->pp->token == ',')
	    set_symbol(s1, tok1);
	break;
    case TOK_ASMDIR_globl:
    case TOK_ASMDIR_global:
    case TOK_ASMDIR_weak:
    case TOK_ASMDIR_hidden:
	tok1 = s1->pp->token;
	do { 
            struct neocc_symbol *sym;
            next(s1);
	    if (s1->pp->token < TOK_IDENT)
		expect(s1, "identifier");
            sym = get_asm_sym(s1->pp->token, NULL);
	    if (tok1 != TOK_ASMDIR_hidden)
                sym->type.t &= ~VT_STATIC;
            if (tok1 == TOK_ASMDIR_weak)
                sym->a.weak = 1;
	    else if (tok1 == TOK_ASMDIR_hidden)
	        sym->a.visibility = STV_HIDDEN;
            update_storage(sym);
            next(s1);
	} while (s1->pp->token == ',');
	break;
    case TOK_ASMDIR_string:
    case TOK_ASMDIR_ascii:
    case TOK_ASMDIR_asciz:
        {
            const char *p;
            int i, size, t;

            t = s1->pp->token;
            next(s1);
            for(;;) {
                if (s1->pp->token != TOK_STR)
                    expect(s1, "string constant");
                p = s1->pp->value.str.data;
                size = s1->pp->value.str.size;
                if (t == TOK_ASMDIR_ascii && size > 0)
                    size--;
                for(i = 0; i < size; i++)
                    g(p[i]);
                next(s1);
                if (s1->pp->token == ',') {
                    next(s1);
                } else if (s1->pp->token != TOK_STR) {
                    break;
                }
            }
	}
	break;
    case TOK_ASMDIR_text:
    case TOK_ASMDIR_data:
    case TOK_ASMDIR_bss:
	{ 
            char sname[64];
            tok1 = s1->pp->token;
            n = 0;
            next(s1);
            if (s1->pp->token != ';' && s1->pp->token != TOK_LINEFEED) {
		n = asm_int_expr(s1);
		next(s1);
            }
            if (n)
                sprintf(sname, "%s%d", get_tok_str(s1, tok1, NULL), n);
            else
                sprintf(sname, "%s", get_tok_str(s1, tok1, NULL));
            use_section(s1, sname);
	}
	break;
    case TOK_ASMDIR_file:
        {
            const char *p;
            s1->pp->parse_flags &= ~PARSE_FLAG_TOK_STR;
            next(s1);
            if (s1->pp->token == TOK_PPNUM)
                next(s1);
            if (s1->pp->token == TOK_PPSTR && s1->pp->value.str.data[0] == '"') {
                s1->pp->value.str.data[s1->pp->value.str.size - 2] = 0;
                p = s1->pp->value.str.data + 1;
            } else if (s1->pp->token >= TOK_IDENT) {
                p = get_tok_str(s1, s1->pp->token, &s1->pp->value);
            } else {
                skip_to_eol(s1, 0);
                break;
            }
            tccpp_putfile(s1, p);
            next(s1);
        }
        break;
    case TOK_ASMDIR_ident:
        {
            char ident[256];

            ident[0] = '\0';
            next(s1);
            if (s1->pp->token == TOK_STR)
                pstrcat(ident, sizeof(ident), s1->pp->value.str.data);
            else
                pstrcat(ident, sizeof(ident), get_tok_str(s1, s1->pp->token, &s1->pp->value));
            tcc_warning_c(s1, warn_unsupported, "ignoring .ident %s", ident);
            next(s1);
        }
        break;
    case TOK_ASMDIR_size:
        { 
            struct neocc_symbol *sym;
            ElfSym *esym;

            next(s1);
	    if (s1->pp->token < TOK_IDENT)
		expect(s1, "identifier");
            sym = asm_label_find(s1->pp->token);
            if (!sym)
                tcc_error("label not found: %s", get_tok_str(s1, s1->pp->token, NULL));
            /* XXX .size name,label2-label1 */
            tcc_warning_c(s1, warn_unsupported, "ignoring .size %s,*", get_tok_str(s1, s1->pp->token, NULL));
            next(s1);
            skip(s1, ',');
            n = asm_int_expr(s1);
            esym = elfsym(sym);
            if (esym) {
                esym->st_size = n;
            }
        }
        break;
    case TOK_ASMDIR_type:
        { 
            struct neocc_symbol *sym;
            const char *newtype;
            int st_type;

            next(s1);
	    if (s1->pp->token < TOK_IDENT)
		expect(s1, "identifier");
            sym = get_asm_sym(s1->pp->token, NULL);
            next(s1);
            skip(s1, ',');
            if (s1->pp->token == TOK_STR) {
                newtype = s1->pp->value.str.data;
            } else {
                if (s1->pp->token == '@' || s1->pp->token == '%')
                    next(s1);
                newtype = get_tok_str(s1, s1->pp->token, NULL);
            }

            if (!strcmp(newtype, "function") || !strcmp(newtype, "STT_FUNC")) {
                if (IS_ASM_SYM(sym))
                    sym->type.t |= VT_ASM_FUNC;
                st_type = STT_FUNC;
            set_st_type:
                if (sym->c) {
                    ElfSym *esym = elfsym(sym);
                    esym->st_info = ELFW(ST_INFO)(ELFW(ST_BIND)(esym->st_info), st_type);
                }
            } else if (!strcmp(newtype, "object") || !strcmp(newtype, "STT_OBJECT")) {
                st_type = STT_OBJECT;
                goto set_st_type;
            } else
                tcc_warning_c(s1, warn_unsupported, "change type of '%s' from 0x%x to '%s' ignored",
                    get_tok_str(s1, sym->v, NULL), sym->type.t, newtype);

            next(s1);
        }
        break;
    case TOK_ASMDIR_pushsection:
    case TOK_ASMDIR_section:
        {
            char sname[256];
	    int old_nb_section = s1->nb_sections;
            int flags = SHF_ALLOC;

	    tok1 = s1->pp->token;
            /* XXX: support more options */
            next(s1);
            sname[0] = '\0';
            while (s1->pp->token != ';' && s1->pp->token != TOK_LINEFEED && s1->pp->token != ',') {
                if (s1->pp->token == TOK_STR)
                    pstrcat(sname, sizeof(sname), s1->pp->value.str.data);
                else
                    pstrcat(sname, sizeof(sname), get_tok_str(s1, s1->pp->token, NULL));
                next(s1);
            }
            if (s1->pp->token == ',') {
                const char *p;
                /* skip section options */
                next(s1);
                if (s1->pp->token != TOK_STR)
                    expect(s1, "string constant");
                for (p = s1->pp->value.str.data; *p; ++p) {
                    if (*p == 'w')
                        flags |= SHF_WRITE;
                    if (*p == 'x')
                        flags |= SHF_EXECINSTR;
                }
                next(s1);
                if (s1->pp->token == ',') {
                    next(s1);
                    if (s1->pp->token == '@' || s1->pp->token == '%')
                        next(s1);
                    next(s1);
                }
            }
            last_text_section = tcc_state->unit->section;
	    if (tok1 == TOK_ASMDIR_section)
	        use_section(s1, sname);
	    else
	        push_section(s1, sname);
	    /* If we just allocated a new section reset its alignment to
	       1.  new_section normally acts for GCC compatibility and
	       sets alignment to PTR_SIZE.  The assembler behaves different. */
	    if (old_nb_section != s1->nb_sections) {
	        tcc_state->unit->section->sh_addralign = 1;
                /* Make .init and .fini sections executable by default.
                   GAS does so, too, and musl relies on it. */
                if (!strcmp(sname, ".init") || !strcmp(sname, ".fini"))
                    flags |= SHF_EXECINSTR;
	        tcc_state->unit->section->sh_flags = flags;
            }
        }
        break;
    case TOK_ASMDIR_previous:
        { 
            Section *sec;
            next(s1);
            if (!last_text_section)
                tcc_error("no previous section referenced");
            sec = tcc_state->unit->section;
            use_section1(s1, last_text_section);
            last_text_section = sec;
        }
        break;
    case TOK_ASMDIR_popsection:
	next(s1);
	pop_section(s1);
	break;
#ifdef TCC_TARGET_I386
    case TOK_ASMDIR_code16:
        {
            next(s1);
            s1->seg_size = 16;
        }
        break;
    case TOK_ASMDIR_code32:
        {
            next(s1);
            s1->seg_size = 32;
        }
        break;
#endif
#if PTR_SIZE == 8
    /* added for compatibility with GAS */
    case TOK_ASMDIR_code64:
        next(s1);
        break;
#endif
#ifdef TCC_TARGET_RISCV64
    case TOK_ASMDIR_option:
        next(s1);
        switch(s1->pp->token){
            case TOK_ASM_rvc:    /* Will be deprecated soon in favor of arch */
            case TOK_ASM_norvc:  /* Will be deprecated soon in favor of arch */
            case TOK_ASM_pic:
            case TOK_ASM_nopic:
            case TOK_ASM_relax:
            case TOK_ASM_norelax:
            case TOK_ASM_push:
            case TOK_ASM_pop:
                /* TODO: unimplemented */
                next(s1);
                break;
            case TOK_ASM_arch:
                /* TODO: unimplemented, requires extra parsing */
                tcc_error("unimp .option '.%s'", get_tok_str(s1, s1->pp->token, NULL));
                break;
            default:
                tcc_error("unknown .option '.%s'", get_tok_str(s1, s1->pp->token, NULL));
                break;
        }
        break;
#endif
    /* TODO: Implement symvar support. FreeBSD >= 14 needs this */
    case TOK_ASMDIR_symver:
	next(s1);
	next(s1);
        skip(s1, ',');
	next(s1);
        skip(s1, '@');
	next(s1);
	break;
    case TOK_ASMDIR_reloc:
	{
	    ExprValue e;
	    const char *reloc_name;
	    int reloc_type = -1;

	    next(s1);
	    asm_expr(s1, &e);
	    skip(s1, ',');
	    reloc_name = get_tok_str(s1, s1->pp->token, NULL);
#if defined(TCC_TARGET_ARM64)
	    if (!strcmp(reloc_name, "R_AARCH64_CALL26"))
	        reloc_type = R_AARCH64_CALL26;
#elif defined(TCC_TARGET_RISCV64)
	    if (!strcmp(reloc_name, "R_RISCV_CALL") || !strcmp(reloc_name, "R_RISCV_CALL_PLT"))
	        reloc_type = R_RISCV_CALL;
	    else if (!strcmp(reloc_name, "R_RISCV_BRANCH"))
	        reloc_type = R_RISCV_BRANCH;
	    else if (!strcmp(reloc_name, "R_RISCV_JAL"))
	        reloc_type = R_RISCV_JAL;
	    else if (!strcmp(reloc_name, "R_RISCV_PCREL_HI20"))
	        reloc_type = R_RISCV_PCREL_HI20;
	    else if (!strcmp(reloc_name, "R_RISCV_PCREL_LO12_I"))
	        reloc_type = R_RISCV_PCREL_LO12_I;
	    else if (!strcmp(reloc_name, "R_RISCV_PCREL_LO12_S"))
	        reloc_type = R_RISCV_PCREL_LO12_S;
	    else if (!strcmp(reloc_name, "R_RISCV_32_PCREL"))
	        reloc_type = R_RISCV_32_PCREL;
	    else if (!strcmp(reloc_name, "R_RISCV_32"))
	        reloc_type = R_RISCV_32;
	    else if (!strcmp(reloc_name, "R_RISCV_64"))
	        reloc_type = R_RISCV_64;
#endif
	    if (reloc_type < 0)
	        tcc_error("unimp: reloc '%s' unknown", reloc_name);
	    next(s1);
	    skip(s1, ',');
	    greloca(tcc_state->unit->section, get_asm_sym(s1->pp->token, NULL), e.v, reloc_type, 0);
	    next(s1);
	}
	break;
    default:
        tcc_error("unknown assembler directive '.%s'", get_tok_str(s1, s1->pp->token, NULL));
        break;
    }
}


/* assemble a file */
static int tcc_assemble_internal(TCCState *s1, int do_preprocess, int global)
{
    int opcode;
    int saved_parse_flags = s1->pp->parse_flags;

    s1->pp->parse_flags = PARSE_FLAG_ASM_FILE | PARSE_FLAG_TOK_STR;
    if (do_preprocess)
        s1->pp->parse_flags |= PARSE_FLAG_PREPROCESS;
    for(;;) {
        next(s1);
        if (s1->pp->token == TOK_EOF)
            break;
        tcc_debug_line(s1);
        s1->pp->parse_flags |= PARSE_FLAG_LINEFEED; /* XXX: suppress that hack */
    redo:
#if !defined(TCC_TARGET_ARM64)
        if (s1->pp->token == '#') {
            /* horrible gas comment */
            while (s1->pp->token != TOK_LINEFEED)
                next(s1);
        } else
#endif
        if (s1->pp->token >= TOK_ASMDIR_FIRST && s1->pp->token <= TOK_ASMDIR_LAST) {
            asm_parse_directive(s1, global);
        } else if (s1->pp->token == TOK_PPNUM) {
            const char *p;
            int n;
            p = s1->pp->value.str.data;
            n = strtoul(p, (char **)&p, 10);
            if (*p != '\0')
                expect(s1, "':'");
            /* new local label */
            asm_new_label(s1, asm_get_local_label_name(s1, n), 1);
            next(s1);
            skip(s1, ':');
            goto redo;
        } else if (s1->pp->token >= TOK_IDENT) {
            /* instruction or label */
            opcode = s1->pp->token;
            next(s1);
            if (s1->pp->token == ':') {
                /* new label */
                asm_new_label(s1, opcode, 0);
                next(s1);
                goto redo;
            } else if (s1->pp->token == '=') {
		set_symbol(s1, opcode);
                goto redo;
            } else {
                asm_opcode(s1, opcode);
            }
        }
        /* end of line */
        if (s1->pp->token != ';' && s1->pp->token != TOK_LINEFEED)
            expect(s1, "end of line");
        s1->pp->parse_flags &= ~PARSE_FLAG_LINEFEED; /* XXX: suppress that hack */
    }

    s1->pp->parse_flags = saved_parse_flags;
    return 0;
}

/* Assemble the current file */
ST_FUNC int tcc_assemble(TCCState *s1, int do_preprocess)
{
    int ret;
    /* Standalone assembly uses absolute STABS line values, as the original
       zero-initialized function cursor did before C function parsing. */
    s1->function->start = 0;
    tcc_debug_start(s1);
    /* default section is text */
    tcc_state->unit->section = text_section;
    tcc_state->unit->instruction = tcc_state->unit->section->data_offset;
    tcc_state->unit->suppression = 0;
    ret = tcc_assemble_internal(s1, do_preprocess, 1);
    tcc_state->unit->section->data_offset = tcc_state->unit->instruction;
    tcc_debug_end(s1);
    return ret;
}

/********************************************************************/
/* GCC inline asm support */

/* assemble the string 'str' in the current C compilation unit without
   C preprocessing. */
static void tcc_assemble_inline(TCCState *s1, const char *str, int len, int global)
{
    const int *saved_macro_ptr = s1->pp->macro_ptr;
    int dotid = set_idnum(s1, '.', NEOCC_CHAR_ID);
#if !defined(TCC_TARGET_RISCV64) && !defined(TCC_TARGET_X86_64)
    int dolid = set_idnum(s1, '$', 0);
#endif

    tcc_open_bf(s1, ":asm:", len);
    memcpy(s1->pp->source->buffer, str, len);
    s1->pp->macro_ptr = NULL;
    tcc_assemble_internal(s1, 0, global);
    tcc_close(s1);

#if !defined(TCC_TARGET_RISCV64) && !defined(TCC_TARGET_X86_64)
    set_idnum(s1, '$', dolid);
#endif
    set_idnum(s1, '.', dotid);
    s1->pp->macro_ptr = saved_macro_ptr;
}

/* find a constraint by its number or id (gcc 3 extended
   syntax). return -1 if not found. Return in *pp in char after the
   constraint */
ST_FUNC int find_constraint(ASMOperand *operands, int nb_operands, 
                           const char *name, const char **pp)
{
    int index;
    struct neocc_token *ts;
    const char *p;

    if (isnum(*name)) {
        index = 0;
        while (isnum(*name)) {
            index = (index * 10) + (*name) - '0';
            name++;
        }
        if ((unsigned)index >= nb_operands)
            index = -1;
    } else if (*name == '[') {
        name++;
        p = strchr(name, ']');
        if (p) {
            ts = tok_alloc(tcc_state, name, p - name);
            for(index = 0; index < nb_operands; index++) {
                if (operands[index].id == ts->tok)
                    goto found;
            }
            index = -1;
        found:
            name = p + 1;
        } else {
            index = -1;
        }
    } else {
        index = -1;
    }
    if (pp)
        *pp = name;
    return index;
}

static void subst_asm_operands(ASMOperand *operands, int nb_operands, 
                               struct neocc_string *out_str, const char *str)
{
    int c, index, modifier;
    ASMOperand *op;
    struct neocc_expression_value sv;

    for(;;) {
        c = *str++;
        if (c == '%') {
            if (*str == '%') {
                str++;
                goto add_char;
            }
            modifier = 0;
            if (*str == 'c' || *str == 'n' ||
                *str == 'b' || *str == 'w' || *str == 'h' || *str == 'k' ||
		*str == 'q' || *str == 'l' ||
#ifdef TCC_TARGET_ARM64
                *str == 'x' || *str == 's' || *str == 'd' || *str == 'Z' ||
#endif
#ifdef TCC_TARGET_RISCV64
		*str == 'z' ||
#endif
		/* P in GCC would add "@PLT" to symbol refs in PIC mode,
		   and make literal operands not be decorated with '$'.  */
		*str == 'P')
                modifier = *str++;
            index = find_constraint(operands, nb_operands, str, &str);
            if (index < 0)
        error:
                tcc_error("invalid operand reference after %%");
            op = &operands[index];
            if (modifier == 'l') {
                cstr_cat(tcc_state, out_str, get_tok_str(tcc_state, op->is_label, NULL), -1);
            } else {
		if (op->vt == NULL)
		    goto error;
                sv = *op->vt;
                if (op->reg >= 0) {
                    sv.r = op->reg;
                    if (op->is_memory)
                      sv.r |= VT_LVAL;
                }
                subst_asm_operand(out_str, &sv, modifier);
            }
        } else {
        add_char:
            cstr_ccat(tcc_state, out_str, c);
            if (c == '\0')
                break;
        }
    }
}


static void parse_asm_operands(ASMOperand *operands, int *nb_operands_ptr,
                               int is_output)
{
    ASMOperand *op;
    int nb_operands;
    char* astr;

    if (tcc_state->pp->token != ':') {
        nb_operands = *nb_operands_ptr;
        for(;;) {
            if (nb_operands >= MAX_ASM_OPERANDS)
                tcc_error("too many asm operands");
            op = &operands[nb_operands++];
            op->id = 0;
            if (tcc_state->pp->token == '[') {
                next(tcc_state);
                if (tcc_state->pp->token < TOK_IDENT)
                    expect(tcc_state, "identifier");
                op->id = tcc_state->pp->token;
                next(tcc_state);
                skip(tcc_state, ']');
            }
	    astr = parse_mult_str("string constant")->data;
            pstrcpy(op->constraint, sizeof op->constraint, astr);
            skip(tcc_state, '(');
            gexpr();
            if (is_output) {
                if (!(tcc_state->expr->top->type.t & VT_ARRAY))
                    test_lvalue();
            } else {
                /* we want to avoid LLOCAL case, except when the 'm'
                   constraint is used. Note that it may come from
                   register storage, so we need to convert (reg)
                   case */
                if ((tcc_state->expr->top->r & VT_LVAL) &&
                    ((tcc_state->expr->top->r & VT_VALMASK) == VT_LLOCAL ||
                     (tcc_state->expr->top->r & VT_VALMASK) < VT_CONST) &&
                    !strchr(op->constraint, 'm')
#ifdef TCC_TARGET_ARM64
                    && !strchr(op->constraint, 'Q')
                    && !strstr(op->constraint, "Ump")
#endif
                    ) {
                    gv(RC_INT);
                }
            }
            op->vt = tcc_state->expr->top;
            skip(tcc_state, ')');
            if (tcc_state->pp->token == ',') {
                next(tcc_state);
            } else {
                break;
            }
        }
        *nb_operands_ptr = nb_operands;
    }
}

/* parse the GCC asm() instruction */
ST_FUNC void asm_instr(void)
{
    struct neocc_string *astr, *astr1;

    ASMOperand operands[MAX_ASM_OPERANDS];
    int nb_outputs, nb_operands, i, must_subst, out_reg, nb_labels;
    uint8_t clobber_regs[NB_ASM_REGS];
    Section *sec;

    /* since we always generate the asm() instruction, we can ignore
       volatile */
    while (tcc_state->pp->token == TOK_VOLATILE1 || tcc_state->pp->token == TOK_VOLATILE2 || tcc_state->pp->token == TOK_VOLATILE3
           || tcc_state->pp->token == TOK_GOTO) {
        next(tcc_state);
    }

    astr1 = parse_asm_str();
    astr = neocc_temp_new(tcc_state);
    cstr_cat(tcc_state, astr, astr1->data, astr1->size);

    nb_operands = 0;
    nb_outputs = 0;
    nb_labels = 0;
    must_subst = 0;
    memset(clobber_regs, 0, sizeof(clobber_regs));
    if (tcc_state->pp->token == ':') {
        next(tcc_state);
        must_subst = 1;
        /* output args */
        parse_asm_operands(operands, &nb_operands, 1);
        nb_outputs = nb_operands;
        if (tcc_state->pp->token == ':') {
            next(tcc_state);
            if (tcc_state->pp->token != ')') {
                /* input args */
                parse_asm_operands(operands, &nb_operands, 0);
                if (tcc_state->pp->token == ':') {
                    /* clobber list */
                    /* XXX: handle registers */
                    next(tcc_state);
                    for(;;) {
                        if (tcc_state->pp->token == ':')
                          break;
                        if (tcc_state->pp->token != TOK_STR)
                            expect(tcc_state, "string constant");
                        asm_clobber(clobber_regs, tcc_state->pp->value.str.data);
                        next(tcc_state);
                        if (tcc_state->pp->token == ',') {
                            next(tcc_state);
                        } else {
                            break;
                        }
                    }
                }
                if (tcc_state->pp->token == ':') {
                    /* goto labels */
                    next(tcc_state);
                    for (;;) {
                        struct neocc_symbol *csym;
                        int asmname;
                        if (nb_operands + nb_labels >= MAX_ASM_OPERANDS)
                          tcc_error("too many asm operands");
                        if (tcc_state->pp->token < TOK_UIDENT)
                          expect(tcc_state, "label identifier");
			memset(operands + nb_operands + nb_labels, 0,
			       sizeof(operands[0]));
                        operands[nb_operands + nb_labels++].id = tcc_state->pp->token;

                        csym = label_find(tcc_state->pp->token);
                        if (!csym) {
                            csym = label_push(NEOCC_SYMBOL_STACK_GLOBAL_LABEL,
                                              tcc_state->pp->token,
                                              LABEL_FORWARD);
                        } else {
                            if (csym->r == LABEL_DECLARED)
                              csym->r = LABEL_FORWARD;
                        }
                        next(tcc_state);
                        asmname = asm_get_prefix_name(tcc_state, "LG.",
                                                      ++asmgoto_n);
                        if (!csym->c)
                          put_extern_sym2(csym, SHN_UNDEF, 0, 0, 1);
                        get_asm_sym(asmname, csym);
                        operands[nb_operands + nb_labels - 1].is_label = asmname;

                        if (tcc_state->pp->token != ',')
                          break;
                        next(tcc_state);
                    }
                }
            }
        }
    }
    skip(tcc_state, ')');
    /* NOTE: we do not eat the ';' so that we can restore the current
       token after the assembler parsing */
    if (tcc_state->pp->token != ';')
        expect(tcc_state, "';'");
    
    /* save all values in the memory */
    save_regs(0);

    /* compute constraints */
    asm_compute_constraints(operands, nb_operands, nb_outputs, 
                            clobber_regs, &out_reg);

    /* substitute the operands in the asm string. No substitution is
       done if no operands (GCC behaviour) */
#ifdef ASM_DEBUG
    printf("asm: \"%s\"\n", (char *)astr->data);
#endif
    if (must_subst) {
        cstr_reset(astr1);
        cstr_cat(tcc_state, astr1, astr->data, astr->size);
        cstr_reset(astr);
        subst_asm_operands(operands, nb_operands + nb_labels, astr, astr1->data);
    }

#ifdef ASM_DEBUG
    printf("subst_asm: \"%s\"\n", (char *)astr->data);
#endif

    /* generate loads */
    asm_gen_code(operands, nb_operands, nb_outputs, 0, 
                 clobber_regs, out_reg);    

    /* We don't allow switching section within inline asm to
       bleed out to surrounding code.  */
    sec = tcc_state->unit->section;
    /* assemble the string with tcc internal assembler */
    tcc_assemble_inline(tcc_state, astr->data, astr->size - 1, 0);
    neocc_temp_free(tcc_state, astr);
    if (sec != tcc_state->unit->section) {
        tcc_warning("inline asm tries to change current section");
        use_section1(tcc_state, sec);
    }

    /* restore the current C token */
    next(tcc_state);

    /* store the output values if needed */
    asm_gen_code(operands, nb_operands, nb_outputs, 1, 
                 clobber_regs, out_reg);
    
    /* free everything */
    for(i=0;i<nb_operands;i++) {
        vpop();
    }

}

ST_FUNC void asm_global_instr(void)
{
    struct neocc_string *astr;
    int saved_nocode_wanted = tcc_state->unit->suppression;

    /* Global asm blocks are always emitted.  */
    tcc_state->unit->suppression = 0;
    next(tcc_state);
    astr = parse_asm_str();
    skip(tcc_state, ')');
    /* NOTE: we do not eat the ';' so that we can restore the current
       token after the assembler parsing */
    if (tcc_state->pp->token != ';')
        expect(tcc_state, "';'");
    
#ifdef ASM_DEBUG
    printf("asm_global: \"%s\"\n", (char *)astr->data);
#endif
    tcc_state->unit->section = text_section;
    tcc_state->unit->instruction = tcc_state->unit->section->data_offset;

    /* assemble the string with tcc internal assembler */
    tcc_assemble_inline(tcc_state, astr->data, astr->size - 1, 1);
    
    tcc_state->unit->section->data_offset = tcc_state->unit->instruction;

    /* restore the current C token */
    next(tcc_state);

    tcc_state->unit->suppression = saved_nocode_wanted;
}

/********************************************************/
#else
ST_FUNC int tcc_assemble(TCCState *s1, int do_preprocess)
{
    tcc_error("asm not supported");
}

ST_FUNC void asm_instr(void)
{
    tcc_error("inline asm() not supported");
}

ST_FUNC void asm_global_instr(void)
{
    tcc_error("inline asm() not supported");
}
#endif /* CONFIG_TCC_ASM */
