/* Symbolic values.
   Copyright (C) 2019-2021 Free Software Foundation, Inc.
   Contributed by David Malcolm <dmalcolm@redhat.com>.

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3, or (at your option)
any later version.

GCC is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING3.  If not see
<http://www.gnu.org/licenses/>.  */

#ifndef GCC_ANALYZER_SVALUE_H
#define GCC_ANALYZER_SVALUE_H

#include "analyzer/complexity.h"

using namespace ana;

namespace ana {

/* An enum for discriminating between the different concrete subclasses
   of svalue.  */

enum svalue_kind
{
  SK_REGION,
  SK_CONSTANT,
  SK_UNKNOWN,
  SK_POISONED,
  SK_SETJMP,
  SK_INITIAL,
  SK_UNARYOP,
  SK_BINOP,
  SK_SUB,
  SK_UNMERGEABLE,
  SK_PLACEHOLDER,
  SK_WIDENING,
  SK_COMPOUND,
  SK_CONJURED
};

/* svalue and its subclasses.

   The class hierarchy looks like this (using indentation to show
   inheritance, and with svalue_kinds shown for the concrete subclasses):

   svalue
     region_svalue (SK_REGION): a pointer to a region
     constant_svalue (SK_CONSTANT): a constant
     unknown_svalue (SK_UNKNOWN): an unknowable value
     poisoned_svalue (SK_POISONED): a unusable value (undefined)
     setjmp_svalue (SK_SETJMP): a setjmp/longjmp buffer
     initial_svalue (SK_INITIAL): the initial value of a region
     unaryop_svalue (SK_UNARYOP): unary operation on another svalue
     binop_svalue (SK_BINOP): binary operation on two svalues
     sub_svalue (SK_SUB): the result of accessing a subregion
     unmergeable_svalue (SK_UNMERGEABLE): a value that is so interesting
       from a control-flow perspective that it can inhibit state-merging
     placeholder_svalue (SK_PLACEHOLDER): for use in selftests.
     widening_svalue (SK_WIDENING): a merger of two svalues (possibly
       in an iteration).
     compound_svalue (SK_COMPOUND): a mapping of bit-ranges to svalues
     conjured_svalue (SK_CONJURED): a value arising from a stmt.  */

/* An abstract base class representing a value held by a region of memory.  */

class svalue
{
public:
  virtual ~svalue () {}

  tree get_type () const { return m_type; }

  virtual enum svalue_kind get_kind () const = 0;

  void print (const region_model &model,
	      pretty_printer *pp) const;

  virtual void dump_to_pp (pretty_printer *pp, bool simple) const = 0;
  void dump (bool simple=true) const;
  label_text get_desc (bool simple=true) const;

  json::value *to_json () const;

  virtual const region_svalue *
  dyn_cast_region_svalue () const { return NULL; }
  virtual const constant_svalue *
  dyn_cast_constant_svalue () const { return NULL; }
  virtual const poisoned_svalue *
  dyn_cast_poisoned_svalue () const { return NULL; }
  virtual const setjmp_svalue *
  dyn_cast_setjmp_svalue () const { return NULL; }
  virtual const initial_svalue *
  dyn_cast_initial_svalue () const { return NULL; }
  virtual const unaryop_svalue *
  dyn_cast_unaryop_svalue () const { return NULL; }
  virtual const binop_svalue *
  dyn_cast_binop_svalue () const { return NULL; }
  virtual const sub_svalue *
  dyn_cast_sub_svalue () const { return NULL; }
  virtual const unmergeable_svalue *
  dyn_cast_unmergeable_svalue () const { return NULL; }
  virtual const widening_svalue *
  dyn_cast_widening_svalue () const { return NULL; }
  virtual const compound_svalue *
  dyn_cast_compound_svalue () const { return NULL; }
  virtual const conjured_svalue *
  dyn_cast_conjured_svalue () const { return NULL; }

  tree maybe_get_constant () const;
  const svalue *maybe_undo_cast () const;
  const svalue *unwrap_any_unmergeable () const;

  const svalue *can_merge_p (const svalue *other,
			      region_model_manager *mgr,
			      model_merger *merger) const;

  const complexity &get_complexity () const { return m_complexity; }

  virtual void accept (visitor *v) const  = 0;

  bool live_p (const svalue_set *live_svalues,
	       const region_model *model) const;
  virtual bool implicitly_live_p (const svalue_set *live_svalues,
				  const region_model *model) const;

  static int cmp_ptr (const svalue *, const svalue *);
  static int cmp_ptr_ptr (const void *, const void *);

  bool involves_p (const svalue *other) const;

 protected:
  svalue (complexity c, tree type)
  : m_complexity (c), m_type (type)
  {}

 private:
  complexity m_complexity;
  tree m_type;
};

/* Concrete subclass of svalue representing a pointer value that points to
   a known region  */

class region_svalue : public svalue
{
public:
  /* A support class for uniquifying instances of region_svalue.  */
  struct key_t
  {
    key_t (tree type, const region *reg)
    : m_type (type), m_reg (reg)
    {}

    hashval_t hash () const
    {
      inchash::hash hstate;
      hstate.add_ptr (m_type);
      hstate.add_ptr (m_reg);
      return hstate.end ();
    }

    bool operator== (const key_t &other) const
    {
      return (m_type == other.m_type && m_reg == other.m_reg);
    }

    void mark_deleted () { m_type = reinterpret_cast<tree> (1); }
    void mark_empty () { m_type = NULL_TREE; }
    bool is_deleted () const { return m_type == reinterpret_cast<tree> (1); }
    bool is_empty () const { return m_type == NULL_TREE; }

    tree m_type;
    const region *m_reg;
  };

  region_svalue (tree type, const region *reg)
  : svalue (complexity (reg), type),
    m_reg (reg)
  {
    gcc_assert (m_reg != NULL);
  }

  enum svalue_kind get_kind () const FINAL OVERRIDE { return SK_REGION; }
  const region_svalue *
  dyn_cast_region_svalue () const FINAL OVERRIDE { return this; }

  void dump_to_pp (pretty_printer *pp, bool simple) const FINAL OVERRIDE;
  void accept (visitor *v) const FINAL OVERRIDE;
  bool implicitly_live_p (const svalue_set *,
			  const region_model *) const FINAL OVERRIDE;

  const region * get_pointee () const { return m_reg; }

  static tristate eval_condition (const region_svalue *lhs_ptr,
				  enum tree_code op,
				  const region_svalue *rhs_ptr);

 private:
  const region *m_reg;
};

} // namespace ana

template <>
template <>
inline bool
is_a_helper <const region_svalue *>::test (const svalue *sval)
{
  return sval->get_kind () == SK_REGION;
}

template <> struct default_hash_traits<region_svalue::key_t>
: public member_function_hash_traits<region_svalue::key_t>
{
  static const bool empty_zero_p = true;
};

namespace ana {

/* Concrete subclass of svalue representing a specific constant value.  */

class constant_svalue : public svalue
{
public:
  constant_svalue (tree cst_expr)
  : svalue (complexity (1, 1), TREE_TYPE (cst_expr)), m_cst_expr (cst_expr)
  {
    gcc_assert (cst_expr);
    gcc_assert (CONSTANT_CLASS_P (cst_expr));
  }

  enum svalue_kind get_kind () const FINAL OVERRIDE { return SK_CONSTANT; }
  const constant_svalue *
  dyn_cast_constant_svalue () const FINAL OVERRIDE { return this; }

  void dump_to_pp (pretty_printer *pp, bool simple) const FINAL OVERRIDE;
  void accept (visitor *v) const FINAL OVERRIDE;
  bool implicitly_live_p (const svalue_set *,
			  const region_model *) const FINAL OVERRIDE;

  tree get_constant () const { return m_cst_expr; }
  static tristate eval_condition (const constant_svalue *lhs,
				  enum tree_code op,
				  const constant_svalue *rhs);

 private:
  tree m_cst_expr;
};

} // namespace ana

template <>
template <>
inline bool
is_a_helper <const constant_svalue *>::test (const svalue *sval)
{
  return sval->get_kind () == SK_CONSTANT;
}

namespace ana {

/* Concrete subclass of svalue representing an unknowable value, the bottom
   value when thinking of svalues as a lattice.
   This is a singleton (w.r.t. its manager): there is a single unknown_svalue
   per type.  Self-comparisons of such instances yield "unknown".  */

class unknown_svalue : public svalue
{
public:
  unknown_svalue (tree type)
  : svalue (complexity (1, 1), type)
  {}

  enum svalue_kind get_kind () const FINAL OVERRIDE { return SK_UNKNOWN; }

  void dump_to_pp (pretty_printer *pp, bool simple) const FINAL OVERRIDE;
  void accept (visitor *v) const FINAL OVERRIDE;
};

/* An enum describing a particular kind of "poisoned" value.  */

enum poison_kind
{
  /* For use to describe freed memory.  */
  POISON_KIND_FREED,

  /* For use on pointers to regions within popped stack frames.  */
  POISON_KIND_POPPED_STACK
};

extern const char *poison_kind_to_str (enum poison_kind);

/* Concrete subclass of svalue representing a value that should not
   be used (e.g. uninitialized memory, freed memory).  */

class poisoned_svalue : public svalue
{
public:
  /* A support class for uniquifying instances of poisoned_svalue.  */
  struct key_t
  {
    key_t (enum poison_kind kind, tree type)
    : m_kind (kind), m_type (type)
    {}

    hashval_t hash () const
    {
      inchash::hash hstate;
      hstate.add_int (m_kind);
      hstate.add_ptr (m_type);
      return hstate.end ();
    }

    bool operator== (const key_t &other) const
    {
      return (m_kind == other.m_kind && m_type == other.m_type);
    }

    void mark_deleted () { m_type = reinterpret_cast<tree> (1); }
    void mark_empty () { m_type = NULL_TREE; }
    bool is_deleted () const { return m_type == reinterpret_cast<tree> (1); }
    bool is_empty () const { return m_type == NULL_TREE; }

    enum poison_kind m_kind;
    tree m_type;
  };

  poisoned_svalue (enum poison_kind kind, tree type)
  : svalue (complexity (1, 1), type), m_kind (kind) {}

  enum svalue_kind get_kind () const FINAL OVERRIDE { return SK_POISONED; }
  const poisoned_svalue *
  dyn_cast_poisoned_svalue () const FINAL OVERRIDE { return this; }

  void dump_to_pp (pretty_printer *pp, bool simple) const FINAL OVERRIDE;
  void accept (visitor *v) const FINAL OVERRIDE;

  enum poison_kind get_poison_kind () const { return m_kind; }

 private:
  enum poison_kind m_kind;
};

} // namespace ana

template <>
template <>
inline bool
is_a_helper <const poisoned_svalue *>::test (const svalue *sval)
{
  return sval->get_kind () == SK_POISONED;
}

template <> struct default_hash_traits<poisoned_svalue::key_t>
: public member_function_hash_traits<poisoned_svalue::key_t>
{
  static const bool empty_zero_p = true;
};

namespace ana {

/* A bundle of information recording a setjmp/sigsetjmp call, corresponding
   roughly to a jmp_buf.  */

struct setjmp_record
{
  setjmp_record (const exploded_node *enode,
		 const gcall *setjmp_call)
  : m_enode (enode), m_setjmp_call (setjmp_call)
  {
  }

  bool operator== (const setjmp_record &other) const
  {
    return (m_enode == other.m_enode
	    && m_setjmp_call == other.m_setjmp_call);
  }

  void add_to_hash (inchash::hash *hstate) const
  {
    hstate->add_ptr (m_enode);
    hstate->add_ptr (m_setjmp_call);
  }

  static int cmp (const setjmp_record &rec1, const setjmp_record &rec2);

  const exploded_node *m_enode;
  const gcall *m_setjmp_call;
};

/* Concrete subclass of svalue representing buffers for setjmp/sigsetjmp,
   so that longjmp/siglongjmp can potentially "return" to an entirely
   different function.  */

class setjmp_svalue : public svalue
{
public:
  /* A support class for uniquifying instances of poisoned_svalue.  */
  struct key_t
  {
    key_t (const setjmp_record &record, tree type)
    : m_record (record), m_type (type)
    {}

    hashval_t hash () const
    {
      inchash::hash hstate;
      m_record.add_to_hash (&hstate);
      hstate.add_ptr (m_type);
      return hstate.end ();
    }

    bool operator== (const key_t &other) const
    {
      return (m_record == other.m_record && m_type == other.m_type);
    }

    void mark_deleted () { m_type = reinterpret_cast<tree> (1); }
    void mark_empty () { m_type = NULL_TREE; }
    bool is_deleted () const { return m_type == reinterpret_cast<tree> (1); }
    bool is_empty () const { return m_type == NULL_TREE; }

    setjmp_record m_record;
    tree m_type;
  };

  setjmp_svalue (const setjmp_record &setjmp_record,
		  tree type)
  : svalue (complexity (1, 1), type), m_setjmp_record (setjmp_record)
  {}

  enum svalue_kind get_kind () const FINAL OVERRIDE { return SK_SETJMP; }
  const setjmp_svalue *
  dyn_cast_setjmp_svalue () const FINAL OVERRIDE { return this; }

  void dump_to_pp (pretty_printer *pp, bool simple) const FINAL OVERRIDE;
  void accept (visitor *v) const FINAL OVERRIDE;

  int get_enode_index () const;

  const setjmp_record &get_setjmp_record () const { return m_setjmp_record; }

 private:
  setjmp_record m_setjmp_record;
};

} // namespace ana

template <>
template <>
inline bool
is_a_helper <const setjmp_svalue *>::test (const svalue *sval)
{
  return sval->get_kind () == SK_SETJMP;
}

template <> struct default_hash_traits<setjmp_svalue::key_t>
: public member_function_hash_traits<setjmp_svalue::key_t>
{
  static const bool empty_zero_p = true;
};

namespace ana {

/* Concrete subclass of svalue representing the initial value of a
   specific region.

   This represents the initial value at the start of the analysis path,
   as opposed to the first time the region is accessed during the path.
   Hence as soon as we have a call to an unknown function, all previously
   unmodelled globals become implicitly "unknown" rathen than "initial".  */

class initial_svalue : public svalue
{
public:
  initial_svalue (tree type, const region *reg)
  : svalue (complexity (reg), type), m_reg (reg)
  {
    gcc_assert (m_reg != NULL);
  }

  enum svalue_kind get_kind () const FINAL OVERRIDE { return SK_INITIAL; }
  const initial_svalue *
  dyn_cast_initial_svalue () const FINAL OVERRIDE { return this; }

  void dump_to_pp (pretty_printer *pp, bool simple) const FINAL OVERRIDE;
  void accept (visitor *v) const FINAL OVERRIDE;
  bool implicitly_live_p (const svalue_set *,
			  const region_model *) const FINAL OVERRIDE;

  bool initial_value_of_param_p () const;

  const region *get_region () const { return m_reg; }

 private:
  const region *m_reg;
};

} // namespace ana

template <>
template <>
inline bool
is_a_helper <const initial_svalue *>::test (const svalue *sval)
{
  return sval->get_kind () == SK_INITIAL;
}

namespace ana {

/* Concrete subclass of svalue representing a unary operation on
   another svalues (e.g. a cast).  */

class unaryop_svalue : public svalue
{
public:
  /* A support class for uniquifying instances of unaryop_svalue.  */
  struct key_t
  {
    key_t (tree type, enum tree_code op, const svalue *arg)
    : m_type (type), m_op (op), m_arg (arg)
    {}

    hashval_t hash () const
    {
      inchash::hash hstate;
      hstate.add_ptr (m_type);
      hstate.add_int (m_op);
      hstate.add_ptr (m_arg);
      return hstate.end ();
    }

    bool operator== (const key_t &other) const
    {
      return (m_type == other.m_type
	      && m_op == other.m_op
	      && m_arg == other.m_arg);
    }

    void mark_deleted () { m_type = reinterpret_cast<tree> (1); }
    void mark_empty () { m_type = NULL_TREE; }
    bool is_deleted () const { return m_type == reinterpret_cast<tree> (1); }
    bool is_empty () const { return m_type == NULL_TREE; }

    tree m_type;
    enum tree_code m_op;
    const svalue *m_arg;
  };

  unaryop_svalue (tree type, enum tree_code op, const svalue *arg)
  : svalue (complexity (arg), type), m_op (op), m_arg (arg)
  {
  }

  enum svalue_kind get_kind () const FINAL OVERRIDE { return SK_UNARYOP; }
  const unaryop_svalue *
  dyn_cast_unaryop_svalue () const FINAL OVERRIDE { return this; }

  void dump_to_pp (pretty_printer *pp, bool simple) const FINAL OVERRIDE;
  void accept (visitor *v) const FINAL OVERRIDE;
  bool implicitly_live_p (const svalue_set *,
			  const region_model *) const FINAL OVERRIDE;

  enum tree_code get_op () const { return m_op; }
  const svalue *get_arg () const { return m_arg; }

 private:
  enum tree_code m_op;
  const svalue *m_arg;
};

} // namespace ana

template <>
template <>
inline bool
is_a_helper <const unaryop_svalue *>::test (const svalue *sval)
{
  return sval->get_kind () == SK_UNARYOP;
}

template <> struct default_hash_traits<unaryop_svalue::key_t>
: public member_function_hash_traits<unaryop_svalue::key_t>
{
  static const bool empty_zero_p = true;
};

namespace ana {

/* Concrete subclass of svalue representing a binary operation of
   two svalues.  */

class binop_svalue : public svalue
{
public:
  /* A support class for uniquifying instances of binop_svalue.  */
  struct key_t
  {
    key_t (tree type, enum tree_code op,
	   const svalue *arg0, const svalue *arg1)
    : m_type (type), m_op (op), m_arg0 (arg0), m_arg1 (arg1)
    {}

    hashval_t hash () const
    {
      inchash::hash hstate;
      hstate.add_ptr (m_type);
      hstate.add_int (m_op);
      hstate.add_ptr (m_arg0);
      hstate.add_ptr (m_arg1);
      return hstate.end ();
    }

    bool operator== (const key_t &other) const
    {
      return (m_type == other.m_type
	      && m_op == other.m_op
	      && m_arg0 == other.m_arg0
	      && m_arg1 == other.m_arg1);
    }

    void mark_deleted () { m_type = reinterpret_cast<tree> (1); }
    void mark_empty () { m_type = NULL_TREE; }
    bool is_deleted () const { return m_type == reinterpret_cast<tree> (1); }
    bool is_empty () const { return m_type == NULL_TREE; }

    tree m_type;
    enum tree_code m_op;
    const svalue *m_arg0;
    const svalue *m_arg1;
  };

  binop_svalue (tree type, enum tree_code op,
		 const svalue *arg0, const svalue *arg1)
  : svalue (complexity::from_pair (arg0->get_complexity (),
				    arg1->get_complexity ()),
	     type),
    m_op (op), m_arg0 (arg0), m_arg1 (arg1)
  {
  }

  enum svalue_kind get_kind () const FINAL OVERRIDE { return SK_BINOP; }
  const binop_svalue *dyn_cast_binop_svalue () const FINAL OVERRIDE
  {
    return this;
  }

  void dump_to_pp (pretty_printer *pp, bool simple) const FINAL OVERRIDE;
  void accept (visitor *v) const FINAL OVERRIDE;
  bool implicitly_live_p (const svalue_set *,
			  const region_model *) const FINAL OVERRIDE;

  enum tree_code get_op () const { return m_op; }
  const svalue *get_arg0 () const { return m_arg0; }
  const svalue *get_arg1 () const { return m_arg1; }

 private:
  enum tree_code m_op;
  const svalue *m_arg0;
  const svalue *m_arg1;
};

} // namespace ana

template <>
template <>
inline bool
is_a_helper <const binop_svalue *>::test (const svalue *sval)
{
  return sval->get_kind () == SK_BINOP;
}

template <> struct default_hash_traits<binop_svalue::key_t>
: public member_function_hash_traits<binop_svalue::key_t>
{
  static const bool empty_zero_p = true;
};

namespace ana {

/* Concrete subclass of svalue representing the result of accessing a subregion
   of another svalue (the value of a component/field of a struct, or an element
   from an array).  */

class sub_svalue : public svalue
{
public:
  /* A support class for uniquifying instances of sub_svalue.  */
  struct key_t
  {
    key_t (tree type, const svalue *parent_svalue, const region *subregion)
    : m_type (type), m_parent_svalue (parent_svalue), m_subregion (subregion)
    {}

    hashval_t hash () const
    {
      inchash::hash hstate;
      hstate.add_ptr (m_type);
      hstate.add_ptr (m_parent_svalue);
      hstate.add_ptr (m_subregion);
      return hstate.end ();
    }

    bool operator== (const key_t &other) const
    {
      return (m_type == other.m_type
	      && m_parent_svalue == other.m_parent_svalue
	      && m_subregion == other.m_subregion);
    }

    void mark_deleted () { m_type = reinterpret_cast<tree> (1); }
    void mark_empty () { m_type = NULL_TREE; }
    bool is_deleted () const { return m_type == reinterpret_cast<tree> (1); }
    bool is_empty () const { return m_type == NULL_TREE; }

    tree m_type;
    const svalue *m_parent_svalue;
    const region *m_subregion;
  };
  sub_svalue (tree type, const svalue *parent_svalue,
	       const region *subregion);

  enum svalue_kind get_kind () const FINAL OVERRIDE { return SK_SUB; }
  const sub_svalue *dyn_cast_sub_svalue () const FINAL OVERRIDE
  {
    return this;
  }

  void dump_to_pp (pretty_printer *pp, bool simple) const FINAL OVERRIDE;
  void accept (visitor *v) const FINAL OVERRIDE;
  bool implicitly_live_p (const svalue_set *,
			  const region_model *) const FINAL OVERRIDE;

  const svalue *get_parent () const { return m_parent_svalue; }
  const region *get_subregion () const { return m_subregion; }

 private:
  const svalue *m_parent_svalue;
  const region *m_subregion;
};

} // namespace ana

template <>
template <>
inline bool
is_a_helper <const sub_svalue *>::test (const svalue *sval)
{
  return sval->get_kind () == SK_SUB;
}

template <> struct default_hash_traits<sub_svalue::key_t>
: public member_function_hash_traits<sub_svalue::key_t>
{
  static const bool empty_zero_p = true;
};

namespace ana {

/* Concrete subclass of svalue: decorate another svalue,
   so that the resulting svalue can be identified as being
   "interesting to control flow".
   For example, consider the return value from setjmp.  We
   don't want to merge states in which the result is 0 with
   those in which the result is non-zero.  By using an
   unmergeable_svalue for the result, we can inhibit such merges
   and have separate exploded nodes for those states, keeping
   the first and second returns from setjmp distinct in the exploded
   graph.  */

class unmergeable_svalue : public svalue
{
public:
  unmergeable_svalue (const svalue *arg)
  : svalue (complexity (arg), arg->get_type ()), m_arg (arg)
  {
  }

  enum svalue_kind get_kind () const FINAL OVERRIDE { return SK_UNMERGEABLE; }
  const unmergeable_svalue *
  dyn_cast_unmergeable_svalue () const FINAL OVERRIDE { return this; }

  void dump_to_pp (pretty_printer *pp, bool simple) const FINAL OVERRIDE;
  void accept (visitor *v) const FINAL OVERRIDE;
  bool implicitly_live_p (const svalue_set *,
			  const region_model *) const FINAL OVERRIDE;

  const svalue *get_arg () const { return m_arg; }

 private:
  const svalue *m_arg;
};

} // namespace ana

template <>
template <>
inline bool
is_a_helper <const unmergeable_svalue *>::test (const svalue *sval)
{
  return sval->get_kind () == SK_UNMERGEABLE;
}

namespace ana {

/* Concrete subclass of svalue for use in selftests, where
   we want a specific but unknown svalue.
   Unlike other svalue subclasses these aren't managed by
   region_model_manager.  */

class placeholder_svalue : public svalue
{
public:
  placeholder_svalue (tree type, const char *name)
  : svalue (complexity (1, 1), type), m_name (name)
  {
  }

  enum svalue_kind get_kind () const FINAL OVERRIDE { return SK_PLACEHOLDER; }

  void dump_to_pp (pretty_printer *pp, bool simple) const FINAL OVERRIDE;
  void accept (visitor *v) const FINAL OVERRIDE;

  const char *get_name () const { return m_name; }

 private:
  const char *m_name;
};

} // namespace ana

template <>
template <>
inline bool
is_a_helper <placeholder_svalue *>::test (svalue *sval)
{
  return sval->get_kind () == SK_PLACEHOLDER;
}

namespace ana {

/* Concrete subclass of svalue representing a "widening" seen when merging
   states, widening from a base value to {base value, iter value} and thus
   representing a possible fixed point in an iteration from the base to
   +ve infinity, or -ve infinity, and thus useful for representing a value
   within a loop.
   We also need to capture the program_point at which the merger happens,
   so that distinguish between different iterators, and thus handle
   nested loops.  (currently we capture the function_point instead, for
   simplicity of hashing).  */

class widening_svalue : public svalue
{
public:
  /* A support class for uniquifying instances of widening_svalue.  */
  struct key_t
  {
    key_t (tree type, const program_point &point,
	   const svalue *base_sval, const svalue *iter_sval)
    : m_type (type), m_point (point.get_function_point ()),
      m_base_sval (base_sval), m_iter_sval (iter_sval)
    {}

    hashval_t hash () const
    {
      inchash::hash hstate;
      hstate.add_ptr (m_base_sval);
      hstate.add_ptr (m_iter_sval);
      return hstate.end ();
    }

    bool operator== (const key_t &other) const
    {
      return (m_type == other.m_type
	      && m_point == other.m_point
	      && m_base_sval == other.m_base_sval
	      && m_iter_sval == other.m_iter_sval);
    }

    void mark_deleted () { m_type = reinterpret_cast<tree> (1); }
    void mark_empty () { m_type = NULL_TREE; }
    bool is_deleted () const { return m_type == reinterpret_cast<tree> (1); }
    bool is_empty () const { return m_type == NULL_TREE; }

    tree m_type;
    function_point m_point;
    const svalue *m_base_sval;
    const svalue *m_iter_sval;
  };

  enum direction_t
    {
     DIR_ASCENDING,
     DIR_DESCENDING,
     DIR_UNKNOWN
    };

  widening_svalue (tree type, const program_point &point,
		   const svalue *base_sval, const svalue *iter_sval)
  : svalue (complexity::from_pair (base_sval->get_complexity (),
				   iter_sval->get_complexity ()),
	    type),
    m_point (point.get_function_point ()),
    m_base_sval (base_sval), m_iter_sval (iter_sval)
  {
  }

  enum svalue_kind get_kind () const FINAL OVERRIDE { return SK_WIDENING; }
  const widening_svalue *dyn_cast_widening_svalue () const FINAL OVERRIDE
  {
    return this;
  }

  void dump_to_pp (pretty_printer *pp, bool simple) const FINAL OVERRIDE;
  void accept (visitor *v) const FINAL OVERRIDE;

  const function_point &get_point () const { return m_point; }
  const svalue *get_base_svalue () const { return m_base_sval; }
  const svalue *get_iter_svalue () const { return m_iter_sval; }

  enum direction_t get_direction () const;

  tristate eval_condition_without_cm (enum tree_code op,
				      tree rhs_cst) const;

 private:
  function_point m_point;
  const svalue *m_base_sval;
  const svalue *m_iter_sval;
};

} // namespace ana

template <>
template <>
inline bool
is_a_helper <widening_svalue *>::test (svalue *sval)
{
  return sval->get_kind () == SK_WIDENING;
}

template <> struct default_hash_traits<widening_svalue::key_t>
: public member_function_hash_traits<widening_svalue::key_t>
{
  static const bool empty_zero_p = true;
};

namespace ana {

/* Concrete subclass of svalue representing a mapping of bit-ranges
   to svalues, analogous to a cluster within the store.

   This is for use in places where we want to represent a store-like
   mapping, but are required to use an svalue, such as when handling
   compound assignments and compound return values.

   All keys within the underlying binding_map are required to be concrete,
   not symbolic.

   Instances of this class shouldn't be bound as-is into the store;
   instead they should be unpacked.  Similarly, they should not be
   nested.  */

class compound_svalue : public svalue
{
public:
  typedef binding_map::iterator_t iterator_t;

  /* A support class for uniquifying instances of compound_svalue.
     Note that to avoid copies, keys store pointers to binding_maps,
     rather than the maps themselves.  */
  struct key_t
  {
    key_t (tree type, const binding_map *map_ptr)
    : m_type (type), m_map_ptr (map_ptr)
    {}

    hashval_t hash () const
    {
      inchash::hash hstate;
      hstate.add_ptr (m_type);
      //hstate.add_ptr (m_map_ptr); // TODO
      return hstate.end ();
    }

    bool operator== (const key_t &other) const
    {
      return (m_type == other.m_type
	      && *m_map_ptr == *other.m_map_ptr);
    }

    void mark_deleted () { m_type = reinterpret_cast<tree> (1); }
    void mark_empty () { m_type = NULL_TREE; }
    bool is_deleted () const { return m_type == reinterpret_cast<tree> (1); }
    bool is_empty () const { return m_type == NULL_TREE; }

    tree m_type;
    const binding_map *m_map_ptr;
  };

  compound_svalue (tree type, const binding_map &map);

  enum svalue_kind get_kind () const FINAL OVERRIDE { return SK_COMPOUND; }
  const compound_svalue *dyn_cast_compound_svalue () const FINAL OVERRIDE
  {
    return this;
  }

  void dump_to_pp (pretty_printer *pp, bool simple) const FINAL OVERRIDE;
  void accept (visitor *v) const FINAL OVERRIDE;

  const binding_map &get_map () const { return m_map; }

  iterator_t begin () const { return m_map.begin (); }
  iterator_t end () const { return m_map.end (); }

  struct key_t make_key () const
  {
    return key_t (get_type (), &m_map);
  }

 private:
  static complexity calc_complexity (const binding_map &map);

  binding_map m_map;
};

} // namespace ana

template <>
template <>
inline bool
is_a_helper <compound_svalue *>::test (svalue *sval)
{
  return sval->get_kind () == SK_COMPOUND;
}

template <> struct default_hash_traits<compound_svalue::key_t>
: public member_function_hash_traits<compound_svalue::key_t>
{
  static const bool empty_zero_p = true;
};

namespace ana {

/* A defined value arising from a statement, where we want to identify a
   particular unknown value, rather than resorting to the unknown_value
   singleton, so that the value can have sm-state.

   Comparisons of variables that share the same conjured_svalue are known
   to be equal, even if we don't know what the value is.

   For example, this is used for the values of regions that may have been
   touched when calling an unknown function.

   The value captures a region as well as a stmt in order to avoid falsely
   aliasing the various values that could arise in one statement.  For
   example, after:
      unknown_fn (&a, &b);
   we want values to clobber a and b with, but we don't want to use the
   same value, or it would falsely implicitly assume that a == b.  */

class conjured_svalue : public svalue
{
public:
  /* A support class for uniquifying instances of conjured_svalue.  */
  struct key_t
  {
    key_t (tree type, const gimple *stmt, const region *id_reg)
    : m_type (type), m_stmt (stmt), m_id_reg (id_reg)
    {}

    hashval_t hash () const
    {
      inchash::hash hstate;
      hstate.add_ptr (m_type);
      hstate.add_ptr (m_stmt);
      hstate.add_ptr (m_id_reg);
      return hstate.end ();
    }

    bool operator== (const key_t &other) const
    {
      return (m_type == other.m_type
	      && m_stmt == other.m_stmt
	      && m_id_reg == other.m_id_reg);
    }

    /* Use m_stmt to mark empty/deleted, as m_type can be NULL for
       legitimate instances.  */
    void mark_deleted () { m_stmt = reinterpret_cast<const gimple *> (1); }
    void mark_empty () { m_stmt = NULL; }
    bool is_deleted () const
    {
      return m_stmt == reinterpret_cast<const gimple *> (1);
    }
    bool is_empty () const { return m_stmt == NULL; }

    tree m_type;
    const gimple *m_stmt;
    const region *m_id_reg;
  };

  conjured_svalue (tree type, const gimple *stmt, const region *id_reg)
  : svalue (complexity (id_reg), type),
    m_stmt (stmt), m_id_reg (id_reg)
  {
    gcc_assert (m_stmt != NULL);
  }

  enum svalue_kind get_kind () const FINAL OVERRIDE { return SK_CONJURED; }
  const conjured_svalue *dyn_cast_conjured_svalue () const FINAL OVERRIDE
  {
    return this;
  }

  void dump_to_pp (pretty_printer *pp, bool simple) const FINAL OVERRIDE;
  void accept (visitor *v) const FINAL OVERRIDE;

  const gimple *get_stmt () const { return m_stmt; }
  const region *get_id_region () const { return m_id_reg; }

 private:
  const gimple *m_stmt;
  const region *m_id_reg;
};

} // namespace ana

template <>
template <>
inline bool
is_a_helper <conjured_svalue *>::test (svalue *sval)
{
  return sval->get_kind () == SK_CONJURED;
}

template <> struct default_hash_traits<conjured_svalue::key_t>
: public member_function_hash_traits<conjured_svalue::key_t>
{
  static const bool empty_zero_p = true;
};

#endif /* GCC_ANALYZER_SVALUE_H */