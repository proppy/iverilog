/*
 * Copyright (c) 2001-2002 Stephen Williams (steve@icarus.com)
 *
 *    This source code is free software; you can redistribute it
 *    and/or modify it in source code form under the terms of the GNU
 *    General Public License as published by the Free Software
 *    Foundation; either version 2 of the License, or (at your option)
 *    any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */
#ifdef HAVE_CVS_IDENT
#ident "$Id: eval_expr.c,v 1.134 2007/02/12 04:37:58 steve Exp $"
#endif

# include  "vvp_priv.h"
# include  <string.h>
#ifdef HAVE_MALLOC_H
# include  <malloc.h>
#endif
# include  <stdlib.h>
# include  <assert.h>

static void draw_eval_expr_dest(ivl_expr_t exp, struct vector_info dest,
				int ok_flags);

int number_is_unknown(ivl_expr_t ex)
{
      const char*bits;
      unsigned idx;

      assert(ivl_expr_type(ex) == IVL_EX_NUMBER);

      bits = ivl_expr_bits(ex);
      for (idx = 0 ;  idx < ivl_expr_width(ex) ;  idx += 1)
	    if ((bits[idx] != '0') && (bits[idx] != '1'))
		  return 1;

      return 0;
}

/*
 * This function returns TRUE if the number can be represented in a
 * 16bit immediate value. This amounts to looking for non-zero bits
 * above bitX. The maximum size of the immediate may vary, so use
 * lim_wid at the width limit to use.
 */
int number_is_immediate(ivl_expr_t ex, unsigned lim_wid)
{
      const char*bits;
      unsigned idx;

      if (ivl_expr_type(ex) != IVL_EX_NUMBER
	  && ivl_expr_type(ex) != IVL_EX_ULONG)
	    return 0;

      bits = ivl_expr_bits(ex);
      for (idx = lim_wid ;  idx < ivl_expr_width(ex) ;  idx += 1)
	    if (bits[idx] != '0')
		  return 0;

      return 1;
}

unsigned long get_number_immediate(ivl_expr_t ex)
{
      unsigned long imm = 0;
      unsigned idx;

      switch (ivl_expr_type(ex)) {
	  case IVL_EX_ULONG:
	    imm = ivl_expr_uvalue(ex);
	    break;

	  case IVL_EX_NUMBER: {
		const char*bits = ivl_expr_bits(ex);
		unsigned nbits = ivl_expr_width(ex);
		for (idx = 0 ; idx < nbits ; idx += 1) switch (bits[idx]){
		    case '0':
		      break;
		    case '1':
		      imm |= 1 << idx;
		      break;
		    default:
		      assert(0);
		}
		break;
	  }

	  default:
	    assert(0);
      }

      return imm;
}

/*
 * This function, in addition to setting the value into index 0, sets
 * bit 4 to 1 if the value is unknown.
 */
void draw_eval_expr_into_integer(ivl_expr_t expr, unsigned ix)
{
      struct vector_info vec;
      int word;

      switch (ivl_expr_value(expr)) {

	  case IVL_VT_BOOL:
	  case IVL_VT_LOGIC:
	    vec = draw_eval_expr(expr, 0);
	    fprintf(vvp_out, "    %%ix/get %u, %u, %u;\n",
		    ix, vec.base, vec.wid);
	    clr_vector(vec);
	    break;

	  case IVL_VT_REAL:
	    word = draw_eval_real(expr);
	    clr_word(word);
	    fprintf(vvp_out, "    %%cvt/ir %u, %u;\n", ix, word);
	    break;

	  default:
	    fprintf(stderr, "XXXX ivl_expr_value == %d\n",
		    ivl_expr_value(expr));
	    assert(0);
      }
}

/*
 * The STUFF_OK_XZ bit is true if the output is going to be further
 * processed so that x and z values are equivilent. This may allow for
 * new optimizations.
 */
static struct vector_info draw_eq_immediate(ivl_expr_t exp, unsigned ewid,
					    ivl_expr_t le,
					    ivl_expr_t re,
					    int stuff_ok_flag)
{
      unsigned wid;
      struct vector_info lv;
      unsigned long imm = get_number_immediate(re);

      wid = ivl_expr_width(le);
      lv = draw_eval_expr_wid(le, wid, stuff_ok_flag);

      switch (ivl_expr_opcode(exp)) {
	  case 'E': /* === */
	    fprintf(vvp_out, "    %%cmpi/u %u, %lu, %u;\n",
		    lv.base, imm, wid);
	    if (lv.base >= 8)
		  clr_vector(lv);
	    lv.base = 6;
	    lv.wid = 1;
	    break;

	  case 'e': /* == */
	      /* If this is a single bit being compared to 1, and the
		 output doesn't care about x vs z, then just return
		 the value itself. */
	    if ((stuff_ok_flag&STUFF_OK_XZ) && (lv.wid == 1) && (imm == 1))
		  break;

	    fprintf(vvp_out, "    %%cmpi/u %u, %lu, %u;\n",
		    lv.base, imm, wid);
	    if (lv.base >= 8)
		  clr_vector(lv);
	    lv.base = 4;
	    lv.wid = 1;
	    break;

	  case 'N': /* !== */
	    fprintf(vvp_out, "    %%cmpi/u %u, %lu, %u;\n",
		    lv.base, imm, wid);
	    if (lv.base >= 8)
		  clr_vector(lv);
	    lv.base = 6;
	    lv.wid = 1;
	    fprintf(vvp_out, "    %%inv 6, 1;\n");
	    break;

	  case 'n': /* != */
	      /* If this is a single bit being compared to 0, and the
		 output doesn't care about x vs z, then just return
		 the value itself. */
	    if ((stuff_ok_flag&STUFF_OK_XZ) && (lv.wid == 1) && (imm == 0))
		  break;

	    fprintf(vvp_out, "    %%cmpi/u %u, %lu, %u;\n",
		    lv.base, imm, wid);
	    if (lv.base >= 8)
		  clr_vector(lv);
	    lv.base = 4;
	    lv.wid = 1;
	    fprintf(vvp_out, "    %%inv 4, 1;\n");
	    break;

	  default:
	    assert(0);
      }

	/* In the special case that 47 bits are ok, and this really is
	   a single bit value, then we are done. */
      if ((lv.wid == 1) && (ewid == 1) && (stuff_ok_flag&STUFF_OK_47))
	    return lv;

	/* Move the result out out the 4-7 bit that the compare
	   uses. This is because that bit may be clobbered by other
	   expressions. */
      if (lv.base < 8) {
	    unsigned base = allocate_vector(ewid);
	    fprintf(vvp_out, "    %%mov %u, %u, 1;\n", base, lv.base);
	    lv.base = base;
	    lv.wid = ewid;
	    if (ewid > 1)
		  fprintf(vvp_out, "    %%mov %u, 0, %u;\n", base+1, ewid-1);

      } else if (lv.wid < ewid) {
	    unsigned base = allocate_vector(ewid);
	    if (lv.base >= 8)
		  clr_vector(lv);
	    fprintf(vvp_out, "    %%mov %u, %u, %u;\n", base,
		    lv.base, lv.wid);
	    fprintf(vvp_out, "    %%mov %u, 0, %u;\n",
		    base+lv.wid, ewid-lv.wid);
	    lv.base = base;
	    lv.wid = ewid;
      }

      return lv;
}

/*
 * This handles the special case that the operands of the comparison
 * are real valued expressions.
 */
static struct vector_info draw_binary_expr_eq_real(ivl_expr_t exp)
{
      struct vector_info res;
      int lword, rword;

      res.base = allocate_vector(1);
      res.wid  = 1;

      lword = draw_eval_real(ivl_expr_oper1(exp));
      rword = draw_eval_real(ivl_expr_oper2(exp));

      clr_word(lword);
      clr_word(rword);

      fprintf(vvp_out, "    %%cmp/wr %d, %d;\n", lword, rword);
      switch (ivl_expr_opcode(exp)) {

	  case 'e':
	    fprintf(vvp_out, "    %%mov %u, 4, 1;\n", res.base);
	    break;

	  case 'n': /* != */
	    fprintf(vvp_out, "    %%mov %u, 4, 1;\n", res.base);
	    fprintf(vvp_out, "    %%inv %u, 1;\n", res.base);
	    break;

	  default:
	    assert(0);
      }

      return res;
}

static struct vector_info draw_binary_expr_eq(ivl_expr_t exp,
					      unsigned ewid,
					      int stuff_ok_flag)
{
      ivl_expr_t le = ivl_expr_oper1(exp);
      ivl_expr_t re = ivl_expr_oper2(exp);

      unsigned wid;

      struct vector_info lv;
      struct vector_info rv;

      if ((ivl_expr_value(le) == IVL_VT_REAL)
	  ||(ivl_expr_value(re) == IVL_VT_REAL))  {
	    return draw_binary_expr_eq_real(exp);
      }

      if ((ivl_expr_type(re) == IVL_EX_ULONG)
	  && (0 == (ivl_expr_uvalue(re) & ~0xffff)))
	    return draw_eq_immediate(exp, ewid, le, re, stuff_ok_flag);

      if ((ivl_expr_type(re) == IVL_EX_NUMBER)
	  && (! number_is_unknown(re))
	  && number_is_immediate(re, 16))
	    return draw_eq_immediate(exp, ewid, le, re, stuff_ok_flag);

      assert(ivl_expr_value(le) == IVL_VT_LOGIC
	     || ivl_expr_value(le) == IVL_VT_BOOL);
      assert(ivl_expr_value(re) == IVL_VT_LOGIC
	     || ivl_expr_value(re) == IVL_VT_BOOL);

      wid = ivl_expr_width(le);
      if (ivl_expr_width(re) > wid)
	    wid = ivl_expr_width(re);

      lv = draw_eval_expr_wid(le, wid, stuff_ok_flag&~STUFF_OK_47);
      rv = draw_eval_expr_wid(re, wid, stuff_ok_flag&~STUFF_OK_47);

      switch (ivl_expr_opcode(exp)) {
	  case 'E': /* === */
	    assert(lv.wid == rv.wid);
	    fprintf(vvp_out, "    %%cmp/u %u, %u, %u;\n", lv.base,
		    rv.base, lv.wid);
	    if (lv.base >= 8)
		  clr_vector(lv);
	    if (rv.base >= 8)
		  clr_vector(rv);
	    lv.base = 6;
	    lv.wid = 1;
	    break;

	  case 'e': /* == */
	    if (lv.wid != rv.wid) {
		  fprintf(stderr,"internal error: operands of == "
			  " have different widths: %u vs %u\n",
			  lv.wid, rv.wid);
	    }
	    assert(lv.wid == rv.wid);
	    fprintf(vvp_out, "    %%cmp/u %u, %u, %u;\n", lv.base,
		    rv.base, lv.wid);
	    clr_vector(lv);
	    clr_vector(rv);
	    lv.base = 4;
	    lv.wid = 1;
	    break;

	  case 'N': /* !== */
	    if (lv.wid != rv.wid) {
		  fprintf(stderr,"internal error: operands of !== "
			  " have different widths: %u vs %u\n",
			  lv.wid, rv.wid);
	    }
	    assert(lv.wid == rv.wid);
	    fprintf(vvp_out, "    %%cmp/u %u, %u, %u;\n", lv.base,
		    rv.base, lv.wid);
	    fprintf(vvp_out, "    %%inv 6, 1;\n");

	    clr_vector(lv);
	    clr_vector(rv);
	    lv.base = 6;
	    lv.wid = 1;
	    break;

	  case 'n': /* != */
	    if (lv.wid != rv.wid) {
		  fprintf(stderr,"internal error: operands of != "
			  " have different widths: %u vs %u\n",
			  lv.wid, rv.wid);
	    }
	    assert(lv.wid == rv.wid);
	    fprintf(vvp_out, "    %%cmp/u %u, %u, %u;\n", lv.base,
		    rv.base, lv.wid);
	    fprintf(vvp_out, "    %%inv 4, 1;\n");

	    clr_vector(lv);
	    clr_vector(rv);
	    lv.base = 4;
	    lv.wid = 1;
	    break;

	  default:
	    assert(0);
      }

      if ((stuff_ok_flag&STUFF_OK_47) && (wid == 1)) {
	    return lv;
      }

	/* Move the result out out the 4-7 bit that the compare
	   uses. This is because that bit may be clobbered by other
	   expressions. */
      { unsigned base = allocate_vector(ewid);
        fprintf(vvp_out, "    %%mov %u, %u, 1;\n", base, lv.base);
	lv.base = base;
	lv.wid = ewid;
	if (ewid > 1)
	      fprintf(vvp_out, "    %%mov %u, 0, %u;\n", base+1, ewid-1);
      }

      return lv;
}

static struct vector_info draw_binary_expr_land(ivl_expr_t exp, unsigned wid)
{
      ivl_expr_t le = ivl_expr_oper1(exp);
      ivl_expr_t re = ivl_expr_oper2(exp);

      struct vector_info lv;
      struct vector_info rv;


      lv = draw_eval_expr(le, STUFF_OK_XZ);

      if ((lv.base >= 4) && (lv.wid > 1)) {
	    struct vector_info tmp;
	    clr_vector(lv);
	    tmp.base = allocate_vector(1);
	    tmp.wid = 1;
	    fprintf(vvp_out, "    %%or/r %u, %u, %u;\n", tmp.base,
		    lv.base, lv.wid);
	    lv = tmp;
      }

      rv = draw_eval_expr(re, STUFF_OK_XZ);
      if ((rv.base >= 4) && (rv.wid > 1)) {
	    struct vector_info tmp;
	    clr_vector(rv);
	    tmp.base = allocate_vector(1);
	    tmp.wid = 1;
	    fprintf(vvp_out, "    %%or/r %u, %u, %u;\n", tmp.base,
		    rv.base, rv.wid);
	    rv = tmp;
      }

      if (lv.base < 4) {
	    if (rv.base < 4) {
		  unsigned lb = lv.base;
		  unsigned rb = rv.base;

		  if ((lb == 0) || (rb == 0)) {
			lv.base = 0;

		  } else if ((lb == 1) && (rb == 1)) {
			lv.base = 1;
		  } else {
			lv.base = 2;
		  }
		  lv.wid = 1;

	    } else {
		  fprintf(vvp_out, "    %%and %u, %u, 1;\n", rv.base, lv.base);
		  lv = rv;
	    }

      } else {
	    fprintf(vvp_out, "    %%and %u, %u, 1;\n", lv.base, rv.base);
	    clr_vector(rv);
      }

	/* If we only want the single bit result, then we are done. */
      if (wid == 1)
	    return lv;

	/* Write the result into a zero-padded result. */
      { unsigned base = allocate_vector(wid);
        fprintf(vvp_out, "    %%mov %u, %u, 1;\n", base, lv.base);
	clr_vector(lv);
	lv.base = base;
	lv.wid = wid;
	fprintf(vvp_out, "    %%mov %u, 0, %u;\n", base+1, wid-1);
      }

      return lv;
}

static struct vector_info draw_binary_expr_lor(ivl_expr_t exp, unsigned wid)
{
      ivl_expr_t le = ivl_expr_oper1(exp);
      ivl_expr_t re = ivl_expr_oper2(exp);

      struct vector_info lv;
      struct vector_info rv;

      lv = draw_eval_expr(le, STUFF_OK_XZ);

	/* if the left operand has width, then evaluate the single-bit
	   logical equivilent. */
      if ((lv.base >= 4) && (lv.wid > 1)) {
	    struct vector_info tmp;
	    clr_vector(lv);
	    tmp.base = allocate_vector(1);
	    tmp.wid = 1;
	    fprintf(vvp_out, "    %%or/r %u, %u, %u;\n", tmp.base,
		    lv.base, lv.wid);
	    lv = tmp;
      }

      rv = draw_eval_expr(re, STUFF_OK_XZ);

	/* if the right operand has width, then evaluate the single-bit
	   logical equivilent. */
      if ((rv.base >= 4) && (rv.wid > 1)) {
	    struct vector_info tmp;
	    clr_vector(rv);
	    tmp.base = allocate_vector(1);
	    tmp.wid = 1;
	    fprintf(vvp_out, "    %%or/r %u, %u, %u;\n", tmp.base,
		    rv.base, rv.wid);
	    rv = tmp;
      }


      if (lv.base < 4) {
	    if (rv.base < 4) {
		  unsigned lb = lv.base;
		  unsigned rb = rv.base;

		  if ((lb == 0) && (rb == 0)) {
			lv.base = 0;

		  } else if ((lb == 1) || (rb == 1)) {
			lv.base = 1;
		  } else {
			lv.base = 2;
		  }

	    } else {
		  fprintf(vvp_out, "    %%or %u, %u, 1;\n", rv.base, lv.base);
		  lv = rv;
	    }

      } else {
	    fprintf(vvp_out, "    %%or %u, %u, 1;\n", lv.base, rv.base);
	    clr_vector(rv);
      }


	/* If we only want the single bit result, then we are done. */
      if (wid == 1)
	    return lv;

	/* Write the result into a zero-padded result. */
      { unsigned base = allocate_vector(wid);
        fprintf(vvp_out, "    %%mov %u, %u, 1;\n", base, lv.base);
	clr_vector(lv);
	lv.base = base;
	lv.wid = wid;
	fprintf(vvp_out, "    %%mov %u, 0, %u;\n", base+1, wid-1);
      }

      return lv;
}

static struct vector_info draw_binary_expr_le_real(ivl_expr_t exp)
{
      struct vector_info res;

      ivl_expr_t le = ivl_expr_oper1(exp);
      ivl_expr_t re = ivl_expr_oper2(exp);

      int lword = draw_eval_real(le);
      int rword = draw_eval_real(re);

      res.base = allocate_vector(1);
      res.wid  = 1;

      clr_word(lword);
      clr_word(rword);

      switch (ivl_expr_opcode(exp)) {
	  case '<':
	    fprintf(vvp_out, "    %%cmp/wr %d, %d;\n", lword, rword);
	    fprintf(vvp_out, "    %%mov %u, 5, 1;\n", res.base);
	    break;

	  case 'L': /* <= */
	    fprintf(vvp_out, "    %%cmp/wr %d, %d;\n", lword, rword);
	    fprintf(vvp_out, "    %%or 5, 4, 1;\n");
	    fprintf(vvp_out, "    %%mov %u, 5, 1;\n", res.base);
	    break;

	  case '>':
	    fprintf(vvp_out, "    %%cmp/wr %d, %d;\n", rword, lword);
	    fprintf(vvp_out, "    %%mov %u, 5, 1;\n", res.base);
	    break;

	  case 'G': /* >= */
	    fprintf(vvp_out, "    %%cmp/wr %d, %d;\n", rword, lword);
	    fprintf(vvp_out, "    %%or 5, 4, 1;\n");
	    fprintf(vvp_out, "    %%mov %u, 5, 1;\n", res.base);
	    break;

	  default:
	    assert(0);
      }

      return res;
}

static struct vector_info draw_binary_expr_le_bool(ivl_expr_t exp,
						   unsigned wid)
{
      ivl_expr_t le = ivl_expr_oper1(exp);
      ivl_expr_t re = ivl_expr_oper2(exp);

      int lw, rw;
      struct vector_info tmp;

      char s_flag = (ivl_expr_signed(le) && ivl_expr_signed(re)) ? 's' : 'u';

      assert(ivl_expr_value(le) == IVL_VT_BOOL);
      assert(ivl_expr_value(re) == IVL_VT_BOOL);

      lw = draw_eval_bool64(le);
      rw = draw_eval_bool64(re);

      switch (ivl_expr_opcode(exp)) {
	  case 'G':
	    fprintf(vvp_out, "    %%cmp/w%c %u, %u;\n", s_flag, rw, lw);
	    fprintf(vvp_out, "    %%or 5, 4, 1;\n");
	    break;

	  case 'L':
	    fprintf(vvp_out, "    %%cmp/w%c %u, %u;\n", s_flag, lw, rw);
	    fprintf(vvp_out, "    %%or 5, 4, 1;\n");
	    break;

	  case '<':
	    fprintf(vvp_out, "    %%cmp/w%c %u, %u;\n", s_flag, lw, rw);
	    break;

	  case '>':
	    fprintf(vvp_out, "    %%cmp/w%c %u, %u;\n", s_flag, rw, lw);
	    break;

	  default:
	    assert(0);
      }

      clr_word(lw);
      clr_word(rw);

	/* Move the result out out the 4-7 bit that the compare
	   uses. This is because that bit may be clobbered by other
	   expressions. */
      { unsigned base = allocate_vector(wid);
        fprintf(vvp_out, "    %%mov %u, 5, 1;\n", base);
	tmp.base = base;
	tmp.wid = wid;
	if (wid > 1)
	      fprintf(vvp_out, "    %%mov %u, 0, %u;\n", base+1, wid-1);
      }

      return tmp;
}

static struct vector_info draw_binary_expr_le(ivl_expr_t exp,
					      unsigned wid,
					      int stuff_ok_flag)
{
      ivl_expr_t le = ivl_expr_oper1(exp);
      ivl_expr_t re = ivl_expr_oper2(exp);

      struct vector_info lv;
      struct vector_info rv;

      char s_flag = (ivl_expr_signed(le) && ivl_expr_signed(re)) ? 's' : 'u';

      unsigned owid = ivl_expr_width(le);
      if (ivl_expr_width(re) > owid)
	    owid = ivl_expr_width(re);

      if (ivl_expr_value(le) == IVL_VT_REAL)
	    return draw_binary_expr_le_real(exp);

      if (ivl_expr_value(re) == IVL_VT_REAL)
	    return draw_binary_expr_le_real(exp);

	/* Detect the special case that we can do this with integers. */
      if (ivl_expr_value(le) == IVL_VT_BOOL
	  && ivl_expr_value(re) == IVL_VT_BOOL
	  && owid < 64) {
	    return draw_binary_expr_le_bool(exp, wid);
      }

      assert(ivl_expr_value(le) == IVL_VT_LOGIC
	     || ivl_expr_value(le) == IVL_VT_BOOL);
      assert(ivl_expr_value(re) == IVL_VT_LOGIC
	     || ivl_expr_value(re) == IVL_VT_BOOL);

      lv = draw_eval_expr_wid(le, owid, STUFF_OK_XZ);
      rv = draw_eval_expr_wid(re, owid, STUFF_OK_XZ);

      switch (ivl_expr_opcode(exp)) {
	  case 'G':
	    assert(lv.wid == rv.wid);
	    fprintf(vvp_out, "    %%cmp/%c %u, %u, %u;\n", s_flag,
		    rv.base, lv.base, lv.wid);
	    fprintf(vvp_out, "    %%or 5, 4, 1;\n");
	    break;

	  case 'L':
	    assert(lv.wid == rv.wid);
	    fprintf(vvp_out, "    %%cmp/%c %u, %u, %u;\n", s_flag,
		    lv.base, rv.base, lv.wid);
	    fprintf(vvp_out, "    %%or 5, 4, 1;\n");
	    break;

	  case '<':
	    assert(lv.wid == rv.wid);
	    fprintf(vvp_out, "    %%cmp/%c %u, %u, %u;\n", s_flag,
		    lv.base, rv.base, lv.wid);
	    break;

	  case '>':
	    assert(lv.wid == rv.wid);
	    fprintf(vvp_out, "    %%cmp/%c %u, %u, %u;\n", s_flag,
		    rv.base, lv.base, lv.wid);
	    break;

	  default:
	    assert(0);
      }

      clr_vector(lv);
      clr_vector(rv);

      if ((stuff_ok_flag&STUFF_OK_47) && (wid == 1)) {
	    lv.base = 5;
	    lv.wid = wid;
	    return lv;
      }

	/* Move the result out out the 4-7 bit that the compare
	   uses. This is because that bit may be clobbered by other
	   expressions. */
      { unsigned base = allocate_vector(wid);
        fprintf(vvp_out, "    %%mov %u, 5, 1;\n", base);
	lv.base = base;
	lv.wid = wid;
	if (wid > 1)
	      fprintf(vvp_out, "    %%mov %u, 0, %u;\n", base+1, wid-1);
      }

      return lv;
}

static struct vector_info draw_binary_expr_logic(ivl_expr_t exp,
						 unsigned wid)
{
      ivl_expr_t le = ivl_expr_oper1(exp);
      ivl_expr_t re = ivl_expr_oper2(exp);

      struct vector_info lv;
      struct vector_info rv;

      lv = draw_eval_expr_wid(le, wid, STUFF_OK_XZ);
      rv = draw_eval_expr_wid(re, wid, STUFF_OK_XZ);

	/* The result goes into the left operand, and that is returned
	   as the result. The instructions do not allow the lv value
	   to be a constant bit, so we either switch the operands, or
	   copy the vector into a new area. */
      if (lv.base < 4) {
	    if (rv.base > 4) {
		  struct vector_info tmp = lv;
		  lv = rv;
		  rv = tmp;

	    } else {
		  struct vector_info tmp;
		  tmp.base = allocate_vector(lv.wid);
		  tmp.wid = lv.wid;
		  fprintf(vvp_out, "    %%mov %u, %u, %u;\n",
			  tmp.base, lv.base, tmp.wid);
		  lv = tmp;
	    }
      }

      switch (ivl_expr_opcode(exp)) {

	  case '&':
	    fprintf(vvp_out, "   %%and %u, %u, %u;\n",
		    lv.base, rv.base, wid);
	    break;

	  case '|':
	    fprintf(vvp_out, "   %%or %u, %u, %u;\n",
		    lv.base, rv.base, wid);
	    break;

	  case '^':
	    fprintf(vvp_out, "   %%xor %u, %u, %u;\n",
		    lv.base, rv.base, wid);
	    break;

	  case 'A': /* NAND (~&) */
	    fprintf(vvp_out, "    %%nand %u, %u, %u;\n",
		    lv.base, rv.base, wid);
	    break;

	  case 'O': /* NOR (~|) */
	    fprintf(vvp_out, "    %%nor %u, %u, %u;\n",
		    lv.base, rv.base, wid);
	    break;

	  case 'X': /* exclusive nor (~^) */
	    fprintf(vvp_out, "    %%xnor %u, %u, %u;\n",
		    lv.base, rv.base, wid);
	    break;

	  default:
	    assert(0);
      }

      clr_vector(rv);
      return lv;
}

/*
 * Draw code to evaluate the << expression. Use the %shiftl/i0
 * or %shiftr/i0 instruction to do the real work of shifting. This
 * means that I can handle both left and right shifts in this
 * function, with the only difference the opcode I generate at the
 * end.
 */
static struct vector_info draw_binary_expr_lrs(ivl_expr_t exp, unsigned wid)
{
      ivl_expr_t le = ivl_expr_oper1(exp);
      ivl_expr_t re = ivl_expr_oper2(exp);
      const char*opcode = "?";

      struct vector_info lv;

	/* Evaluate the expression that is to be shifted. */
      switch (ivl_expr_opcode(exp)) {

	  case 'l': /* << (left shift) */
	    lv = draw_eval_expr_wid(le, wid, 0);

	      /* shifting 0 gets 0. */
	    if (lv.base == 0)
		  break;

	    if (lv.base < 4) {
		  struct vector_info tmp;
		  tmp.base = allocate_vector(lv.wid);
		  tmp.wid = lv.wid;
		  fprintf(vvp_out, "    %%mov %u, %u, %u;\n",
			  tmp.base, lv.base, lv.wid);
		  lv = tmp;
	    }
	    opcode = "%shiftl";
	    break;

	  case 'r': /* >> (unsigned right shift) */

	      /* with the right shift, there may be high bits that are
		 shifted into the desired width of the expression, so
		 we let the expression size itself, if it is bigger
		 then what is requested of us. */
	    if (wid > ivl_expr_width(le)) {
		  lv = draw_eval_expr_wid(le, wid, 0);
	    } else {
		  lv = draw_eval_expr_wid(le, ivl_expr_width(le), 0);
	    }

	      /* shifting 0 gets 0. */
	    if (lv.base == 0)
		  break;

	    if (lv.base < 4) {
		  struct vector_info tmp;
		  tmp.base = allocate_vector(lv.wid);
		  tmp.wid = lv.wid;
		  fprintf(vvp_out, "    %%mov %u, %u, %u;\n",
			  tmp.base, lv.base, lv.wid);
		  lv = tmp;
	    }
	    opcode = "%shiftr";
	    break;

	  case 'R': /* >>> (signed right shift) */

	      /* with the right shift, there may be high bits that are
		 shifted into the desired width of the expression, so
		 we let the expression size itself, if it is bigger
		 then what is requested of us. */
	    if (wid > ivl_expr_width(le)) {
		  lv = draw_eval_expr_wid(le, wid, 0);
	    } else {
		  lv = draw_eval_expr_wid(le, ivl_expr_width(le), 0);
	    }

	      /* shifting 0 gets 0. */
	    if (lv.base == 0)
		  break;

	      /* Sign extend any constant begets itself, if this
		 expression is signed. */
	    if ((lv.base < 4) && (ivl_expr_signed(exp)))
		  break;

	    if (lv.base < 4) {
		  struct vector_info tmp;
		  tmp.base = allocate_vector(lv.wid);
		  tmp.wid = lv.wid;
		  fprintf(vvp_out, "    %%mov %u, %u, %u;\n",
			  tmp.base, lv.base, lv.wid);
		  lv = tmp;
	    }

	    if (ivl_expr_signed(exp))
		  opcode = "%shiftr/s";
	    else
		  opcode = "%shiftr";
	    break;

	  default:
	    assert(0);
      }

	/* Figure out the shift amount and load that into the index
	   register. The value may be a constant, or may need to be
	   evaluated at run time. */
      switch (ivl_expr_type(re)) {

	  case IVL_EX_NUMBER: {
		unsigned shift = 0;
		unsigned idx, nbits = ivl_expr_width(re);
		const char*bits = ivl_expr_bits(re);

		for (idx = 0 ;  idx < nbits ;  idx += 1) switch (bits[idx]) {

		    case '0':
		      break;
		    case '1':
		      assert(idx < (8*sizeof shift));
		      shift |= 1 << idx;
		      break;
		    default:
		      assert(0);
		}

		fprintf(vvp_out, "    %%ix/load 0, %u;\n", shift);
		break;
	  }

	  case IVL_EX_ULONG:
	    fprintf(vvp_out, "    %%ix/load 0, %lu;\n", ivl_expr_uvalue(re));
	    break;

	  default: {
		  struct vector_info rv;
		  rv = draw_eval_expr(re, 0);
		  fprintf(vvp_out, "    %%ix/get 0, %u, %u;\n",
			  rv.base, rv.wid);
		  clr_vector(rv);
		  break;
	    }
      }


      fprintf(vvp_out, "    %s/i0  %u, %u;\n", opcode, lv.base, lv.wid);

      if (lv.base >= 8)
	    save_expression_lookaside(lv.base, exp, lv.wid);

      return lv;
}

static struct vector_info draw_add_immediate(ivl_expr_t le,
					     ivl_expr_t re,
					     unsigned wid)
{
      struct vector_info lv;
      unsigned long imm;

      lv = draw_eval_expr_wid(le, wid, STUFF_OK_XZ);
      assert(lv.wid == wid);

      imm = get_number_immediate(re);

	/* Now generate enough %addi instructions to add the entire
	   immediate value to the destination. The adds are done 16
	   bits at a time, but 17 bits are done to push the carry into
	   the higher bits if needed. */
      { unsigned base;
        for (base = 0 ;  base < lv.wid ;  base += 16) {
	      unsigned long tmp = imm & 0xffffUL;
	      unsigned add_wid = lv.wid - base;

	      imm >>= 16;

	      fprintf(vvp_out, "    %%addi %u, %lu, %u;\n",
		      lv.base+base, tmp, add_wid);

	      if (imm == 0)
		    break;
	}
      }

      return lv;
}

/*
 * The subi is restricted to imm operands that are <= 16 bits wide.
 */
static struct vector_info draw_sub_immediate(ivl_expr_t le,
					     ivl_expr_t re,
					     unsigned wid)
{
      struct vector_info lv;
      unsigned long imm;
      unsigned tmp;

      lv = draw_eval_expr_wid(le, wid, STUFF_OK_XZ);
      assert(lv.wid == wid);

      imm = get_number_immediate(re);
      assert( (imm & ~0xffff) == 0 );

      switch (lv.base) {
	  case 0:
	  case 1:
	    tmp = allocate_vector(wid);
	    fprintf(vvp_out, "    %%mov %u, %u, %u;\n", tmp, lv.base, wid);
	    lv.base = tmp;
	    fprintf(vvp_out, "    %%subi %u, %lu, %u;\n", lv.base, imm, wid);
	    return lv;

	  case 2:
	  case 3:
	    lv.base = 2;
	    return lv;

	  default:
	    fprintf(vvp_out, "    %%subi %u, %lu, %u;\n", lv.base, imm, wid);
      }


      return lv;
}

static struct vector_info draw_mul_immediate(ivl_expr_t le,
					     ivl_expr_t re,
					     unsigned wid)
{
      struct vector_info lv;
      unsigned long imm;

      lv = draw_eval_expr_wid(le, wid, STUFF_OK_XZ);
      assert(lv.wid == wid);

      imm = get_number_immediate(re);

      fprintf(vvp_out, "    %%muli %u, %lu, %u;\n", lv.base, imm, lv.wid);

      return lv;
}

static struct vector_info draw_binary_expr_arith(ivl_expr_t exp, unsigned wid)
{
      ivl_expr_t le = ivl_expr_oper1(exp);
      ivl_expr_t re = ivl_expr_oper2(exp);

      struct vector_info lv;
      struct vector_info rv;

      const char*sign_string = ivl_expr_signed(exp)? "/s" : "";

      if ((ivl_expr_opcode(exp) == '+')
	  && (ivl_expr_type(re) == IVL_EX_ULONG))
	    return draw_add_immediate(le, re, wid);

      if ((ivl_expr_opcode(exp) == '+')
	  && (ivl_expr_type(re) == IVL_EX_NUMBER)
	  && (! number_is_unknown(re))
	  && number_is_immediate(re, 8*sizeof(unsigned long)))
	    return draw_add_immediate(le, re, wid);

      if ((ivl_expr_opcode(exp) == '-')
	  && (ivl_expr_type(re) == IVL_EX_ULONG))
	    return draw_sub_immediate(le, re, wid);

      if ((ivl_expr_opcode(exp) == '-')
	  && (ivl_expr_type(re) == IVL_EX_NUMBER)
	  && (! number_is_unknown(re))
	  && number_is_immediate(re, 16))
	    return draw_sub_immediate(le, re, wid);

      if ((ivl_expr_opcode(exp) == '*')
	  && (ivl_expr_type(re) == IVL_EX_NUMBER)
	  && (! number_is_unknown(re))
	  && number_is_immediate(re, 16))
	    return draw_mul_immediate(le, re, wid);

      lv = draw_eval_expr_wid(le, wid, STUFF_OK_XZ);
      rv = draw_eval_expr_wid(re, wid, STUFF_OK_XZ|STUFF_OK_RO);

      if (lv.wid != wid) {
	    fprintf(stderr, "XXXX ivl_expr_opcode(exp) = %c,"
		    " lv.wid=%u, wid=%u\n", ivl_expr_opcode(exp),
		    lv.wid, wid);
      }

      assert(lv.wid == wid);
      assert(rv.wid == wid);

	/* The arithmetic instructions replace the left operand with
	   the result. If the left operand is a replicated constant,
	   then I need to make a writeable copy so that the
	   instruction can operate. */
      if (lv.base < 4) {
	    struct vector_info tmp;

	    tmp.base = allocate_vector(wid);
	    tmp.wid = wid;
	    fprintf(vvp_out, "    %%mov %u, %u, %u;\n", tmp.base,
		    lv.base, wid);
	    lv = tmp;
      }

      switch (ivl_expr_opcode(exp)) {
	  case '+':
	    fprintf(vvp_out, "    %%add %u, %u, %u;\n", lv.base, rv.base, wid);
	    break;

	  case '-':
	    fprintf(vvp_out, "    %%sub %u, %u, %u;\n", lv.base, rv.base, wid);
	    break;

	  case '*':
	    fprintf(vvp_out, "    %%mul %u, %u, %u;\n", lv.base, rv.base, wid);
	    break;

	  case '/':
	    fprintf(vvp_out, "    %%div%s %u, %u, %u;\n", sign_string,
		    lv.base, rv.base, wid);
	    break;

	  case '%':
	    fprintf(vvp_out, "    %%mod%s %u, %u, %u;\n", sign_string,
		    lv.base, rv.base, wid);
	    break;

	  default:
	    assert(0);
      }

      clr_vector(rv);

      return lv;
}

static struct vector_info draw_binary_expr(ivl_expr_t exp,
					   unsigned wid,
					   int stuff_ok_flag)
{
      struct vector_info rv;
      int stuff_ok_used_flag = 0;

      switch (ivl_expr_opcode(exp)) {
	  case 'a': /* && (logical and) */
	    rv = draw_binary_expr_land(exp, wid);
	    break;

	  case 'E': /* === */
	  case 'e': /* == */
	  case 'N': /* !== */
	  case 'n': /* != */
	    rv = draw_binary_expr_eq(exp, wid, stuff_ok_flag);
	    stuff_ok_used_flag = 1;
	    break;

	  case '<':
	  case '>':
	  case 'L': /* <= */
	  case 'G': /* >= */
	    rv = draw_binary_expr_le(exp, wid, stuff_ok_flag);
	    stuff_ok_used_flag = 1;
	    break;

	  case '+':
	  case '-':
	  case '*':
	  case '/':
	  case '%':
	    rv = draw_binary_expr_arith(exp, wid);
	    break;

	  case 'l': /* << */
	  case 'r': /* >> */
	  case 'R': /* >>> */
	    rv = draw_binary_expr_lrs(exp, wid);
	    break;

	  case 'o': /* || (logical or) */
	    rv = draw_binary_expr_lor(exp, wid);
	    break;

	  case '&':
	  case '|':
	  case '^':
	  case 'A': /* NAND (~&) */
	  case 'O': /* NOR  (~|) */
	  case 'X': /* XNOR (~^) */
	    rv = draw_binary_expr_logic(exp, wid);
	    break;

	  default:
	    fprintf(stderr, "vvp.tgt error: unsupported binary (%c)\n",
		    ivl_expr_opcode(exp));
	    assert(0);
      }

	/* Mark in the lookaside that this value is done. If any OK
	   flags besides the STUFF_OK_47 flag are set, then the result
	   may not be a pure one, so clear the lookaside for the range
	   instead of setting in to the new expression result.

	   The stuff_ok_used_flag tells me if the stuff_ok_flag was
	   even used by anything. If not, then I can ignore it in the
	   following logic. */
      if (rv.base >= 8) {
	    if (stuff_ok_used_flag && (stuff_ok_flag & ~STUFF_OK_47))
		  save_expression_lookaside(rv.base, 0, wid);
	    else
		  save_expression_lookaside(rv.base, exp, wid);
      }

      return rv;
}

/*
 * The concatenation operator is evaluated by evaluating each sub-
 * expression, then copying it into the continguous vector of the
 * result. Do this until the result vector is filled.
 */
static struct vector_info draw_concat_expr(ivl_expr_t exp, unsigned wid,
					   int stuff_ok_flag)
{
      unsigned off, rep;
      struct vector_info res;

      int alloc_exclusive = (stuff_ok_flag&STUFF_OK_RO) ? 0 : 1;

	/* Allocate a vector to hold the result. */
      res.base = allocate_vector(wid);
      res.wid = wid;

	/* Get the repeat count. This must be a constant that has been
	   evaluated at compile time. The operands will be repeated to
	   form the result. */
      rep = ivl_expr_repeat(exp);
      off = 0;

      while (rep > 0) {

	      /* Each repeat, evaluate the sub-expressions, from lsb
		 to msb, and copy each into the result vector. The
		 expressions are arranged in the concatenation from
		 MSB to LSB, to go through them backwards.

		 Abort the loop if the result vector gets filled up. */

	    unsigned idx = ivl_expr_parms(exp);
	    while ((idx > 0) && (off < wid)) {
		  ivl_expr_t arg = ivl_expr_parm(exp, idx-1);
		  unsigned awid = ivl_expr_width(arg);

		  unsigned trans;
		  struct vector_info avec;

		    /* Try to locate the subexpression in the
		       lookaside map. */
		  avec.base = allocate_vector_exp(arg, awid, alloc_exclusive);
		  avec.wid = awid;

		  trans = awid;
		  if ((off + awid) > wid)
			trans = wid - off;

		  if (avec.base != 0) {
			assert(awid == avec.wid);

			fprintf(vvp_out, "    %%mov %u, %u, %u;\n",
				res.base+off,
				avec.base, trans);
			clr_vector(avec);

		  } else {
			struct vector_info dest;

			dest.base = res.base+off;
			dest.wid  = trans;
			draw_eval_expr_dest(arg, dest, 0);
		  }


		  idx -= 1;
		  off += trans;
		  assert(off <= wid);
	    }
	    rep -= 1;
      }

	/* Pad the result with 0, if necessary. */
      if (off < wid) {
	    fprintf(vvp_out, "    %%mov %u, 0, %u;\n",
		    res.base+off, wid-off);
      }

	/* Save the accumulated result in the lookaside map. */
      if (res.base >= 8)
	    save_expression_lookaside(res.base, exp, wid);

      return res;
}

/*
 * A number in an expression is made up by copying constant bits into
 * the allocated vector.
 */
static struct vector_info draw_number_expr(ivl_expr_t exp, unsigned wid)
{
      unsigned idx;
      unsigned nwid;
      struct vector_info res;
      const char*bits = ivl_expr_bits(exp);

      res.wid  = wid;

      nwid = wid;
      if (ivl_expr_width(exp) < nwid)
	    nwid = ivl_expr_width(exp);

	/* If all the bits of the number have the same value, then we
	   can use a constant bit. There is no need to allocate wr
	   bits, and there is no need to generate any code. */

      for (idx = 1 ;  idx < nwid ;  idx += 1) {
	    if (bits[idx] != bits[0])
		  break;
      }

      if (idx >= res.wid) {
	    switch (bits[0]) {
		case '0':
		  res.base = 0;
		  break;
		case '1':
		  res.base = 1;
		  break;
		case 'x':
		  res.base = 2;
		  break;
		case 'z':
		  res.base = 3;
		  break;
		default:
		  assert(0);
		  res.base = 0;
	    }
	    return res;
      }

	/* The number value needs to be represented as an allocated
	   vector. Allocate the vector and use %mov instructions to
	   load the constant bit values. */
      res.base = allocate_vector(wid);

      idx = 0;
      while (idx < nwid) {
	    unsigned cnt;
	    char src = '?';
	    switch (bits[idx]) {
		case '0':
		  src = '0';
		  break;
		case '1':
		  src = '1';
		  break;
		case 'x':
		  src = '2';
		  break;
		case 'z':
		  src = '3';
		  break;
	    }

	    for (cnt = 1 ;  idx+cnt < wid ;  cnt += 1)
		  if (bits[idx+cnt] != bits[idx])
			break;

	    fprintf(vvp_out, "    %%mov %u, %c, %u;\n",
		    res.base+idx, src, cnt);

	    idx += cnt;
      }

	/* Pad the number up to the expression width. */
      if (idx < wid) {
	    if (ivl_expr_signed(exp) && bits[nwid-1] == '1')
		  fprintf(vvp_out, "    %%mov %u, 1, %u;\n",
			  res.base+idx, wid-idx);

	    else if (bits[nwid-1] == 'x')
		  fprintf(vvp_out, "    %%mov %u, 2, %u;\n",
			  res.base+idx, wid-idx);

	    else if (bits[nwid-1] == 'z')
		  fprintf(vvp_out, "    %%mov %u, 3, %u;\n",
			  res.base+idx, wid-idx);

	    else
		  fprintf(vvp_out, "    %%mov %u, 0, %u;\n",
			  res.base+idx, wid-idx);
      }

      if (res.base >= 8)
	    save_expression_lookaside(res.base, exp, wid);

      return res;
}

/*
 * The PAD expression takes a smaller node and pads it out to a larger
 * value. It will zero extend or sign extend depending on the
 * signedness of the expression.
 */
static struct vector_info draw_pad_expr(ivl_expr_t exp, unsigned wid)
{
      struct vector_info subv;
      struct vector_info res;

      subv = draw_eval_expr(ivl_expr_oper1(exp), 0);
      if (wid <= subv.wid) {
	    if (subv.base >= 8)
		  save_expression_lookaside(subv.base, exp, subv.wid);
	    res.base = subv.base;
	    res.wid = wid;
	    return res;
      }

      res.base = allocate_vector(wid);
      res.wid = wid;

      fprintf(vvp_out, "    %%mov %u, %u, %u;\n",
	      res.base, subv.base, subv.wid);

      assert(wid > subv.wid);

      if (ivl_expr_signed(exp)) {
	    unsigned idx;
	    for (idx = subv.wid ;  idx < res.wid ;  idx += 1)
		  fprintf(vvp_out, "    %%mov %u, %u, 1;\n",
			  res.base+idx, subv.base+subv.wid-1);
      } else {
	    fprintf(vvp_out, "    %%mov %u, 0, %u;\n",
		    res.base+subv.wid, res.wid - subv.wid);
      }

      save_expression_lookaside(res.base, exp, wid);
      return res;
}

static struct vector_info draw_realnum_expr(ivl_expr_t exp, unsigned wid)
{
      struct vector_info res;
      double val = ivl_expr_dvalue(exp);
      long ival = val;

      unsigned addr, run, idx;
      int bit;

      fprintf(vvp_out, "; draw_realnum_expr(%f, %u) as %ld\n",
	      val, wid, ival);

      res.base = allocate_vector(wid);
      res.wid = wid;

      addr = res.base;
      run = 1;
      bit = ival & 1;
      ival >>= 1LL;

      for (idx = 1 ;  idx < wid ;  idx += 1) {
	    int next_bit = ival & 1;
	    ival >>= 1LL;

	    if (next_bit == bit) {
		  run += 1;
		  continue;
	    }

	    fprintf(vvp_out, "  %%mov %u, %d, %u;\n", addr, bit, run);
	    addr += run;
	    run = 1;
	    bit = next_bit;
      }

      fprintf(vvp_out, "  %%mov %u, %d, %u;\n", addr, bit, run);


      return res;
}

/*
 * A string in an expression is made up by copying constant bits into
 * the allocated vector.
 */
static struct vector_info draw_string_expr(ivl_expr_t exp, unsigned wid)
{
      struct vector_info res;
      const char *p = ivl_expr_string(exp);
      unsigned ewid, nwid;
      unsigned bit = 0, idx;

      res.wid  = wid;
      nwid = wid;
      ewid = ivl_expr_width(exp);
      if (ewid < nwid)
	    nwid = ewid;

      p += (ewid / 8) - 1;

	/* The string needs to be represented as an allocated
	   vector. Allocate the vector and use %mov instructions to
	   load the constant bit values. */
      res.base = allocate_vector(wid);

      idx = 0;
      while (idx < nwid) {
	    unsigned this_bit = ((*p) & (1 << bit)) ? 1 : 0;

	    fprintf(vvp_out, "    %%mov %u, %d, 1;\n",
		    res.base+idx, this_bit);

	    bit++;
	    if (bit == 8) {
		  bit = 0;
		  p--;
	    }

	    idx++;
      }

	/* Pad the number up to the expression width. */
      if (idx < wid)
	    fprintf(vvp_out, "    %%mov %u, 0, %u;\n", res.base+idx, wid-idx);

      if (res.base >= 8)
	    save_expression_lookaside(res.base, exp, wid);

      return res;
}

/*
* This function is given an expression, the preallocated vector
* result, and the swid that is filled in so far. The caller has
* already calculated the load swid bits of exp into the beginning of
* the res vector. This function just calculates the pad to fill out
* the res area.
*/
static void pad_expr_in_place(ivl_expr_t exp, struct vector_info res, unsigned swid)
{
      if (res.wid <= swid)
	    return;

      if (ivl_expr_signed(exp)) {
	    unsigned idx;
	    for (idx = swid ;  idx < res.wid ;  idx += 1)
		  fprintf(vvp_out, "    %%mov %u, %u, 1;\n",
			  res.base+idx, res.base+swid-1);

      } else {
	    fprintf(vvp_out, "    %%mov %u, 0, %u;\n",
		    res.base+swid, res.wid-swid);
      }
}

/*
 * Evaluating a signal expression means loading the bits of the signal
 * into the thread bits. Remember to account for the part select by
 * offsetting the read from the lsi (least significant index) of the
 * signal.
 */
static void draw_signal_dest(ivl_expr_t exp, struct vector_info res)
{
      unsigned swid = ivl_expr_width(exp);
      ivl_signal_t sig = ivl_expr_signal(exp);

      unsigned word = 0;

      if (swid > res.wid)
	    swid = res.wid;

	/* If this is an access to an array, handle that by emiting a
	   load/av instruction. */
      if (ivl_signal_array_count(sig) > 1) {
	    ivl_expr_t ix = ivl_expr_oper1(exp);
	    if (!number_is_immediate(ix, 8*sizeof(unsigned long))) {
		  draw_eval_expr_into_integer(ix, 3);
		  fprintf(vvp_out, "   %%load/av %u, v%p, %u;\n",
			  res.base, sig, swid);
		  pad_expr_in_place(exp, res, swid);
		  return;
	    }

	      /* The index is constant, so we can return to direct
	         readout with the specific word selected. */
	    word = get_number_immediate(ix);
      }

	/* If this is a REG (a variable) then I can do a vector read. */
      fprintf(vvp_out, "    %%load/v %u, v%p_%u, %u;\n",
	      res.base, sig, word, swid);

      pad_expr_in_place(exp, res, swid);
}

static struct vector_info draw_signal_expr(ivl_expr_t exp, unsigned wid,
					   int stuff_ok_flag)
{
      struct vector_info res;

      int alloc_exclusive = (stuff_ok_flag&STUFF_OK_RO) ? 0 : 1;

	/* Already in the vector lookaside? */
      res.base = allocate_vector_exp(exp, wid, alloc_exclusive);
      res.wid = wid;
      if (res.base != 0) {
	    fprintf(vvp_out, "; Reuse signal base=%u wid=%u from lookaside.\n",
		    res.base, res.wid);
	    return res;
      }

      res.base = allocate_vector(wid);
      res.wid  = wid;
      save_expression_lookaside(res.base, exp, wid);

      draw_signal_dest(exp, res);
      return res;
}

static struct vector_info draw_select_signal(ivl_expr_t sube,
					     ivl_expr_t bit_idx,
					     unsigned bit_wid,
					     unsigned wid)
{
      ivl_signal_t sig = ivl_expr_signal(sube);
      struct vector_info shiv;
      struct vector_info res;
      unsigned idx;

	/* Use this word of the signal. */
      unsigned use_word = 0;
	/* If this is an access to an array, handle that by emiting a
	   load/av instruction. */
      if (ivl_signal_array_count(sig) > 1) {
	    ivl_expr_t ix = ivl_expr_oper1(sube);
	    if (!number_is_immediate(ix, 8*sizeof(unsigned long))) {
		  draw_eval_expr_into_integer(ix, 3);
		  assert(0); /* XXXX Don't know how to load part
			     select! */
		  return res;
	    }

	      /* The index is constant, so we can return to direct
	         readout with the specific word selected. */
	    use_word = get_number_immediate(ix);
      }

      shiv = draw_eval_expr(bit_idx, STUFF_OK_XZ|STUFF_OK_RO);

      fprintf(vvp_out, "   %%ix/get 0, %u, %u;\n", shiv.base, shiv.wid);
      if (shiv.base >= 8)
	    clr_vector(shiv);

	/* Try the special case that the base is 0 and the width
	   exactly matches the signal. Just load the signal in one
	   instruction. */
      if (shiv.base == 0 && ivl_expr_width(sube) == wid) {
	    res.base = allocate_vector(wid);
	    res.wid = wid;
	    fprintf(vvp_out, "   %%load/v %u, v%p_%u, %u;\n",
		    res.base, sig, use_word, ivl_expr_width(sube));

	    return res;
      }

	/* Try the special case that hte part is at the beginning and
	   nearly the width of the signal. In this case, just load the
	   entire signal in one go then simply drop the excess bits. */
      if (shiv.base == 0
	  && (ivl_expr_width(sube) > wid)
	  && (ivl_expr_width(sube) < (wid+wid/10))) {

	    res.base = allocate_vector(ivl_expr_width(sube));
	    res.wid = ivl_expr_width(sube);
	    fprintf(vvp_out, "   %%load/v %u, v%p_%u, %u; Only need %u bits\n",
		    res.base, sig, use_word, ivl_expr_width(sube), wid);
 
	    save_signal_lookaside(res.base, sig, use_word, res.wid);

	    {
		  struct vector_info tmp;
		  tmp.base = res.base + wid;
		  tmp.wid = res.wid - wid;
		  clr_vector(tmp);
		  res.wid = wid;
	    }
	    return res;
      }

	/* Alas, do it the hard way. */
      res.base = allocate_vector(wid);
      res.wid = wid;

      for (idx = 0 ;  idx < res.wid ;  idx += 1) {
	    if (idx >= bit_wid) {
		  fprintf(vvp_out, "   %%mov %u, 0, %u; Pad from %u to %u\n",
			  res.base+idx, res.wid-idx,
			  ivl_expr_width(sube), wid);
		  break;
	    }
	    fprintf(vvp_out, "   %%load/x.p %u, v%p_%u, 0;\n",
		    res.base+idx, sig, use_word);
      }

      return res;
}

static struct vector_info draw_select_expr(ivl_expr_t exp, unsigned wid,
					   int stuff_ok_flag)
{
      struct vector_info subv, shiv, res;
      ivl_expr_t sube  = ivl_expr_oper1(exp);
      ivl_expr_t shift = ivl_expr_oper2(exp);

      int alloc_exclusive = (stuff_ok_flag&STUFF_OK_RO)? 0 : 1;

      res.wid = wid;

	/* First look for the self expression in the lookaside, and
	   allocate that if possible. If I find it, then immediatly
	   return that. */
      if ( (res.base = allocate_vector_exp(exp, wid, alloc_exclusive)) != 0) {
	    fprintf(vvp_out, "; Reuse base=%u wid=%u from lookaside.\n",
		    res.base, wid);
	    return res;
      }

      if (ivl_expr_type(sube) == IVL_EX_SIGNAL) {
	    res = draw_select_signal(sube, shift, ivl_expr_width(exp), wid);
	    fprintf(vvp_out, "; Save base=%u wid=%u in lookaside.\n",
		    res.base, wid);
	    save_expression_lookaside(res.base, exp, wid);
	    return res;
      }

	/* Evaluate the sub-expression. */
      subv = draw_eval_expr(sube, 0);

	/* Any bit select of a constant zero is another constant zero,
	   so short circuit and return the value we know. */
      if (subv.base == 0) {
	    subv.wid = wid;
	    return subv;
      }

	/* Evaluate the bit select base expression and store the
	   result into index register 0. */
      shiv = draw_eval_expr(shift, STUFF_OK_XZ);

	/* Detect and handle the special case that the shift is a
	   constant 0. Skip the shift, and return the subexpression
	   with the width trimmed down to the desired width. */
      if (shiv.base == 0) {
	    assert(subv.wid >= wid);
	    res.base = subv.base;
	    return res;
      }

      fprintf(vvp_out, "    %%ix/get 0, %u, %u;\n", shiv.base, shiv.wid);
      clr_vector(shiv);

	/* If the subv result is a magic constant, then make a copy in
	   writeable vector space and work from there instead. */
      if (subv.base < 4) {
	    res.base = allocate_vector(subv.wid);
	    res.wid = subv.wid;
	    fprintf(vvp_out, "    %%mov %u, %u, %u;\n", res.base,
		    subv.base, res.wid);
	    subv = res;
      }

      fprintf(vvp_out, "    %%shiftr/i0 %u, %u;\n", subv.base, subv.wid);

      if (subv.wid > wid) {
	    res.base = subv.base;
	    res.wid = wid;

	    subv.base += wid;
	    subv.wid  -= wid;
	    clr_vector(subv);

      } else {
	    assert(subv.wid == wid);
	    res = subv;
      }

      if (res.base >= 8) {
	    fprintf(vvp_out, "; Save expression base=%u wid=%u in lookaside\n",
		    res.base, wid);
	    save_expression_lookaside(res.base, exp, wid);
      }

      return res;
}

static struct vector_info draw_ternary_expr(ivl_expr_t exp, unsigned wid)
{
      struct vector_info res, tru, fal, tst;

      unsigned lab_true, lab_false, lab_out;
      ivl_expr_t cond = ivl_expr_oper1(exp);
      ivl_expr_t true_ex = ivl_expr_oper2(exp);
      ivl_expr_t false_ex = ivl_expr_oper3(exp);

      lab_true  = local_count++;
      lab_false = local_count++;
      lab_out = local_count++;

	/* Evaluate the condition expression, and if necessary reduce
	   it to a single bit. */
      tst = draw_eval_expr(cond, STUFF_OK_XZ|STUFF_OK_RO);
      if ((tst.base >= 4) && (tst.wid > 1)) {
	    struct vector_info tmp;

	    fprintf(vvp_out, "    %%or/r %u, %u, %u;\n",
		    tst.base, tst.base, tst.wid);

	    tmp = tst;
	    tmp.base += 1;
	    tmp.wid -= 1;
	    clr_vector(tmp);

	    tst.wid = 1;
      }

      fprintf(vvp_out, "    %%jmp/0  T_%d.%d, %u;\n",
	      thread_count, lab_true, tst.base);

      tru = draw_eval_expr_wid(true_ex, wid, 0);

	/* The true result must be in a writable register, because the
	   blend might want to manipulate it. */
      if (tru.base < 4) {
	    struct vector_info tmp;
	    tmp.base = allocate_vector(wid);
	    tmp.wid = wid;
	    fprintf(vvp_out, "    %%mov %u, %u, %u;\n",
		    tmp.base, tru.base, wid);
	    tru = tmp;
      }

      fprintf(vvp_out, "    %%jmp/1  T_%d.%d, %u;\n",
	      thread_count, lab_out, tst.base);

      clear_expression_lookaside();

      fprintf(vvp_out, "T_%d.%d ; End of true expr.\n",
	      thread_count, lab_true);

      fal = draw_eval_expr_wid(false_ex, wid, 0);

      fprintf(vvp_out, "    %%jmp/0  T_%d.%d, %u;\n",
	      thread_count, lab_false, tst.base);

      fprintf(vvp_out, " ; End of false expr.\n");

      clr_vector(tst);
      clr_vector(fal);

      fprintf(vvp_out, "    %%blend  %u, %u, %u; Condition unknown.\n",
	      tru.base, fal.base, wid);
      fprintf(vvp_out, "    %%jmp  T_%d.%d;\n",
	      thread_count, lab_out);

      fprintf(vvp_out, "T_%d.%d ;\n", thread_count, lab_false);
      fprintf(vvp_out, "    %%mov %u, %u, %u; Return false value\n",
	      tru.base, fal.base, wid);

	/* This is the out label. */
      fprintf(vvp_out, "T_%d.%d ;\n", thread_count, lab_out);
      clear_expression_lookaside();

      res = tru;

      if (res.base >= 8)
	    save_expression_lookaside(res.base, exp, wid);

      return res;
}

static struct vector_info draw_sfunc_expr(ivl_expr_t exp, unsigned wid)
{
      unsigned parm_count = ivl_expr_parms(exp);
      struct vector_info res;


	/* If the function has no parameters, then use this short-form
	   to draw the statement. */
      if (parm_count == 0) {
	    assert(ivl_expr_value(exp) == IVL_VT_LOGIC
		   || ivl_expr_value(exp) == IVL_VT_BOOL);
	    res.base = allocate_vector(wid);
	    res.wid  = wid;
	    fprintf(vvp_out, "    %%vpi_func \"%s\", %u, %u;\n",
		    ivl_expr_name(exp), res.base, res.wid);
	    return res;

      }

      res = draw_vpi_func_call(exp, wid);

	/* New basic block starts after VPI calls. */
      clear_expression_lookaside();

      return res;
}

static struct vector_info draw_unary_expr(ivl_expr_t exp, unsigned wid)
{
      struct vector_info res;
      ivl_expr_t sub = ivl_expr_oper1(exp);
      const char *rop = 0;
      int inv = 0;

      switch (ivl_expr_opcode(exp)) {
	  case '&': rop = "and";  break;
	  case '|': rop = "or";   break;
	  case '^': rop = "xor";  break;
	  case 'A': rop = "nand"; break;
	  case 'N': rop = "nor";  break;
	  case 'X': rop = "xnor"; break;
      }

      switch (ivl_expr_opcode(exp)) {
	  case '~':
	    res = draw_eval_expr_wid(sub, wid, STUFF_OK_XZ);
	    switch (res.base) {
		case 0:
		  res.base = 1;
		  break;
		case 1:
		  res.base = 0;
		  break;
		case 2:
		case 3:
		  res.base = 2;
		  break;
		default:
		  fprintf(vvp_out, "    %%inv %u, %u;\n", res.base, res.wid);
		  break;
	    }
	    break;

	  case '-':
	      /* Unary minus is implemented by generating the 2's
		 complement of the number. That is the 1's complement
		 (bitwise invert) with a 1 added in. Note that the
		 %sub subtracts -1 (1111...) to get %add of +1. */
	    res = draw_eval_expr_wid(sub, wid, STUFF_OK_XZ);
	    switch (res.base) {
		case 0:
		  res.base = 0;
		  break;
		case 1:
		  res.base = 1;
		  break;
		case 2:
		case 3:
		  res.base = 2;
		  break;
		default:
		  fprintf(vvp_out, "    %%inv %u, %u;\n", res.base, res.wid);
		  fprintf(vvp_out, "    %%addi %u, 1, %u;\n",res.base,res.wid);
		  break;
	    }
	    break;

	  case '!':
	    res = draw_eval_expr(sub, STUFF_OK_XZ);
	    if (res.wid > 1) {
		    /* a ! on a vector is implemented with a reduction
		       nor. Generate the result into the first bit of
		       the input vector and free the rest of the
		       vector. */
		  struct vector_info tmp;
		  assert(res.base >= 4);
		  tmp.base = res.base+1;
		  tmp.wid = res.wid - 1;
		  fprintf(vvp_out, "    %%nor/r %u, %u, %u;\n",
			  res.base, res.base, res.wid);
		  clr_vector(tmp);
		  res.wid = 1;
	    } else switch (res.base) {
		case 0:
		  res.base = 1;
		  break;
		case 1:
		  res.base = 0;
		  break;
		case 2:
		case 3:
		  res.base = 2;
		  break;
		default:
		  fprintf(vvp_out, "    %%inv %u, 1;\n", res.base);
		  break;
	    }

	      /* If the result needs to be bigger then the calculated
		 value, then write it into a padded vector. */
	    if (res.wid < wid) {
		  struct vector_info tmp;
		  tmp.base = allocate_vector(wid);
		  tmp.wid = wid;
		  fprintf(vvp_out, "    %%mov %u, %u, %u;\n",
			  tmp.base, res.base, res.wid);
		  fprintf(vvp_out, "    %%mov %u, 0, %u;\n",
			  tmp.base+res.wid, tmp.wid-res.wid);
		  clr_vector(res);
		  res = tmp;
	    }
	    break;

	  case 'N':
	  case 'A':
	  case 'X':
	    inv = 1;
	  case '&':
	  case '|':
	  case '^':
	    res = draw_eval_expr(sub, 0);
	    if (res.wid > 1) {
		  struct vector_info tmp;
		    /* If the previous result is in the constant area
		       (and is a vector) then copy it out into some
		       temporary space. */
		  if (res.base < 4) {
			tmp.base = allocate_vector(res.wid);
			tmp.wid = res.wid;
			fprintf(vvp_out, "    %%mov %u, %u, %u;\n",
				tmp.base, res.base, res.wid);
			res = tmp;
		  }

		  tmp.base = res.base+1;
		  tmp.wid = res.wid - 1;
		  fprintf(vvp_out, "    %%%s/r %u, %u, %u;\n",
			  rop,
			  res.base, res.base, res.wid);
		  clr_vector(tmp);
		  res.wid = 1;
	    } else if (inv) {
		  assert(res.base >= 4);
		  fprintf(vvp_out, "    %%inv %u, 1;\n", res.base);
	    }

	      /* If the result needs to be bigger then the calculated
		 value, then write it into a passed vector. */
	    if (res.wid < wid) {
		  struct vector_info tmp;
		  tmp.base = allocate_vector(wid);
		  tmp.wid = wid;
		  fprintf(vvp_out, "    %%mov %u, %u, %u;\n",
			  tmp.base, res.base, res.wid);
		  fprintf(vvp_out, "    %%mov %u, 0, %u;\n",
			  tmp.base+res.wid, tmp.wid-res.wid);
		  clr_vector(res);
		  res = tmp;
	    }
	    break;

	  default:
	    fprintf(stderr, "vvp error: unhandled unary: %c\n",
		    ivl_expr_opcode(exp));
	    assert(0);
      }

      if (res.base >= 8)
	    save_expression_lookaside(res.base, exp, wid);

      return res;
}

/*
 * Sometimes we know ahead of time where we want the expression value
 * to go. In that case, call this function. It will check to see if
 * the expression can be preplaced, and if so it will evaluate it in
 * place.
 */
static void draw_eval_expr_dest(ivl_expr_t exp, struct vector_info dest,
				int stuff_ok_flag)
{
      struct vector_info tmp;

      switch (ivl_expr_type(exp)) {

	  case IVL_EX_SIGNAL:
	    draw_signal_dest(exp, dest);
	    return;

	  default:
	    break;
      }

	/* Fallback, is to draw the expression by width, and mov it to
	   the required dest. */
      tmp = draw_eval_expr_wid(exp, dest.wid, stuff_ok_flag);
      assert(tmp.wid == dest.wid);

      fprintf(vvp_out, "    %%mov %u, %u, %u;\n",
	      dest.base, tmp.base, dest.wid);

      if (tmp.base >= 8)
	    save_expression_lookaside(tmp.base, exp, tmp.wid);

      clr_vector(tmp);
}

struct vector_info draw_eval_expr_wid(ivl_expr_t exp, unsigned wid,
				      int stuff_ok_flag)
{
      struct vector_info res;

      switch (ivl_expr_type(exp)) {
	  default:
	    fprintf(stderr, "vvp error: unhandled expr type: %u\n",
	    ivl_expr_type(exp));
	  case IVL_EX_NONE:
	    assert(0);
	    res.base = 0;
	    res.wid = 0;
	    break;

	  case IVL_EX_STRING:
	    res = draw_string_expr(exp, wid);
	    break;

	  case IVL_EX_BINARY:
	    res = draw_binary_expr(exp, wid, stuff_ok_flag);
	    break;

	  case IVL_EX_CONCAT:
	    res = draw_concat_expr(exp, wid, stuff_ok_flag);
	    break;

	  case IVL_EX_NUMBER:
	    res = draw_number_expr(exp, wid);
	    break;

	  case IVL_EX_REALNUM:
	    res = draw_realnum_expr(exp, wid);
	    break;

	  case IVL_EX_SELECT:
	    if (ivl_expr_oper2(exp) == 0)
		  res = draw_pad_expr(exp, wid);
	    else
		  res = draw_select_expr(exp, wid, stuff_ok_flag);
	    break;

	  case IVL_EX_SIGNAL:
	    res = draw_signal_expr(exp, wid, stuff_ok_flag);
	    break;

	  case IVL_EX_TERNARY:
	    res = draw_ternary_expr(exp, wid);
	    break;

	  case IVL_EX_SFUNC:
	    res = draw_sfunc_expr(exp, wid);
	    break;

	  case IVL_EX_UFUNC:
	    res = draw_ufunc_expr(exp, wid);
	    break;

	  case IVL_EX_UNARY:
	    res = draw_unary_expr(exp, wid);
	    break;
      }

      return res;
}

struct vector_info draw_eval_expr(ivl_expr_t exp, int stuff_ok_flag)
{
      return draw_eval_expr_wid(exp, ivl_expr_width(exp), stuff_ok_flag);
}

/*
 * $Log: eval_expr.c,v $
 * Revision 1.134  2007/02/12 04:37:58  steve
 *  Get padding right when loading array word into big vector.
 *
 * Revision 1.133  2007/01/19 05:24:53  steve
 *  Handle real constants in vector expressions.
 *
 * Revision 1.132  2007/01/17 04:39:18  steve
 *  Remove dead code related to memories.
 *
 * Revision 1.131  2007/01/16 05:44:16  steve
 *  Major rework of array handling. Memories are replaced with the
 *  more general concept of arrays. The NetMemory and NetEMemory
 *  classes are removed from the ivl core program, and the IVL_LPM_RAM
 *  lpm type is removed from the ivl_target API.
 *
 * Revision 1.130  2006/02/02 02:43:59  steve
 *  Allow part selects of memory words in l-values.
 *
 * Revision 1.129  2006/01/02 05:33:20  steve
 *  Node delays can be more general expressions in structural contexts.
 *
 * Revision 1.128  2005/12/22 15:42:22  steve
 *  Pad part selects
 *
 * Revision 1.127  2005/10/11 18:54:10  steve
 *  Remove the $ from signal labels. They do not help.
 *
 * Revision 1.126  2005/10/11 18:30:50  steve
 *  Remove obsolete vvp_memory_label function.
 *
 * Revision 1.125  2005/09/19 21:45:36  steve
 *  Spelling patches from Larry.
 *
 * Revision 1.124  2005/09/19 20:18:20  steve
 *  Fix warnings about uninitialized variables.
 *
 * Revision 1.123  2005/09/17 04:01:32  steve
 *  Improve loading of part selects when easy.
 *
 * Revision 1.122  2005/09/17 01:01:00  steve
 *  More robust use of precalculated expressions, and
 *  Separate lookaside for written variables that can
 *  also be reused.
 *
 * Revision 1.121  2005/09/15 02:49:47  steve
 *  Better reuse of IVL_EX_SELECT expressions.
 *
 * Revision 1.120  2005/09/14 02:53:15  steve
 *  Support bool expressions and compares handle them optimally.
 *
 * Revision 1.119  2005/07/13 04:52:31  steve
 *  Handle functions with real values.
 *
 * Revision 1.118  2005/07/11 16:56:51  steve
 *  Remove NetVariable and ivl_variable_t structures.
 *
 * Revision 1.117  2005/03/12 23:45:33  steve
 *  Handle function/task port vectors.
 *
 * Revision 1.116  2005/03/03 04:34:42  steve
 *  Rearrange how memories are supported as vvp_vector4 arrays.
 *
 * Revision 1.115  2005/02/15 07:12:55  steve
 *  Support constant part select writes to l-values, and large part select reads from signals.
 *
 * Revision 1.114  2005/01/28 05:37:48  steve
 *  Special handling of constant shift 0.
 *
 * Revision 1.113  2005/01/24 05:28:31  steve
 *  Remove the NetEBitSel and combine all bit/part select
 *  behavior into the NetESelect node and IVL_EX_SELECT
 *  ivl_target expression type.
 *
 * Revision 1.112  2005/01/24 05:08:02  steve
 *  Part selects are done in the compiler, not here.
 *
 * Revision 1.111  2004/12/11 02:31:28  steve
 *  Rework of internals to carry vectors through nexus instead
 *  of single bits. Make the ivl, tgt-vvp and vvp initial changes
 *  down this path.
 *
 * Revision 1.110  2004/10/04 01:10:57  steve
 *  Clean up spurious trailing white space.
 *
 * Revision 1.109  2004/09/10 00:14:31  steve
 *  Relaxed width constraint on pad_expression output.
 *
 * Revision 1.108  2004/06/30 03:07:32  steve
 *  Watch out for real compared to constant. Handle as real.
 *
 * Revision 1.107  2004/06/19 16:17:37  steve
 *  Generate signed modulus if appropriate.
 *
 * Revision 1.106  2003/10/01 17:44:20  steve
 *  Slightly more efficient unary minus.
 *
 * Revision 1.105  2003/09/24 20:46:20  steve
 *  Clear expression lookaside after true cause of ternary.
 *
 * Revision 1.104  2003/08/03 03:53:38  steve
 *  Subtract from constant values.
 *
 * Revision 1.103  2003/07/26 03:34:43  steve
 *  Start handling pad of expressions in code generators.
 *
 * Revision 1.102  2003/06/18 03:55:19  steve
 *  Add arithmetic shift operators.
 *
 * Revision 1.101  2003/06/17 19:17:42  steve
 *  Remove short int restrictions from vvp opcodes.
 *
 * Revision 1.100  2003/06/16 22:14:15  steve
 *  Fix fprintf warning.
 *
 * Revision 1.99  2003/06/15 22:49:32  steve
 *  More efficient code for ternary expressions.
 *
 * Revision 1.98  2003/06/14 22:18:54  steve
 *  Sign extend signed vectors.
 *
 * Revision 1.97  2003/06/13 19:10:20  steve
 *  Handle assign of real to vector.
 *
 * Revision 1.96  2003/06/11 02:23:45  steve
 *  Proper pad of signed constants.
 *
 * Revision 1.95  2003/05/10 02:38:49  steve
 *  Proper width handling of || expressions.
 *
 * Revision 1.94  2003/03/25 02:15:48  steve
 *  Use hash code for scope labels.
 *
 * Revision 1.93  2003/03/15 04:45:18  steve
 *  Allow real-valued vpi functions to have arguments.
 *
 * Revision 1.92  2003/02/28 20:21:13  steve
 *  Merge vpi_call and vpi_func draw functions.
 *
 * Revision 1.91  2003/02/07 02:46:16  steve
 *  Handle real value subtract and comparisons.
 *
 * Revision 1.90  2003/01/27 00:14:37  steve
 *  Support in various contexts the $realtime
 *  system task.
 *
 * Revision 1.89  2003/01/26 21:15:59  steve
 *  Rework expression parsing and elaboration to
 *  accommodate real/realtime values and expressions.
 *
 * Revision 1.88  2002/12/20 01:11:14  steve
 *  Evaluate shift index after shift operand because
 *  the chift operand may use the index register itself.
 *
 * Revision 1.87  2002/12/19 23:11:29  steve
 *  Keep bit select subexpression width if it is constant.
 */

