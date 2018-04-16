#ifndef YC_REGEXPR_H_
#define YC_REGEXPR_H_

// ========  HEADER FILE FOR MISCELLANEOUS MAX UTILITIES  ========

#include "ext.h"        // header file for all objects, should always be first
#include "ext_obex.h"   // header file for all objects, required for new style Max object
#include "z_dsp.h"      // header file for MSP objects, included here for t_double type

typedef enum _my_err {

  ERR_NULL,    // For initialization, should never be returned
  ERR_NONE,
  ERR_ARG_TYPE,
  ERR_ARG_VALUE,
  ERR_ALLOC,
  ERR_ALREADY_ALLOC,
  ERR_NULL_PTR,
  ERR_NOT_YET_ALLOC,
  ERR_DICT_NONE,
  ERR_DICT_PROTECT,
  ERR_SYNTAX,
  ERR_INDEX,
  ERR_COUNT,
  ERR_STR_LEN,
  ERR_ARR_FULL,
  ERR_ARG0,
  ERR_ARG1,
  ERR_ARG2,
  ERR_ARG3,
  ERR_ARG4,
  ERR_LOCKED,
  ERR_MISC

} t_my_err;

// ========  DEFINES  ========

#define CH_ESCAPE   '/'
#define CH_REP_0_N  '*'
#define CH_REP_1_N  '+'
#define CH_REP_0_1  '?'
#define CH_CONCAT   '~'
#define CH_ALTERN   '|'
#define CH_WILDCARD '.'
#define CH_PAREN_L  '('
#define CH_PAREN_R  ')'

#define STACK_OPER(ch) *++(regexpr->oper_iter) = (ch);

#define TRACE_L(...) do { if (0) object_post(g_object, "TRACE:  " __VA_ARGS__); } while (0)
#define POST_L(...) do { object_post(g_object, __VA_ARGS__); } while (0)
#define ERR_L(_err, _ret, ...) do { object_error(g_object, __VA_ARGS__);\
  regexpr->err = _err; return _ret; } while (0)

// ====  STRUCTURE DECLARATIONS  ====

typedef t_uint8 t_nfa_ind;       // Index type for the search expression and nfa
typedef t_uint16 t_string_ind;   // Index type for the other strings

extern const t_nfa_ind IND_NULL;   // used like a NULL pointer for indexes

// ========  ENUM  ========

//******************************************************************************
//  Enum to characterize the different types of states
//
//  Stored in the t_state type field
//
typedef enum _state_type {

  ST_CHAR,      // An ordinary character
  ST_BRACKET,   // A char class defined in a bracketed expression
  ST_BRANCH,    // A branching state, for repetition or alternation
  ST_PAREN,     // A parenthesis state, left or right depends on the value
  ST_END,       // The ending state

  ST_CH_CLASS_NONE,
  ST_CH_CLASS_ANY,
  ST_CH_CLASS_DIGIT,
  ST_CH_CLASS_NOT_DIGIT,
  ST_CH_CLASS_ALPHA,
  ST_CH_CLASS_NOT_ALPHA,
  ST_CH_CLASS_LOWER,
  ST_CH_CLASS_NOT_LOWER,
  ST_CH_CLASS_UPPER,
  ST_CH_CLASS_NOT_UPPER,
  ST_CH_CLASS_WORD,
  ST_CH_CLASS_NOT_WORD,
  ST_CH_CLASS_SPACE,
  ST_CH_CLASS_NOT_SPACE,

  ST_NULL   // Used to initialize the state array

} e_state;

typedef enum _operator {

  OP_NULL = 10,   // 0 to 9 used to for parenthesis counters
  OP_ALTERN,
  OP_ANCHOR_BEG,
  OP_ANCHOR_END,
  OP_CONCAT,
  OP_REPEAT,
  OP_PAREN_L,
  OP_PAREN_R,
  OP_BRACKET_L,
  OP_BRACKET_R,
  OP_BEGIN,
  OP_VALUE,
  OP_LAST

} e_operator;

//******************************************************************************
//  Function pointer type used for the predefined character classes
//
//  A constant extern array is declared in the header file
//  and defined in the source file
//
typedef t_bool(*t_match)(char match_c, char ref);
extern const t_match match_arr[];

// ========  STRUCTURES  ========

typedef struct _fragment {

  t_nfa_ind first;      // The index to the first state of the fragment
  t_nfa_ind term_beg;   // Beginning of the linked list of terminal links
  t_nfa_ind term_end;   // End of the linked list of terminal links

} t_fragment;

typedef union _state_misc {

  char value;
  t_nfa_ind ind2;

} u_state_misc;

#define U_VAL(_val_) (u_state_misc){ .value = (_val_) }
#define U_IND(_ind_) (u_state_misc){ .ind2 = (_ind_) }

typedef struct _state {

  t_uint8 type;
  t_nfa_ind ind1;
  u_state_misc u;
  t_uint8 gen_cnt;

} t_state;

// @TODO could reuse fragment stack maybe
typedef struct _simul {

  t_nfa_ind state_ind;
  t_nfa_ind set_ind;

} t_simul;

typedef struct _regexp2 {

  // VARIABLES SET AT COMPILATION AND USED IN THE SIMULATION

  t_nfa_ind length_max;         // The maximum length of strings that can be processed

  t_nfa_ind state_cnt;          // The number of states used
  t_nfa_ind state_max;          // The maximum number of states
  t_nfa_ind state_first;        // The first state in the NFA
  t_nfa_ind state_last;         // The last state in the NFA
  t_nfa_ind state_first_free;   // The first state in the linked list of free states

//******************************************************************************
//  Array of states: organised as a non-deterministic finite automata (NFA)
//  The size of the array should be at least (n + 1).
//  An ending state is added.
//
  t_state* state_arr;

//******************************************************************************
//  All the substrings from the replace expression:
//  Processed at compilation.
//  The substrings are separated by '\0' and a capture index
//
  char*        repl_sub_s;    // a string holding the sub string
  t_string_ind repl_sub_max;  // the maximum length allocated
  t_string_ind repl_sub_cnt;  // the number of substrings

  // ====  TEMPORARY SIMULATION VARIABLES  ====

//******************************************************************************
//  Match string:
//  The string to be matched to the regular expression.
//  There is no copying, just a stored a pointer to the existing string.
//
  const char* match_iter;
  t_string_ind match_ind;

//******************************************************************************
//  Replace string:
//  Concatenated when requested in the simulation phase.
//
  char* replace_s;
  char* replace_p;  // Points to NULL, repl_sub_s, or replace_s
  t_string_ind replace_max;

//******************************************************************************
//  Stacks of routines
//
  t_simul* routine_cur;
  t_simul* routine_new;
  t_simul* rnew_iter;
  t_uint8 gen_cnt;

//******************************************************************************
//  Capture variables and arrays
//
  t_uint16 capt_flags;          // flags indicating which capture groups are used

  t_uint8 capt_all_to_used[10]; // to use minimal length capture sets

  t_uint8 paren_cnt;            // the number of parentheses pairs in the search expression
  t_uint8 capt_cnt;             // the number of capture groups actually used

  t_string_ind* capt_set_arr;   // the beginning and endings of capture sets
  t_nfa_ind* capt_cnt_arr;      // the number of references for each capture set
  t_nfa_ind capt_free_ind;      // the index of the first free set
  t_nfa_ind capt_end_ind;       // the index of the set referenced on ending

  // ====  TEMPORARY COMPILATION VARIABLES  ====

//******************************************************************************
//  Search string:
//  The maximum lengths of other arrays depends on this length, noted n.
//  No copying. The structure just stores a pointer to an existing string.
//
  const char* re_search_s;
  const char* re_search_iter;

//******************************************************************************
//  Fragment stack:
//  The first element is initialized to a null fragment.
//  The size of the stack should be at least (E(n/2) + 2).
//  Worst case, counting invalid cases, is "a(b(cd...".
//
  t_fragment* frag_arr;
  t_fragment* frag_iter;   // Points to the current fragment

//******************************************************************************
//  Operator stack:
//  The first element is initialized to OP_NULL.
//  The size of the stack should be at least (n + 1).
//  Worst case, counting invalid cases, is "(((".
//  Only holds:  using OP_CONCAT, OP_ALTERN or OP_LAST + (0-10) for parentheses.
//  (  (|  (~  (|~
//
  t_uint8* oper_arr;
  t_uint8* oper_iter;   // Points to the current operator

//******************************************************************************
//  Reverse polish notation string:
//  The size of the string should be at least (2 * n)
//  Worst case is abc...: ab~c~...\0
//
  char* rpn_s;
  char* rpn_iter;

//******************************************************************************
//  Track when a subexpression has its first value, to know when to concatenate
//  set to false when a value is added
//  set to true initially, after '(', and after CH_ALTERN
//  concatenate when encountering a value or '(', and is_first is false
//
  t_bool is_first;

//******************************************************************************
//  Track the type of the previous character type to detect invalid character pairs
//  (),  |),  ([*+?],  [*+?][*+?],  |[*+?],  (|,  ||
//  set prev_type to OP_BEGIN initially, and for value tokens
//
  t_uint8 prev_type;

  // ====  MISC  ====

  t_my_err err;   // Used for error control

} t_regexp2;

typedef void(*t_simul_state)(t_regexp2* regexpr, t_state* state, t_nfa_ind set_ind);

#define CAPT_IND(_set_ind) (regexpr->capt_set_arr + regexpr->capt_cnt * (_set_ind))
#define CAPT_CNT(_set_ind) (*(regexpr->capt_cnt_arr + (_set_ind)))

// ========  FUNCTION DECLARATIONS  ========

t_regexp2* re_new (t_nfa_ind max);
void re_init      (t_regexp2* regexpr, t_nfa_ind max);
void re_reset     (t_regexp2* regexpr);
void re_free      (t_regexp2** regexpr);
void re_empty     (t_regexp2* regexpr);

void re_set_object (void* object);

t_nfa_ind state_new (t_regexp2* regexpr, t_uint8 type, t_nfa_ind ind1, u_state_misc u);
void state_post     (t_regexp2* regexpr);

void frag_set     (t_fragment* frag, t_nfa_ind first, t_nfa_ind term_beg, t_nfa_ind term_end);
void frag_connect (t_regexp2* regexpr, t_fragment* frag, t_nfa_ind to_state);
void frag_post    (t_regexp2* regexpr);

void frag_new_val     (t_regexp2* regexpr, e_state type, char value);
void frag_new_repeat  (t_regexp2* regexpr);
void frag_new_altern  (t_regexp2* regexpr);
void frag_new_concat  (t_regexp2* regexpr);
void frag_new_parenth (t_regexp2* regexpr);

t_int32 re_compile_replace1 (t_regexp2* regexpr, const char* const re_replace_s);
void re_compile_search   (t_regexp2* regexpr, const char* const re_search_s);
void re_compile_replace2 (t_regexp2* regexpr, const char* const re_replace_s);
void re_compile          (t_regexp2* regexpr, const char* const re_search_s, const char* const re_replace_s);

void re_simul_state_nc (t_regexp2* regexpr, t_state* state, t_nfa_ind set_ind);
void re_simul_state_wc (t_regexp2* regexpr, t_state* state, t_nfa_ind set_ind);
void re_simul_replace  (t_regexp2* regexpr, const char* const match_s);
t_bool re_simulate     (t_regexp2* regexpr, const char* const match_s);

//******************************************************************************
//  Boolean functions used for the predefined character classes
//
t_bool st_match_char      (char match_c, char ref_c);
t_bool st_match_end       (char match_c, char ref_c);

t_bool st_match_none      (char match_c, char ref_c);
t_bool st_match_any       (char match_c, char ref_c);
t_bool st_match_digit     (char match_c, char ref_c);
t_bool st_match_not_digit (char match_c, char ref_c);
t_bool st_match_alpha     (char match_c, char ref_c);
t_bool st_match_not_alpha (char match_c, char ref_c);
t_bool st_match_lower     (char match_c, char ref_c);
t_bool st_match_not_lower (char match_c, char ref_c);
t_bool st_match_upper     (char match_c, char ref_c);
t_bool st_match_not_upper (char match_c, char ref_c);
t_bool st_match_word      (char match_c, char ref_c);
t_bool st_match_not_word  (char match_c, char ref_c);
t_bool st_match_space     (char match_c, char ref_c);
t_bool st_match_not_space (char match_c, char ref_c);

// ========  OLD  ========

struct _regexpr;
typedef struct _regexpr t_regexpr;

typedef t_bool (*t_regexpr_match)(t_regexpr* expr, t_symbol* match_sym);

struct _regexpr {

  t_symbol* search_sym;        // the original search expression as a symbol

  t_symbol* search_frag_sym;   // the search fragment as a symbol
  t_int32    search_frag_len;  // the search fragment's length
  char*     search_frag_s;     // the search fragment as a string

  char type_beg;
  char type_end;

  t_regexpr_match match_fct;
};

// ====  PROCEDURE DECLARATIONS  ====

t_regexpr* regexpr_new();

void     regexpr_reset (t_regexpr* expr);
t_my_err regexpr_set   (t_regexpr* expr, t_symbol* search_sym);
void     regexpr_free  (t_regexpr* expr);
t_bool   regexpr_match (t_regexpr* expr, t_symbol* match_sym);

t_bool _regexpr_match_true  (t_regexpr* expr, t_symbol* match_sym);
t_bool _regexpr_match_false (t_regexpr* expr, t_symbol* match_sym);
t_bool _regexpr_match_reg   (t_regexpr* expr, t_symbol* match_sym);
t_bool _regexpr_match_beg   (t_regexpr* expr, t_symbol* match_sym);
t_bool _regexpr_match_end   (t_regexpr* expr, t_symbol* match_sym);
t_bool _regexpr_match_mid   (t_regexpr* expr, t_symbol* match_sym);

t_bool _regexpr_match_in_forward  (char* search_frag_s, char* match_s);
t_bool _regexpr_match_in_backward (char* search_frag_s, char* match_s, t_int32 search_frag_len, t_int32 match_len);

// ========  END OF HEADER FILE  ========

#endif
