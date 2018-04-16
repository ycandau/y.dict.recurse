#include "regexpr.h"

// @TODO:
// dynamic strings
// bracket expressions
// anchoring: ^, $, \b, \B
// separate temporary and non temporary allocation
// if resize ->replace_s, update ->replace_p
// test after reinstallation

// Set the constant value for IND_NULL, used as a NULL index value
const t_nfa_ind IND_NULL = (t_nfa_ind)(~0);

// A pointer to the object for object_post()
t_object* g_object = NULL;

//******************************************************************************
//  Constant array of function pointers used for matching states
//
const t_match match_arr[ST_NULL] = {

  [ST_CHAR]    = st_match_char,
  [ST_BRACKET] = st_match_any,
  [ST_BRANCH]  = st_match_any,
  [ST_PAREN]   = st_match_any,
  [ST_END]     = st_match_end,

  [ST_CH_CLASS_NONE]      = st_match_none,
  [ST_CH_CLASS_ANY]       = st_match_any,
  [ST_CH_CLASS_DIGIT]     = st_match_digit,
  [ST_CH_CLASS_NOT_DIGIT] = st_match_not_digit,
  [ST_CH_CLASS_ALPHA]     = st_match_alpha,
  [ST_CH_CLASS_NOT_ALPHA] = st_match_not_alpha,
  [ST_CH_CLASS_LOWER]     = st_match_lower,
  [ST_CH_CLASS_NOT_LOWER] = st_match_not_lower,
  [ST_CH_CLASS_UPPER]     = st_match_upper,
  [ST_CH_CLASS_NOT_UPPER] = st_match_not_upper,
  [ST_CH_CLASS_WORD]      = st_match_word,
  [ST_CH_CLASS_NOT_WORD]  = st_match_not_word,
  [ST_CH_CLASS_SPACE]     = st_match_space,
  [ST_CH_CLASS_NOT_SPACE] = st_match_not_space
};

// ====  REGEXPR  ====

//******************************************************************************
//  Create and allocate a new regular expression structure.
//
//  @param max The size of the array of states in the regular expression.
//
//  @return A pointer to the newly allocated a structure, or NULL on failure.
//
t_regexp2* re_new(t_nfa_ind max) {

  TRACE_L("re_new");

  // Allocate the structure, and check the allocation
  t_regexp2* regexpr = (t_regexp2*)sysmem_newptr(sizeof(t_regexp2));
  if (!regexpr) { return NULL; }

  // Initialize the structure
  re_init(regexpr, max);

  // Test the initialization
  if (regexpr->err != ERR_NONE) { sysmem_freeptr(regexpr); return NULL; }

  return regexpr;
}

//******************************************************************************
//  Initialize a regular expression structure.
//
//  Allocates then sets all the basic types.
//
//  @param regexpr A pointer to the regular expression structure.
//  @param max The maximum length of strings that can be processed.
//
//  Note: .err set to ERR_ALLOC or ERR_INDEX on failure, ERR_NONE otherwise.
//
void re_init(t_regexp2* regexpr, t_nfa_ind max) {

  TRACE_L("re_init");

  // Test the maximum length argument
  if ((max <= 1) || (max >= IND_NULL)) {
    re_empty(regexpr);
    ERR_L(ERR_INDEX, ,"re_init:  Invalid length argument:  %i - Should be:  %i to %i",
      max, 1, IND_NULL - 1);
    }

  regexpr->length_max = max;

  // For all arrays: initialize the pointers to NULL
  regexpr->state_arr = NULL;
  regexpr->frag_arr = NULL;
  regexpr->oper_arr = NULL;
  regexpr->rpn_s = NULL;
  regexpr->repl_sub_s = NULL;
  regexpr->replace_s = NULL;
  regexpr->routine_cur = NULL;
  regexpr->routine_new = NULL;
  regexpr->capt_set_arr = NULL;
  regexpr->capt_cnt_arr = NULL;

  // For all arrays: set the size, allocate, and check the allocation

  // Array of states
  regexpr->state_max = regexpr->length_max + 1;
  regexpr->state_arr = (t_state*)sysmem_newptr(sizeof(t_state) * regexpr->state_max);
  if (!regexpr->state_arr) { goto RE_INIT_END; }

  // Stack of fragments
  regexpr->frag_arr = (t_fragment*)sysmem_newptr(sizeof(t_fragment)
    * (regexpr->length_max / 2 + 2));
  if (!regexpr->frag_arr) { goto RE_INIT_END; }

  // Stack of operators
  regexpr->oper_arr = (t_uint8*)sysmem_newptr(sizeof(t_uint8) * (regexpr->length_max + 1));
  if (!regexpr->oper_arr) { goto RE_INIT_END; }

  // The search expression in reverse Polish notation
  regexpr->rpn_s = (char*)sysmem_newptr(sizeof(char) * regexpr->length_max * 2);
  if (!regexpr->rpn_s) { goto RE_INIT_END; }

  // A string to hold all the substrings from the replace string
  // resized when necessary
  regexpr->repl_sub_max = max;
  regexpr->repl_sub_s = (char*)sysmem_newptr(sizeof(char) * regexpr->repl_sub_max);
  if (!regexpr->repl_sub_s) { goto RE_INIT_END; }

  // A string to hold the assembled replace string
  // resized when necessary
  regexpr->replace_max = max;
  regexpr->replace_s = (char*)sysmem_newptr(sizeof(char) * regexpr->replace_max);
  if (!regexpr->replace_s) { goto RE_INIT_END; }

  // Stacks of routines
  regexpr->routine_cur = (t_simul*)sysmem_newptr(sizeof(t_simul) * regexpr->state_max);
  if (!regexpr->routine_cur) { goto RE_INIT_END; }
  regexpr->routine_new = (t_simul*)sysmem_newptr(sizeof(t_simul) * regexpr->state_max);
  if (!regexpr->routine_new) { goto RE_INIT_END; }

  // An array to hold information on the capture groups
  regexpr->capt_set_arr = (t_string_ind*)sysmem_newptr(sizeof(t_string_ind) * 20 * regexpr->state_max);
  if (!regexpr->capt_set_arr) { goto RE_INIT_END; }
  regexpr->capt_cnt_arr = (t_nfa_ind*)sysmem_newptr(sizeof(t_nfa_ind) * regexpr->state_max);
  if (!regexpr->capt_cnt_arr) { goto RE_INIT_END; }

  // Initialize the structure's members
  re_reset(regexpr);
  return;

  // In case there was an allocation error
RE_INIT_END:
  re_empty(regexpr);
  ERR_L(ERR_ALLOC, , "re_init:  Allocation error");
}

//******************************************************************************
//  Reset a regular expression structure.
//
//  Called from re_compile() each time to reset all the compilation structures.
//  Resets all values, but allocation is unchanged.
//
//  @param regexpr A pointer to the regular expression structure.
//
void re_reset(t_regexp2* regexpr) {

  TRACE_L("re_reset");

  // Check the error status
  if (!regexpr->length_max) {
    ERR_L(ERR_INDEX, , "re_reset:  Empty structure. The arrays are not allocated.");
  }

  // ====  Replace compilation  ====

  regexpr->capt_flags = 0;
  regexpr->repl_sub_cnt = 1;

  // ====  Search compilation:  States  ====

  regexpr->state_cnt = 0;
  regexpr->state_first = IND_NULL;
  regexpr->state_last = IND_NULL;
  regexpr->state_first_free = 0;
  regexpr->err = ERR_NONE;

  // Reset the array of states
  // It is organized as a linked list of free states, starting at state_first_free,
  // following the ind1 indexes, and with a terminal IND_NULL value
  t_state* state;
  for (t_nfa_ind ind = 0; ind < regexpr->state_max; ind++) {
    state = regexpr->state_arr + ind;
    state->type = ST_NULL;
    state->ind1 = ind + 1;
    state->u.ind2 = IND_NULL;
    state->gen_cnt = 0;
  }

  // Set the last state link to NULL
  state->ind1 = IND_NULL;

  // ==== Search compilation:  Other  ====

  // Trailing variables
  regexpr->is_first = true;
  regexpr->prev_type = OP_BEGIN;

  // Counters
  regexpr->paren_cnt = 0;
  regexpr->capt_cnt = 0;

  // Pointers
  regexpr->re_search_iter = regexpr->re_search_s;
  regexpr->frag_iter = regexpr->frag_arr;
  regexpr->oper_iter = regexpr->oper_arr;
  regexpr->rpn_iter = regexpr->rpn_s;

  // Set the first fragments and operators to NULL
  frag_set(regexpr->frag_iter, IND_NULL, IND_NULL, IND_NULL);
  *regexpr->oper_iter = OP_NULL;
}

//******************************************************************************
//  Free a regular expression structure and set its pointer to NULL.
//
//  @param regexpr A pointer to a pointer to the regular expression structure.
//
//  Note: No need to check if the pointer argument is NULL.
//
void re_free(t_regexp2** regexpr) {

  TRACE_L("re_free");

  // If the pointer is NULL do nothing
  if (!regexpr) { return; }

  // Empty the structure
  re_empty(*regexpr);

  // Free the structure pointer to NULL
  if (*regexpr) {  sysmem_freeptr(*regexpr);  *regexpr = NULL; }
}

//******************************************************************************
//  Empty a regular expression structure.
//
//  @param regexpr A pointer to the regular expression structure.
//
//  Note: No need to check if the pointer argument is NULL.
//
void re_empty(t_regexp2* regexpr) {

  TRACE_L("re_empty");

  // If the pointer is NULL do nothing
  if (!regexpr) { return; }

  // Free the array members if necessary
  if (regexpr->state_arr) { sysmem_freeptr(regexpr->state_arr);  regexpr->state_arr = NULL; }
  if (regexpr->frag_arr) { sysmem_freeptr(regexpr->frag_arr);  regexpr->frag_arr = NULL; }
  if (regexpr->oper_arr) { sysmem_freeptr(regexpr->oper_arr);  regexpr->oper_arr = NULL; }
  if (regexpr->rpn_s) { sysmem_freeptr(regexpr->rpn_s);  regexpr->rpn_s = NULL; }
  if (regexpr->repl_sub_s) { sysmem_freeptr(regexpr->repl_sub_s);  regexpr->repl_sub_s = NULL; }
  if (regexpr->replace_s) { sysmem_freeptr(regexpr->replace_s);  regexpr->replace_s = NULL; }
  if (regexpr->routine_cur) { sysmem_freeptr(regexpr->routine_cur);  regexpr->routine_cur = NULL; }
  if (regexpr->routine_new) { sysmem_freeptr(regexpr->routine_new);  regexpr->routine_new = NULL; }
  if (regexpr->capt_set_arr) { sysmem_freeptr(regexpr->capt_set_arr);  regexpr->capt_set_arr = NULL; }
  if (regexpr->capt_cnt_arr) { sysmem_freeptr(regexpr->capt_cnt_arr);  regexpr->capt_cnt_arr = NULL; }

  // Set the maximum length to 0
  regexpr->length_max = 0;  // Indicates that the structure is empty
}

//******************************************************************************
//  Set the object which uses the regular expression library.
//  Useful for object_post() for instance.
//
//  @param x A pointer to the object.
//
void re_set_object(void* object) {

  TRACE_L("re_set_object");

  g_object = (t_object*)object;
}

//******************************************************************************
//  Create a new state.
//
//  @param regexpr A pointer to the regular expression structure.
//  @param type The state's type, taken from e_state.
//  @param ind1 The first link to another state, as an index.
//  @param u A union for an index or a char value.
//
//  @return The index of the new state, or IND_NULL if there are no remaining free states.
//
//  Note: There is no actual allocation. All states are allocated as an array.
//  The function gets the first state from the linked list of free states.
//  .u.ind2 is used for .type = ST_BRANCH, otherwise .u.value is used.
//
t_nfa_ind state_new(t_regexp2* regexpr, t_uint8 type, t_nfa_ind ind1, u_state_misc u) {

  // If there are no remaining free states
  if (regexpr->state_first_free == IND_NULL) {
    ERR_L(ERR_ARR_FULL, IND_NULL, "state_new:  No more empty states to compile the automaton");
  }

  // Get the first state from the linked list of free states
  t_nfa_ind ind = regexpr->state_first_free;
  t_state* state = regexpr->state_arr + ind;
  regexpr->state_first_free = state->ind1;

  // Set the state values
  state->type = type;
  state->ind1 = ind1;
  state->u = u;
  state->gen_cnt = 0;

  // Iterate the state count
  regexpr->state_cnt++;

  return ind;
}

//******************************************************************************
//  Post information on all the states.
//
//  @param regexpr A pointer to the regular expression structure.
//
void state_post(t_regexp2* regexpr) {

  POST_L("RE States:  Count: %i - Max: %i - First: %i - Last: %i - First free: %i",
    regexpr->state_cnt, regexpr->state_max, regexpr->state_first, regexpr->state_last, regexpr->state_first_free);

  // Loop through the states in use
  t_state* state = NULL;
  for (t_nfa_ind ind = 0; ind < regexpr->state_cnt; ind++) {
    state = regexpr->state_arr + ind;

    switch (state->type) {

    case ST_BRANCH:
      POST_L("  S[%i]:  Type: %i - Ind1: %i - Ind2: %i",
      ind, state->type, state->ind1, state->u.ind2);
      break;

    default:
      POST_L("  S[%i]:  Type: %i - Ind1: %i - Value: %c",
        ind, state->type, state->ind1, state->u.value);
      break;
    }
  }
}

//******************************************************************************
//  Set the values of a fragment.
//
//  @param frag A pointer to the fragment.
//  @param first The index of the first state in the fragment.
//  @param term_beg The index of the state holding the first terminal link.
//  @param term_end The index of the state holding the last terminal link.
//
void frag_set(t_fragment* frag, t_nfa_ind first, t_nfa_ind term_beg, t_nfa_ind term_end) {

  frag->first = first;
  frag->term_beg = term_beg;
  frag->term_end = term_end;
}

//******************************************************************************
//  Connect the terminal links of a fragment to a state.
//
//  @param regexpr A pointer to the regular expression structure.
//  @param frag A pointer to the fragment.
//  @param to_state The index of the state.
//
void frag_connect(t_regexp2* regexpr, t_fragment* frag, t_nfa_ind to_state) {

  t_nfa_ind ind = frag->term_beg;   // an index to loop through the terminal links
  t_state* state = NULL;            // a state pointer to the corresponding state

  // Loop through the list of terminal links
  while (ind != IND_NULL) {
    state = regexpr->state_arr + ind;  // get the state corresponding to the index
    ind = state->ind1;                 // iterate the index
    state->ind1 = to_state;
  }  // connect the terminal link to the state

  frag->term_beg = IND_NULL;           // the list of terminal links is now empty
  frag->term_end = IND_NULL;
}

//******************************************************************************
//  Post information on all the fragments.
//
//  @param regexpr A pointer to the regular expression structure.
//
void frag_post(t_regexp2* regexpr) {

  POST_L("RE Fragments:  Count: %i", regexpr->frag_iter - regexpr->frag_arr + 1);

  // Loop through the fragments
  t_fragment* frag = NULL;;
  for (t_nfa_ind ind = 0; ind < regexpr->frag_iter - regexpr->frag_arr + 1; ind++) {
    frag = regexpr->frag_arr + ind;
    POST_L("  F[%i]:  First: %i - Term beg: %i - Term end: %i",
      ind, frag->first, frag->term_beg, frag->term_end);
    }
}

//******************************************************************************
//  Create a new value state and a new fragment containing just that state.
//
//  @param regexpr A pointer to the regular expression structure.
//  @param type The type of the state (ST_CHAR or ST_CH_CLASS).
//  @param value The character to store in the state value.
//
//  Note: The fragment stack is incremented.
//
void frag_new_val(t_regexp2* regexpr, e_state type, char value) {

  // == A new value might imply a concatenation

  // Add a concatenation operator to the operator stack
  // if there is a preceding value: not first, or following '(', or following '|'
  if (!regexpr->is_first) {

    // Unstack any previous concatenation, and stack a new concatenation
    if (*regexpr->oper_iter == OP_CONCAT) { frag_new_concat(regexpr); }
    STACK_OPER(OP_CONCAT);
  }

  // == Back to adding a value to the fragment stack

  // Create a new fragment containing just one value state
  regexpr->frag_iter++;
  regexpr->frag_iter->first = state_new(regexpr, type, IND_NULL, U_VAL(value) );
  regexpr->frag_iter->term_beg = regexpr->frag_iter->first;
  regexpr->frag_iter->term_end = regexpr->frag_iter->first;

  // Set the trailing variables
  regexpr->is_first = false;       // There is now a preceding value
  regexpr->prev_type = OP_VALUE;   // The previous type is a value

  // Build the reverse polish notation string  @OPTION
  if ((type != ST_CHAR) && (type != ST_CH_CLASS_ANY)) {
    *regexpr->rpn_iter++ = CH_ESCAPE;
  }
  *regexpr->rpn_iter++ = value;
}

//******************************************************************************
//  Create a new branch state and add a repetition to the current fragment.
//
//  @param regexpr A pointer to the regular expression structure.
//
void frag_new_repeat(t_regexp2* regexpr) {

  // Check for invalid character pairs:  ([*+?]  [*+?][*+?]  |[*+?]  ^[*+?]
  switch (regexpr->prev_type) {

  case OP_BEGIN:
    ERR_L(ERR_SYNTAX, , "RE Compile:  Syntax error at 1:  Invalid first char:  %c", *regexpr->re_search_iter);

  case OP_PAREN_L:  case OP_REPEAT:  case OP_ALTERN:
    ERR_L(ERR_SYNTAX, , "RE Compile:  Syntax error at %i:  Invalid sequence:  %c%c",
      regexpr->re_search_iter - regexpr->re_search_s, *(regexpr->re_search_iter - 1), *regexpr->re_search_iter);
    }

  // There should be a preceding value (this is likely redundant)
  if (regexpr->is_first == true) {
    ERR_L(ERR_SYNTAX, , "RE Compile:  Syntax error at %i:  No value before %c",
      regexpr->re_search_iter - regexpr->re_search_s + 1, *regexpr->re_search_iter);
    }

  // Set the previous type
  regexpr->prev_type = OP_REPEAT;

  // Build the reverse polish notation string  @OPTION
  *regexpr->rpn_iter++ = *regexpr->re_search_iter;

  // Switch through the different types of repetitions
  switch (*regexpr->re_search_iter) {
  t_nfa_ind state_ind;    // temporary variable to store the new state's index

  case CH_REP_0_N:
    // Create a new branch state, and connect it to the beginning of the fragment
    state_ind = state_new(regexpr, ST_BRANCH, IND_NULL, U_IND(regexpr->frag_iter->first));

    // Connect the current fragment to the branch state
    frag_connect(regexpr, regexpr->frag_iter, state_ind);
    // The current fragment now begins at the branch state
    regexpr->frag_iter->first = state_ind;
    // The list of terminal links now consists of just the first branch state link
    regexpr->frag_iter->term_beg = state_ind;
    regexpr->frag_iter->term_end = state_ind;
    break;

  case CH_REP_1_N:
    // Create a new branch state, and connect it to the beginning of the fragment
    state_ind = state_new(regexpr, ST_BRANCH, IND_NULL, U_IND(regexpr->frag_iter->first));

    // Connect the current fragment to the branch state
    frag_connect(regexpr, regexpr->frag_iter, state_ind);
    // The beginning of the current fragment is unchanged
    // The list of terminal links now consists of just the first branch state link
    regexpr->frag_iter->term_beg = state_ind;
    regexpr->frag_iter->term_end = state_ind;
    break;

  case CH_REP_0_1:
    // Create a new branch state, connect it to the beginning of the fragment
    // and add the first link to the list of terminal links
    state_ind = state_new(regexpr, ST_BRANCH, regexpr->frag_iter->term_beg, U_IND(regexpr->frag_iter->first));

    // The current fragment now begins at the branch state
    regexpr->frag_iter->first = state_ind;
    // The list of terminal links now begins at the first branch state link
    // The end is unchanged
    regexpr->frag_iter->term_beg = state_ind;
    break;
  }

  return;
}

//******************************************************************************
//  Create a new branch state and add an alternation,
//  connecting the previous and current fragments.
//
//  @param regexpr A pointer to the regular expression structure.
//
//  Note: The operator and fragment stacks are decremented.
//
void frag_new_altern(t_regexp2* regexpr) {

  // Decrement the stack of fragments and the stack of operators
  regexpr->frag_iter--;
  regexpr->oper_iter--;

  // Create a new branch state, connect it to the beginning of
  // the current and previous fragments, and decrement the stack of fragments
  // ind2 precedes ind1, to conform to the precedence in repeating states
  t_nfa_ind state_ind = state_new(regexpr, ST_BRANCH,
    (regexpr->frag_iter + 1)->first, U_IND(regexpr->frag_iter->first));

  // The current fragment now begins at the branch state
  regexpr->frag_iter->first = state_ind;
  // Concatenate the two fragment's terminal links lists
  // Connect the last element of one list to the first of the other list
  (regexpr->state_arr + regexpr->frag_iter->term_end)->ind1 = (regexpr->frag_iter + 1)->term_beg;
  // The list of terminal links now ends at the last element of the other list
  // The beginning is unchanged
  regexpr->frag_iter->term_end = (regexpr->frag_iter + 1)->term_end;

  // Reverse polish notation string  @OPTION
  *regexpr->rpn_iter++ = CH_ALTERN;
}

//******************************************************************************
//  Concatenate the previous and current fragments.
//
//  @param regexpr A pointer to the regular expression structure.
//
//  Note: The operator and fragment stacks are decremented.
//
void frag_new_concat(t_regexp2* regexpr) {

  // Decrement the stack of fragments and the stack of operators
  regexpr->frag_iter--;
  regexpr->oper_iter--;

  frag_connect(regexpr, regexpr->frag_iter, (regexpr->frag_iter + 1)->first);

  regexpr->frag_iter->term_beg = (regexpr->frag_iter + 1)->term_beg;
  regexpr->frag_iter->term_end = (regexpr->frag_iter + 1)->term_end;

  // Reverse polish notation string  @OPTION
  *regexpr->rpn_iter++ = CH_CONCAT;
}

//******************************************************************************
//  Enclose the fragment in parentheses.
//
//  @param regexpr A pointer to the regular expression structure.
//
//  Note: Only for capture groups requested in the replace string.
//
void frag_new_parenth(t_regexp2* regexpr) {

  t_nfa_ind left_ind = state_new(regexpr, ST_PAREN,
    regexpr->frag_iter->first, U_IND(2 * *regexpr->oper_iter));
  t_nfa_ind right_ind = state_new(regexpr, ST_PAREN,
    IND_NULL, U_IND(2 * *regexpr->oper_iter + 1));

  frag_connect(regexpr, regexpr->frag_iter, right_ind);

  regexpr->frag_iter->first = left_ind;
  regexpr->frag_iter->term_beg = right_ind;
  regexpr->frag_iter->term_end = right_ind;

  // Decrement the stack of operators
  regexpr->oper_iter--;

  // Reverse polish notation string  @OPTION
  *regexpr->rpn_iter++ = CH_PAREN_R;
}

//******************************************************************************
//  First compilation phase for the replace expression.
//
//  The function scans the string once to determine which capture groups are requested.
//
//  Sets: capture_flags.
//
//  @param regexpr A pointer to the regular expression structure.
//  @param re_replace_s A pointer to the replace expression.
//
//  @return The length of the string.
//
t_int32 re_compile_replace1(t_regexp2* regexpr, const char* const re_repl_s) {

  // Initialize the string iterator and counter
  const char* re_repl_iter = re_repl_s;
  t_int32 cnt = 0;

  // Loop through the replace string
  while (*re_repl_iter) {

    // Look for capture group requests
    if (*re_repl_iter == CH_ESCAPE) {

      re_repl_iter++; cnt++;

      if ((*re_repl_iter >= '0') && (*re_repl_iter <= '9')) {
        regexpr->capt_flags |= 1 << (*re_repl_iter - '0');
      }

      if (*re_repl_iter == '\0') { continue; }
    }  // In case of invalid terminal '/'

    re_repl_iter++; cnt++;
  }

  return cnt;
}

//******************************************************************************
//  Compile the search expression of a RE into an NFA.
//
//  Sets: paren_cnt, capt_cnt, state_arr, state_cnt, state_first, state_last.
//
//  @param regexpr A pointer to the regular expression structure.
//  @param str A pointer to the regular expression search expression.
//
void re_compile_search(t_regexp2* regexpr, const char* const re_search_s) {

  // Loop through the characters of the regular expression
  while (*regexpr->re_search_iter && (regexpr->err == ERR_NONE)) {
    switch (*regexpr->re_search_iter) {

    // ==== Escape sequences and character classes
    case CH_ESCAPE:
      regexpr->re_search_iter++;
      switch (*regexpr->re_search_iter) {

      // == Character classes
      case 'd': frag_new_val(regexpr, ST_CH_CLASS_DIGIT, *regexpr->re_search_iter); break;
      case 'D': frag_new_val(regexpr, ST_CH_CLASS_NOT_DIGIT, *regexpr->re_search_iter); break;
      case 'a': frag_new_val(regexpr, ST_CH_CLASS_ALPHA, *regexpr->re_search_iter); break;
      case 'A': frag_new_val(regexpr, ST_CH_CLASS_NOT_ALPHA, *regexpr->re_search_iter); break;
      case 'l': frag_new_val(regexpr, ST_CH_CLASS_LOWER, *regexpr->re_search_iter); break;
      case 'L': frag_new_val(regexpr, ST_CH_CLASS_NOT_LOWER, *regexpr->re_search_iter); break;
      case 'u': frag_new_val(regexpr, ST_CH_CLASS_UPPER, *regexpr->re_search_iter); break;
      case 'U': frag_new_val(regexpr, ST_CH_CLASS_NOT_UPPER, *regexpr->re_search_iter); break;
      case 'w': frag_new_val(regexpr, ST_CH_CLASS_WORD, *regexpr->re_search_iter); break;
      case 'W': frag_new_val(regexpr, ST_CH_CLASS_NOT_WORD, *regexpr->re_search_iter); break;
      case 's': frag_new_val(regexpr, ST_CH_CLASS_SPACE, *regexpr->re_search_iter); break;
      case 'S': frag_new_val(regexpr, ST_CH_CLASS_NOT_SPACE, *regexpr->re_search_iter); break;

      // == Anchoring
      // case 'b': break;    // word boundary  @TODO
      // case 'B': break;    // not a word boundary

      // == Escaping special characters - Quoted characters
      case CH_REP_0_N:   case CH_REP_1_N:   case CH_REP_0_1:  case CH_ALTERN:
      case CH_WILDCARD:  case CH_ESCAPE:    case '^':  case '$':
      case CH_PAREN_L:  case CH_PAREN_R:  case '[':  case '{':
        frag_new_val(regexpr, ST_CHAR, *regexpr->re_search_iter); break;

      // == Otherwise treat the following char as ordinary but raise a warning
      default:
        POST_L("WARNING:  RE Compile:  Invalid escape character discarded in the RE.");
        POST_L("  It should be used to escape an operator or to indicate a character class.");
        if (*regexpr->re_search_iter == '\0') { continue; }    // case of invalid '/' at then end
        frag_new_val(regexpr, ST_CHAR, *regexpr->re_search_iter);
        break;
      }
      break;

    // ==== Any character wildkey
    case CH_WILDCARD: frag_new_val(regexpr, ST_CH_CLASS_ANY, *regexpr->re_search_iter);
      break;

    // ==== Repetition:  acted on immediately as they are already postfix
    case CH_REP_0_N:  case CH_REP_1_N:  case CH_REP_0_1:
      frag_new_repeat(regexpr);
      if (regexpr->err != ERR_NONE) { return; }
      break;

    // ==== Alternation
    case CH_ALTERN:

      // Check for invalid character pairs:  ^|  (|  ||
      switch (regexpr->prev_type) {

      case OP_BEGIN:
        ERR_L(ERR_SYNTAX, , "RE Compile:  Syntax error at 1:  Invalid first char:  %c", CH_ALTERN);

      case OP_PAREN_L:  case OP_ALTERN:
        ERR_L(ERR_SYNTAX, , "RE Compile:  Syntax error at %i:  Invalid sequence:  %c%c",
          regexpr->re_search_iter - regexpr->re_search_s, *(regexpr->re_search_iter - 1), CH_ALTERN);
        }

      // There should be a preceding value (this is likely redundant)
      if (regexpr->is_first == true) {
        ERR_L(ERR_SYNTAX, , "RE Compile:  Syntax error at %i:  No value before %c",
          regexpr->re_search_iter - regexpr->re_search_s + 1, CH_ALTERN);
        }

      // Set the previous type
      regexpr->prev_type = OP_ALTERN;
      regexpr->is_first = true;

      // Unstack any previous concatenation and alternation, and stack a new alternation
      if (*regexpr->oper_iter == OP_CONCAT) { frag_new_concat(regexpr); }
      if (*regexpr->oper_iter == OP_ALTERN) { frag_new_altern(regexpr); }
      STACK_OPER(OP_ALTERN);
      break;

    // ==== Left parenthesis
    case CH_PAREN_L:

      // If there is a value preceding the parenthesis
      // then unstack any previous concatenation, and stack a new concatenation
      if (!regexpr->is_first) {
        if (*regexpr->oper_iter == OP_CONCAT) { frag_new_concat(regexpr); }
        STACK_OPER(OP_CONCAT);
      }

      // Test if there is a capture request for the parenthesis
      if (regexpr->capt_flags & (1 << regexpr->paren_cnt)) {

        regexpr->capt_all_to_used[regexpr->paren_cnt] = regexpr->capt_cnt;
        STACK_OPER(regexpr->capt_cnt++);
      }

      else { STACK_OPER(OP_PAREN_L); }

      // Set the trailing variables and increment the parenthesis counter
      regexpr->is_first = true;
      regexpr->prev_type = OP_PAREN_L;
      regexpr->paren_cnt++;
      break;

    // ==== Right parenthesis
    case CH_PAREN_R:

      // Check for invalid character pairs:  ()  |)  ^)
      switch (regexpr->prev_type) {

      case OP_BEGIN:
        ERR_L(ERR_SYNTAX, , "RE Compile:  Syntax error at 1:  Invalid first char:  %c", CH_PAREN_R);

      case OP_PAREN_L:  case OP_ALTERN:
        ERR_L(ERR_SYNTAX, , "RE Compile:  Syntax error at %i:  Invalid sequence:  %c%c",
          regexpr->re_search_iter - regexpr->re_search_s, *(regexpr->re_search_iter - 1), CH_PAREN_R);
        }

      // There should be a preceding value (this is likely redundant)
      if (regexpr->is_first == true) {
        ERR_L(ERR_SYNTAX, , "RE Compile:  Syntax error at %i:  No value before %c",
          regexpr->re_search_iter - regexpr->re_search_s + 1, *regexpr->re_search_iter);
        }

      // Set the previous type
      regexpr->prev_type = OP_PAREN_R;

      // Unstack any previous concatenation and alternation
      if (*regexpr->oper_iter == OP_CONCAT) {  frag_new_concat(regexpr); }
      if (*regexpr->oper_iter == OP_ALTERN) {  frag_new_altern(regexpr); }

      // The operator stack should now hold a left parenthesis
      if (*regexpr->oper_iter <= 9) { frag_new_parenth(regexpr); }            // capture group parenthesis
      else if (*regexpr->oper_iter == OP_PAREN_L) { regexpr->oper_iter--; }    // non capturing parenthesis

      else { ERR_L(ERR_SYNTAX, , "RE Compile:  Syntax error at %i:  Missing left parenthesis",
          regexpr->re_search_iter - regexpr->re_search_s + 1);
        }
      break;

    // ==== Ordinary character
    default: frag_new_val(regexpr, ST_CHAR, *regexpr->re_search_iter); break;

    }  // End switch over the character types

    regexpr->re_search_iter++;   // iterate through the regular expression

  }  // End while loop over the regular expression's characters

  // Test for invalid last characters
  switch (regexpr->prev_type) {
  case OP_ALTERN:  case OP_BRACKET_L:
    ERR_L(ERR_SYNTAX, , "RE Compile:  Syntax error at %i:  Invalid last character",
      regexpr->re_search_iter - regexpr->re_search_s);
    }

  // Unstack any remaining concatenation and alternation
  if (*regexpr->oper_iter == OP_CONCAT) { frag_new_concat(regexpr); }
  if (*regexpr->oper_iter == OP_ALTERN) {  frag_new_altern(regexpr); }

  // The operator stack should now be empty
  if ((*regexpr->oper_iter >= OP_LAST) && (*regexpr->oper_iter <= OP_LAST + 10)) {
    ERR_L(ERR_SYNTAX, , "RE Compile:  Syntax error:  Missing right parenthesis");
  }
  else if (*regexpr->oper_iter != OP_NULL) {
    ERR_L(ERR_SYNTAX, , "RE Compile:  Syntax error:  Operator stack not empty");
  }

  // The fragment stack should hold one fragment
  if ((regexpr->frag_iter - regexpr->frag_arr) != 1) {
    ERR_L(ERR_SYNTAX, , "RE Compile:  Syntax error:  Fragment stack should hold one fragment");
  }

  // Complete the NFA by setting the first state and the end state
  regexpr->state_first = regexpr->frag_iter->first;
  t_nfa_ind ind = state_new(regexpr, ST_END, IND_NULL, U_VAL('>'));
  frag_connect(regexpr, regexpr->frag_iter, ind);
  regexpr->state_last = ind;

  // Complete the reverse polish notation  @OPTION
  *regexpr->rpn_iter = '\0';

  // Multiply by 2 to account for parentheses pairs
  regexpr->capt_cnt <<= 1;
}

//******************************************************************************
//  Second compilation phase for the replace expression.
//
//  The function assembles a compiled string of substrings, looking for capture groups.
//  Each substring but the last is terminated by '\0' and a capture group index.
//  The function discards invalid escape characters and capture requests for which
//  there are not enough parentheses pairs. It also clears these in the flags.
//
//  Sets:  repl_sub_s, repl_sub_cnt, capt_flags.
//
//  @param regexpr A pointer to the regular expression structure.
//  @param re_replace_s A pointer to the replace expression.
//
void re_compile_replace2(t_regexp2* regexpr, const char* const re_repl_s) {

  // If there are less parentheses pairs than requested capture groups
  // set the extraneous flags to 0
  if ((1 << regexpr->paren_cnt) <= regexpr->capt_flags) {
    POST_L("WARNING:  RE Compile:  Less parentheses than capture groups requested.");
    POST_L("  The extraneous capture requests are ignored.");
    regexpr->capt_flags &= (1 << regexpr->paren_cnt) - 1;
  }

  // Initialize the pointers and the counter
  const char* re_repl_iter = re_repl_s;
  char* repl_sub_iter = regexpr->repl_sub_s;

  // Loop through the replace string
  while (*re_repl_iter) {

    // Process escape characters separately
    if (*re_repl_iter == CH_ESCAPE) {

      // Consider the next character
      switch (*++re_repl_iter) {

      // Escaped escape character
      case CH_ESCAPE: break;

      // Capture command
      case '0':  case '1':  case '2':  case '3': case '4':
      case '5':  case '6':  case '7':  case '8': case '9':

        // If there is a parenthesis pair for the capture group
        if (*re_repl_iter - '0' < regexpr->paren_cnt) {
          regexpr->repl_sub_cnt++;
          *repl_sub_iter++ = '\0';
          *repl_sub_iter++ = regexpr->capt_all_to_used[*re_repl_iter++ - '0'];
          continue;
        }

        // Otherwise discard it
        else { re_repl_iter++; continue; }

      // Invalid escape character at the end
      case '\0':
        POST_L("WARNING:  RE Compile:  Invalid escape char at end of replace expression. Discarded.");
        continue;

      // Invalid escape character not followed by a digit or escape character
      default:
        POST_L("WARNING:  RE Compile:  Invalid escape char in replace expression. Discarded.");
        POST_L("  It should be followed by %c (escaped escape) or a digit (capture group).", CH_ESCAPE);
        break;
      }
    }

    // In most cases, copy and iterate. Exceptions use continue instead of break.
    *repl_sub_iter++ = *re_repl_iter++;
  }

  // Add a terminal character to the string
  *repl_sub_iter = '\0';
}

//******************************************************************************
//  Compile a regular expression into an NFA.
//
//  @param regexpr A pointer to the regular expression structure.
//  @param re_search_s A pointer to the regular expression search expression.
//  @param re_replace_s A pointer to the regular expression replace string.
//
//  Note: ->err set to ERR_ALLOC or ERR_SYNTAX if there is an error.
//
void re_compile(t_regexp2* regexpr, const char* const re_search_s, const char* const re_repl_s) {

  TRACE_L("re_compile");

  // == Search expression

  // Test that the search expression is not NULL
  if (!re_search_s) {  ERR_L(ERR_NULL_PTR, , "RE Compile:  NULL search expression."); }

  // Store the pointer to the search expression
  regexpr->re_search_s = re_search_s;

  // Test its length and resize if necessary
  size_t len = strlen(regexpr->re_search_s);

  // ... shorter than current allocation: just reset
  if (len <= regexpr->length_max) { re_reset(regexpr); }

  // ... longer, but not too much: resize, initialize, and test the allocation
  else if (len < IND_NULL) {
    re_empty(regexpr);
    re_init(regexpr, (t_nfa_ind)len);
    if (regexpr->err != ERR_NONE) { return; }
  }

  // ... too long: abort
  else { ERR_L(ERR_STR_LEN, , "RE Compile:  Search string too long:  max is %i", IND_NULL - 1); }

  // == Replace expression

  // If there is a replace expression
  if (re_repl_s) {

    // Test the length of the search expression
    len = strlen(re_repl_s);
    if (len >= regexpr->repl_sub_max) {

      // Resize if necessary
      regexpr->repl_sub_max = (t_string_ind)MAX((len + 1), (2 * regexpr->repl_sub_max));
      sysmem_freeptr(regexpr->repl_sub_s);
      regexpr->repl_sub_s = (char*)sysmem_newptr(sizeof(char) * regexpr->repl_sub_max);

      // Test the allocation
      if (!regexpr->repl_sub_s) {
        regexpr->repl_sub_max = 0;
        ERR_L(ERR_ALLOC, , "re_compile:  Allocation error.");
      }
    }

    // Compile the replace and the search expressions
    re_compile_replace1(regexpr, re_repl_s);
    re_compile_search(regexpr, re_search_s);
    re_compile_replace2(regexpr, re_repl_s);

    // If there are no capture groups, get the replace string directly
    if (regexpr->repl_sub_cnt == 1) { regexpr->replace_p = regexpr->repl_sub_s; }
    else { regexpr->replace_p = regexpr->replace_s; }
  }

  // If there is no replace expression
  else {
    regexpr->capt_flags = 0;
    regexpr->repl_sub_cnt = 0;
    re_compile_search(regexpr, re_search_s);
    regexpr->replace_p = NULL;
  }
}

//******************************************************************************
//  Try matching a value with a state, with no capture.
//
//  @param regexpr A pointer to the regular expression structure.
//  @param state The state with witch to match the character.
//  @param set_ind The index of the capture set referenced by the routine.
//
void re_simul_state_nc(t_regexp2* regexpr, t_state* state, t_nfa_ind set_ind) {

  // If the state has not been visited this round, and it matches the input
  if ((state->gen_cnt != regexpr->gen_cnt)
    && (match_arr[state->type](*regexpr->match_iter, state->u.value))) {

    // Mark the state as visited using the generation count
    state->gen_cnt = regexpr->gen_cnt;

    switch (state->type) {

    // For branch states recurse through each branch
    case ST_BRANCH:
      re_simul_state_nc(regexpr, regexpr->state_arr + state->u.ind2, 0);
      re_simul_state_nc(regexpr, regexpr->state_arr + state->ind1, 0);
      break;

    case ST_PAREN:
      re_simul_state_nc(regexpr, regexpr->state_arr + state->ind1, 0);
      break;

    case ST_END: break;

    // Otherwise add the state to the new list of matching states
    default:
      (regexpr->rnew_iter++)->state_ind = state->ind1;
      break;
    }
  }
}

//******************************************************************************
//  Try matching a value with a state.
//
//  @param regexpr A pointer to the regular expression structure.
//  @param state The state with witch to match the character.
//  @param set_ind The index of the capture set referenced by the routine.
//
void re_simul_state_wc(t_regexp2* regexpr, t_state* state, t_nfa_ind set_ind) {

  // If the state has not been visited this round, and it matches the input
  if ((state->gen_cnt != regexpr->gen_cnt)
    && (match_arr[state->type](*regexpr->match_iter, state->u.value))) {

    // Mark the state as visited using the generation count
    state->gen_cnt = regexpr->gen_cnt;

    switch (state->type) {

      t_nfa_ind set_ind2;

    // For branch states recurse through each branch
    case ST_BRANCH:
      // Increment the reference count
      (CAPT_CNT(set_ind))++;

      // Recurse to simulate both branches
      re_simul_state_wc(regexpr, regexpr->state_arr + state->u.ind2, set_ind);
      re_simul_state_wc(regexpr, regexpr->state_arr + state->ind1, set_ind);
      break;

    case ST_PAREN:
      set_ind2 = set_ind;    // unchanged when count is one, otherwise changed below

      // If the reference count is more then one we duplicate the set into a new one
      if (CAPT_CNT(set_ind) != 1) {

        // Decrement the reference count
        (CAPT_CNT(set_ind))--;

        // Get the next free set, duplicate, and set its count to one
        t_string_ind* capt_iter = CAPT_IND(set_ind);

        set_ind2 = regexpr->capt_free_ind;
        t_string_ind* capt_iter2 = CAPT_IND(set_ind2);
        regexpr->capt_free_ind = (t_nfa_ind)*capt_iter2;

        t_uint8 cntd = regexpr->capt_cnt;
        while (cntd--) { *capt_iter2++ = *capt_iter++; }

        CAPT_CNT(set_ind2) = 1;
      }

      // Record the parenthesis index
      *(CAPT_IND(set_ind2) + state->u.ind2) = regexpr->match_ind;
      re_simul_state_wc(regexpr, regexpr->state_arr + state->ind1, set_ind2);
      break;

    case ST_END:
      regexpr->capt_end_ind = set_ind;
      break;

    // Otherwise add the state to the new list of matching states
    default:
      regexpr->rnew_iter->state_ind = state->ind1;
      (regexpr->rnew_iter++)->set_ind = set_ind;
      break;
    }
  }

  // Otherwise (no match or state already crossed)
  else {

    // Decrement the reference count
    (CAPT_CNT(set_ind))--;

    // If the count is 0, release the capture set
    if (CAPT_CNT(set_ind) == 0) {
      *CAPT_IND(set_ind) = regexpr->capt_free_ind;
      regexpr->capt_free_ind = set_ind;
    }
  }
}

//******************************************************************************
//  Concatenate the replace string in the simulation phase.
//
//  @param regexpr A pointer to the regular expression structure.
//  @param match_s A pointer to the match string.
//
void re_simul_replace(t_regexp2* regexpr, const char* const match_s) {

  const char* sub_iter = regexpr->repl_sub_s;    // substrings from the replace expression
  t_string_ind* capt_end = CAPT_IND(regexpr->capt_end_ind);
  t_string_ind* capt_ind = NULL;
  const char* capt_iter = NULL;                  // substrings from the capture groups
  char* replace_iter = regexpr->replace_s;      // destination replace string

  // Loop through the substrings and capture groups
  for (t_uint8 cnt = 1; cnt < regexpr->repl_sub_cnt; cnt++) {

    // Copy a substring from the replace string
    while (*sub_iter) { *replace_iter++ = *sub_iter++; }
    sub_iter++;

    // Copy a capture group
    capt_ind = capt_end + 2 * (*sub_iter++);
    capt_iter = match_s + *capt_ind;
    t_string_ind cntd = *(capt_ind + 1) - *capt_ind;
    while (cntd--) { *replace_iter++ = *capt_iter++; }
  }

  // Copy the last substring from the replace string
  while (*sub_iter) { *replace_iter++ = *sub_iter++; }
  *replace_iter = '\0';
}

//******************************************************************************
//  Simulation: Run a string through the NFA to see whether it matches.
//
//  @param regexpr A pointer to the regular expression structure.
//  @param match_s A pointer to the string which is to be matched.
//
t_bool re_simulate(t_regexp2* regexpr, const char* const match_s) {

  // Test if a regular expression has been compiled
  if (!regexpr->state_cnt) {
    ERR_L(ERR_MISC, false , "RE Simulate:  No preceding compilation");
  }

  // Initialize the pointers
  t_simul* rcur_iter = NULL;
  regexpr->match_iter = match_s;
  regexpr->gen_cnt = 255;
  regexpr->match_ind = 0;

  // A state simulation function pointer to choose capture or no capure
  t_simul_state simul_state;

  // Initialize the routine stack to hold just the first state
  regexpr->routine_new->state_ind = regexpr->state_first;
  regexpr->routine_new->set_ind = 0;
  (regexpr->routine_new + 1)->state_ind = IND_NULL;    // terminal value

  // If there is a replace expression with capture groups
  if (regexpr->capt_flags) {

    simul_state = re_simul_state_wc;

    // Set the first capture group to 0 and count to 1
    CAPT_CNT(0) = 1;
    t_nfa_ind cnt = regexpr->capt_cnt;
    t_string_ind* capt_iter = CAPT_IND(0);
    while (cnt--) { *capt_iter++ = 0; }

    // Set the linked list of capture sets up to state_cnt
    regexpr->capt_free_ind = 1;
    capt_iter = CAPT_IND(1);
    for (cnt = 1; cnt < regexpr->state_cnt - 1; cnt++) {
      *capt_iter = cnt + 1;
      capt_iter += regexpr->capt_cnt;
    }
    *capt_iter = IND_NULL;
  }

  // Otherwise: no replace expression or no capture groups
  else { simul_state = re_simul_state_nc; }

  // ====  Loop through the match string ====
  do {

    // Swap the routine stacks and reset their iterating pointers
    // and iterate the generation count
    rcur_iter = regexpr->routine_new;
    regexpr->routine_new = regexpr->routine_cur;
    regexpr->routine_cur = rcur_iter;
    regexpr->rnew_iter = regexpr->routine_new;

    // Iterate the generation count and reset if it has reached 255
    if (regexpr->gen_cnt == 255) {
      regexpr->gen_cnt = 0;
      for (t_nfa_ind ind = 0; ind < regexpr->state_cnt; ind++) {
        (regexpr->state_arr + ind)->gen_cnt = 0;
      }
    }
    regexpr->gen_cnt++;

    // ==  Loop through the list of matching states  ==
    // Using the function pointer previsouly set
    while (rcur_iter->state_ind != IND_NULL) {
      simul_state(regexpr, regexpr->state_arr + rcur_iter->state_ind, rcur_iter->set_ind);
      rcur_iter++;
    }

    // Add a terminal index to the new list of matching states
    regexpr->rnew_iter->state_ind = IND_NULL;

    // Increment the match string index
    regexpr->match_ind++;

  // End the loop through the test string when:
  // the list of matching states is empty, or the end of the string is reached
  } while ((regexpr->rnew_iter != regexpr->routine_new) && *(regexpr->match_iter++));

  // Test the generation count of the last state for overall matching
  if ((regexpr->state_arr + regexpr->state_last)->gen_cnt == regexpr->gen_cnt) {
    if (regexpr->repl_sub_cnt) { re_simul_replace(regexpr, match_s); }
    return true;
  }

  else { return false; }
}

// ====  CHARACTER CLASSES  ====

t_bool st_match_char(char match_c, char ref_c) {

  return (match_c == ref_c);
}

t_bool st_match_end(char match_c, char ref_c) {

  return (match_c == '\0');
}

t_bool st_match_none(char match_c, char ref_c) {

  return false;
}

t_bool st_match_any(char match_c, char ref_c) {

  return true;
}

t_bool st_match_digit(char match_c, char ref_c) {

  return ((match_c >= '0') && (match_c <= '9'));
}

t_bool st_match_not_digit(char match_c, char ref_c) {

  return ((match_c < '0') || (match_c > '9'));
}

t_bool st_match_alpha(char match_c, char ref_c) {

  return (((match_c >= 'a') && (match_c <= 'z')) || ((match_c >= 'A') && (match_c <= 'Z')));
}

t_bool st_match_not_alpha(char match_c, char ref_c) {

  return (((match_c < 'a') || (match_c > 'z')) && ((match_c < 'A') || (match_c > 'Z')));
}

t_bool st_match_lower(char match_c, char ref_c) {

  return ((match_c >= 'a') && (match_c <= 'z'));
}

t_bool st_match_not_lower(char match_c, char ref_c) {

  return ((match_c < 'a') || (match_c > 'z'));
}

t_bool st_match_upper(char match_c, char ref_c) {

  return ((match_c >= 'A') && (match_c <= 'Z'));
}

t_bool st_match_not_upper(char match_c, char ref_c) {

  return ((match_c < 'A') || (match_c > 'Z'));
}

t_bool st_match_word(char match_c, char ref_c) {

  return (((match_c >= 'a') && (match_c <= 'z')) || ((match_c >= 'A') && (match_c <= 'Z'))
    || ((match_c >= '0') && (match_c <= '9')) || (match_c == CH_CONCAT));
}

t_bool st_match_not_word(char match_c, char ref_c) {

  return (((match_c < 'a') || (match_c > 'z')) && ((match_c < 'A') || (match_c > 'Z'))
    && ((match_c < '0') || (match_c > '9')) && (match_c != CH_CONCAT));
}

t_bool st_match_space(char match_c, char ref_c) {

  return ((match_c == ' ') || (match_c == '\t') || (match_c == '\r') || (match_c == '\n') || (match_c == '\f'));
}

t_bool st_match_not_space(char match_c, char ref_c) {

  return ((match_c != ' ') && (match_c != '\t') && (match_c != '\r') && (match_c != '\n') && (match_c != '\f'));
}

// ====  REGEXPR_NEW  ====

t_regexpr* regexpr_new() {

  t_regexpr* expr = (t_regexpr*)sysmem_newptr(sizeof(t_regexpr));

  if (expr) {
    expr->search_frag_s = NULL;
    regexpr_reset(expr);
  }

  return expr;
}

// ====  REGEXPR_RESET  ====

void regexpr_reset(t_regexpr* expr) {

  expr->search_sym = gensym("");
  expr->search_frag_sym = gensym("");
  expr->search_frag_len = 0;
  expr->type_beg = '\0';
  expr->type_end = '\0';
  expr->match_fct = &_regexpr_match_false;

  if (expr->search_frag_s) {
    sysmem_freeptr(expr->search_frag_s); expr->search_frag_s = NULL;
  }
}

// ====  REGEXPR_SET  ====

t_my_err regexpr_set(t_regexpr* expr, t_symbol* search_sym) {

  // Universal wildcard
  if ((search_sym == gensym("*")) || (search_sym == gensym("**"))
      || (search_sym == gensym("*$")) || (search_sym == gensym("$*"))) {
    regexpr_reset(expr);
    expr->search_sym = search_sym;
    expr->match_fct = &_regexpr_match_true;
    return ERR_NONE;
  }

  // The other special cases are invalid
  else if ((search_sym == gensym("")) || (search_sym == gensym("$"))
      || (search_sym == gensym("$$"))) {
    regexpr_reset(expr);
    expr->search_sym = search_sym;
    expr->match_fct = &_regexpr_match_false;
    return ERR_NONE;
  }

  // Other cases
  else {
    expr->search_sym = search_sym;

    t_int32 expr_len = (t_int32)strlen(search_sym->s_name);
    t_int32 offset_beg, offset_end;

    switch (search_sym->s_name[0]) {
    case CH_REP_0_N:
    case '$': expr->type_beg = search_sym->s_name[0]; offset_beg = 1; break;
    default: expr->type_beg = 'X'; offset_beg = 0; break;
  }

    switch (search_sym->s_name[expr_len - 1]) {
    case CH_REP_0_N:
    case '$': expr->type_end = search_sym->s_name[expr_len - 1]; offset_end = 1; break;
    default: expr->type_end = 'X'; offset_end = 0; break;
  }

    expr->search_frag_len = expr_len - offset_beg - offset_end;

    if (expr->search_frag_s) { sysmem_freeptr(expr->search_frag_s); }
    expr->search_frag_s = (char*)sysmem_newptr(sizeof(char) * (expr->search_frag_len + 1));
    if (!expr->search_frag_s) { regexpr_reset(expr); return ERR_ALLOC; }

    strncpy_zero(expr->search_frag_s, expr->search_sym->s_name + offset_beg, expr->search_frag_len + 1);
    expr->search_frag_s[expr->search_frag_len] = '\0';

    expr->search_frag_sym = gensym(expr->search_frag_s);

    if (offset_beg + offset_end == 0) { expr->match_fct = &_regexpr_match_reg; }
    else if ((offset_beg == 0) && (offset_end == 1)) { expr->match_fct = &_regexpr_match_beg; }
    else if ((offset_beg == 1) && (offset_end == 0)) { expr->match_fct = &_regexpr_match_end; }
    else if ((offset_beg == 1) && (offset_end == 1)) { expr->match_fct = &_regexpr_match_mid; }

    return ERR_NONE;
  }
}

// ====  REGEXPR_FREE  ====
void regexpr_free(t_regexpr* expr) {

  if (expr->search_frag_s) { sysmem_freeptr(expr->search_frag_s); }
  regexpr_reset(expr);
}

// ====  REGEXPR_MATCH  ====

t_bool regexpr_match(t_regexpr* expr, t_symbol* match_sym) {

  return (expr->match_fct(expr, match_sym));
}

// ========  MATCHING FUNCTIONS  ========

// ====  _REGEXPR_MATCH_TRUE  ====

t_bool _regexpr_match_true(t_regexpr* expr, t_symbol* match_sym) {

  return true;
}

// ====  _REGEXPR_MATCH_FALSE  ====

t_bool _regexpr_match_false(t_regexpr* expr, t_symbol* match_sym) {

  return false;
}

// ====  _REGEXPR_MATCH_REG  ====

t_bool _regexpr_match_reg(t_regexpr* expr, t_symbol* match_sym) {

  return (expr->search_frag_sym == match_sym);
}

// ====  _REGEXPR_MATCH_BEG  ====

t_bool _regexpr_match_beg(t_regexpr* expr, t_symbol* match_sym) {

  t_bool test = _regexpr_match_in_forward(expr->search_frag_s, match_sym->s_name);

  if ((!test) || ((expr->type_end == '$') && (match_sym->s_name[expr->search_frag_len] != ' ') && (match_sym->s_name[expr->search_frag_len] != '\0'))) { return false; }
  else { return true; }
}

// ====  _REGEXPR_MATCH_END  ====

t_bool _regexpr_match_end(t_regexpr* expr, t_symbol* match_sym) {

  t_int32 match_len = (t_int32)strlen(match_sym->s_name);
  t_bool test = _regexpr_match_in_backward(expr->search_frag_s, match_sym->s_name, expr->search_frag_len, match_len);

  if ((!test) || ((expr->type_beg == '$') && (match_len != expr->search_frag_len) && (match_sym->s_name[match_len - expr->search_frag_len - 1] != ' '))) { return false; }
  else { return true; }
}

// ====  _REGEXPR_MATCH_MID  ====

t_bool _regexpr_match_mid(t_regexpr* expr, t_symbol* match_sym) {

  char* find = match_sym->s_name;

  while (find != NULL) {

    find = strstr(find, expr->search_frag_s);

    if (find) {

        if (((expr->type_beg != '$') || (find == match_sym->s_name) || (*(find - 1) == ' '))
            && ((expr->type_end != '$') || (*(find + expr->search_frag_len) == ' ') || (*(find + expr->search_frag_len) == '\0'))) {
          return true;
        }

        find += expr->search_frag_len;
      }
    }

  return false;
}

// ========  UTILITY FUNCTIONS  ========

// ====  _REGEXPR_MATCH_IN_FORWARD  ====

t_bool _regexpr_match_in_forward(char* search_frag_s, char* match_s) {

  char* pch1 = search_frag_s;
  char* pch2 = match_s;

  while ((*pch1 == *pch2) && (*pch1 != '\0')) { pch1++; pch2++; }

  return (*pch1 == '\0');
}

// ====  _REGEXPR_MATCH_IN_BACKWARD  ====

t_bool _regexpr_match_in_backward(char* search_frag_s, char* match_s, t_int32 search_frag_len, t_int32 match_len) {

  if (search_frag_len > match_len) { return false; }

  char* pch1 = search_frag_s + search_frag_len;
  char* pch2 = match_s + match_len;

  while ((*pch1 == *pch2) && (pch1 != search_frag_s)) { pch1--; pch2--; }

  return (*pch1 == *pch2);
}
