/**
  @file
  dict_recurse - a dict_recurse object
  Yves Candau - ycandau@gmail.com

  @ingroup  myExternals  
*/

// TO DO:
//   Check string length
//   Simulate, all, clean empty, depth
//   Remove dynamic allocation for regexpr
//   Systematize replace entry vs from replace_dict

// ========  HEADER FILES  ========

#include "ext.h"       // Header file for all objects, should always be first
#include "ext_obex.h"  // Header file for all objects, required for new style Max object
#include "z_dsp.h"     // Header file for MSP objects, included here for t_double type

#include "ext_dictionary.h"
#include "ext_dictobj.h"

#include "regexpr.h"

// ========  MACROS  ========

#define _TRACE false
#define TRACE(...) do { if (_TRACE) object_post((t_object *)x, "TRACE:  " __VA_ARGS__); } while (0)

#define _POST true
#define POST(...) do { if (_POST) object_post((t_object *)x, __VA_ARGS__); } while (0)

#define WARNING(...) do { object_warn((t_object *)x, __VA_ARGS__); } while (0)

#define MY_ERR(...)  do { object_error((t_object *)x, __VA_ARGS__); } while (0)

#define MY_ASSERT(test, err, ...) if (test) { object_error((t_object *)x, __VA_ARGS__); return err; }
#define MY_ASSERT_GOTO(test, label, ...) if (test) { object_error((t_object *)x, __VA_ARGS__); goto label; }

// ========  DEFINES  ========

#define MAX_LEN_PATH   1000  // Maximum message size
#define MAX_LEN_NUMBER 50    // Maximum string length for numbers

// ========  TYPEDEF AND CONST GLOBAL VARIABLES  ========

/**
The different commands
*/
typedef enum _command
{
  CMD_NONE,
  CMD_ALL,
  CMD_FIND_KEY_IN,
  CMD_FIND_KEY,
  CMD_FIND_VALUE_SYM,
  CMD_FIND_ENTRY,
  CMD_FIND_DICT_CONT_ENTRY,
  CMD_REPLACE_KEY,
  CMD_REPLACE_VALUE_SYM,
  CMD_REPLACE_ENTRY,
  CMD_REPLACE_DICT_CONT_ENTRY,
  CMD_REPLACE_VALUE_FROM_DICT,
  CMD_APPEND_IN_DICT_CONT_ENTRY,
  CMD_APPEND_IN_DICT_CONT_ENTRY_D,
  CMD_APPEND_IN_DICT_FROM_KEY,
  CMD_DELETE_KEY,
  CMD_DELETE_VALUE_SYM,
  CMD_DELETE_ENTRY,
  CMD_DELETE_DICT_CONT_ENTRY

} t_command;

/**
The different types of values
*/
typedef enum _value_type
{
  VALUE_TYPE_NONE,
  VALUE_TYPE_INT,
  VALUE_TYPE_FLOAT,
  VALUE_TYPE_SYM,
  VALUE_TYPE_DICT,
  VALUE_TYPE_ARRAY,
  VALUE_TYPE_TRUE,
  VALUE_TYPE_FALSE,
  VALUE_TYPE_NULL

} t_value_type;

/**
Return value used by _dict_recurse_value() to indicate
when a value was deleted in an array
*/
typedef enum _value_del
{
  VALUE_NO_DEL,
  VALUE_DEL

} t_value_del;

// ========  STRUCTURE DECLARATION  ========

typedef struct _dict_recurse 
{
  t_object ob;

  void *outl_dict;    // Outlet 0: dictionaries
  void *outl_mess;    // Outlet 1: messages
  void *outl_bang;    // Outlet 2: bang on completion

  t_symbol     *dict_sym;
  t_dictionary *dict;

  t_value_type  type_iter;
  t_dictionary *dict_iter;
  t_symbol     *key_iter;
  t_atomarray  *array_iter;
  t_int32       index_iter;

  char *path;
  char  str_tmp[MAX_LEN_NUMBER];

  t_int32 path_len_max;
  t_int32 count;

  t_command command;

  t_regexpr *search_key_expr;
  t_regexpr *search_val_expr;

  t_symbol *replace_key_sym;
  t_symbol *replace_val_sym;

  t_symbol     *replace_dict_sym;
  t_dictionary *replace_dict;

  t_bool is_busy;
  t_bool has_match;

  char a_verbose;

  t_regexp2 *re2;

} t_dict_recurse;

// ========  FUNCTION PROTOTYPES  ========

void *dict_recurse_new    (t_symbol *sym, long argc, t_atom *argv);
void  dict_recurse_free   (t_dict_recurse *x);
void  dict_recurse_assist (t_dict_recurse *x, void *b, long msg, long arg, char *str);

void  dict_recurse_all     (t_dict_recurse *x, t_symbol *dict_sym);
void  dict_recurse_find    (t_dict_recurse *x, t_symbol *sym, long argc, t_atom *argv);
void  dict_recurse_replace (t_dict_recurse *x, t_symbol *sym, long argc, t_atom *argv);
void  dict_recurse_append  (t_dict_recurse *x, t_symbol *sym, long argc, t_atom *argv);
void  dict_recurse_delete  (t_dict_recurse *x, t_symbol *sym, long argc, t_atom *argv);

void     _dict_recurse_reset     (t_dict_recurse *x);
t_my_err _dict_recurse_begin_cmd (t_dict_recurse *x, t_atom *dict_ato, t_symbol *cmd_sym);
void     _dict_recurse_end_cmd   (t_dict_recurse *x);

void    _dict_recurse_dict  (t_dict_recurse *x, t_dictionary *dict, t_int32 depth);
t_int32 _dict_recurse_value (t_dict_recurse *x, t_atom *value, t_int32 depth);
void    _dict_recurse_array (t_dict_recurse *x, t_atomarray *atom_arr, t_int32 depth);

void  dict_recurse_bang     (t_dict_recurse *x);
void  dict_recurse_set      (t_dict_recurse *x, t_symbol *sym, long argc, t_atom *argv);
void  dict_recurse_get      (t_dict_recurse *x, t_symbol *sym, long argc, t_atom *argv);
void  dict_recurse_anything (t_dict_recurse *x, t_symbol *sym, long argc, t_atom *argv);

void  dict_compile_re  (t_dict_recurse *x, t_symbol *sym, long argc, t_atom *argv);
void  dict_simulate_re (t_dict_recurse *x, t_symbol *sym, long argc, t_atom *argv);
void  dict_post_state  (t_dict_recurse *x);

// ========  GLOBAL CLASS POINTER AND STATIC VARIABLES  ========

void *dict_recurse_class;

void dict_re_compile(t_dict_recurse *x, t_symbol *sym, long argc, t_atom *argv)
{
  TRACE("dict_re_compile");

  if (argc == 1) { re_compile(x->re2, atom_getsym(argv)->s_name, NULL); }
  else if (argc == 2) { re_compile(x->re2, atom_getsym(argv)->s_name, atom_getsym(argv + 1)->s_name); }

  MY_ASSERT(x->re2->err != ERR_NONE, , "Compilation error.");

  POST("Compile: %s %s - RPN: %s - States: %i - Flags: %i - Substr: %i %s",
    x->re2->re_search_s, atom_getsym(argv + 1)->s_name, x->re2->rpn_s, x->re2->state_cnt,
    x->re2->capt_flags, x->re2->repl_sub_cnt, x->re2->repl_sub_s);
}

void dict_re_simulate(t_dict_recurse *x, t_symbol *sym, long argc, t_atom *argv)
{
  TRACE("dict_re_simulate");

  t_bool test = re_simulate(x->re2, atom_getsym(argv)->s_name);
  MY_ASSERT(x->re2->err != ERR_NONE, , "Simulation error.");

  POST("Search: %s - Match: %s%s%s - %s", x->re2->re_search_s, atom_getsym(argv)->s_name, 
    (x->re2->repl_sub_cnt) ? " - Replace: " : "", (x->re2->repl_sub_cnt) ? x->re2->replace_p : "",
    test ? "MATCH" : "NO MATCH");
}

void dict_re_states(t_dict_recurse *x)
{
  state_post(x->re2);
}

// ========  INITIALIZATION ROUTINE  ========

void C74_EXPORT ext_main(void *r)
{
  t_class *c;
  
  c = class_new("dict.recurse", (method)dict_recurse_new, (method)dict_recurse_free, 
      (long)sizeof(t_dict_recurse), (method)NULL, A_GIMME, 0);
  
  // Methods
  class_addmethod(c, (method)dict_recurse_assist, "assist", A_CANT, 0);  

  class_addmethod(c, (method)dict_recurse_all, "all", A_SYM, 0);
  class_addmethod(c, (method)dict_recurse_find, "find", A_GIMME, 0);
  class_addmethod(c, (method)dict_recurse_replace, "replace", A_GIMME, 0);
  class_addmethod(c, (method)dict_recurse_append, "append", A_GIMME, 0);
  class_addmethod(c, (method)dict_recurse_delete, "delete", A_GIMME, 0);

  class_addmethod(c, (method)dict_recurse_bang, "bang", 0);
  class_addmethod(c, (method)dict_recurse_set, "set", A_GIMME, 0);
  class_addmethod(c, (method)dict_recurse_get, "get", A_GIMME, 0);
  class_addmethod(c, (method)dict_recurse_anything, "anything", A_GIMME, 0);  

  class_addmethod(c, (method)dict_re_compile, "compile", A_GIMME, 0);
  class_addmethod(c, (method)dict_re_simulate, "simulate", A_GIMME, 0);
  class_addmethod(c, (method)dict_re_states, "states", 0);

  // Attributes
  CLASS_ATTR_CHAR(c, "verbose", 0, t_dict_recurse, a_verbose);
  CLASS_ATTR_STYLE(c, "verbose", 0, "onoff");
  CLASS_ATTR_SAVE(c, "verbose", 0);

  class_register(CLASS_BOX, c);
  dict_recurse_class = c;
}

// ========  NEW INSTANCE ROUTINE: DICT_RECURSE_NEW  ========

void *dict_recurse_new(t_symbol *sym, long argc, t_atom *argv)
{
  t_dict_recurse *x = NULL;

  x = (t_dict_recurse *)object_alloc(dict_recurse_class);

  if (x == NULL) {
    MY_ERR("Object allocation failed.");
    return NULL; }

  x->outl_bang = bangout(x);                      // Outler 1: Bang on completion
  x->outl_mess = outlet_new((t_object*)x, NULL);  // Outlet 0: General messages

  x->path_len_max = MAX_LEN_PATH;
  
  x->path = NULL;
  x->path = (char *)sysmem_newptr(sizeof(char) * x->path_len_max);
  if (!x->path) { MY_ERR("new:  Allocation error for \"path\"."); }

  x->search_key_expr = regexpr_new();
  x->search_val_expr = regexpr_new();
  if (!x->search_key_expr || !x->search_val_expr) {
    MY_ERR("new:  Allocation error for the search expressions."); }

  _dict_recurse_reset(x);

  re_set_object(x);
  
  x->re2 = re_new(254);
  if (!x->re2) { return NULL; }

  return(x);
}

// ========  DICT_RECURSE_FREE  ========

void dict_recurse_free(t_dict_recurse *x)
{
  TRACE("dict_recurse_free");

  if (x->path) { sysmem_freeptr(x->path); }

  regexpr_free(x->search_key_expr);
  regexpr_free(x->search_val_expr);

  re_free(&x->re2);
}

// ====  DICT_RECURSE_ASSIST  ====

void dict_recurse_assist(t_dict_recurse *x, void *b, long msg, long arg, char *str)
{
  if (msg == ASSIST_INLET) {
    switch (arg) {
    case 0: sprintf(str, "Inlet 0: All purpose (list)"); break;
    default: break; } }
  
  else if (msg == ASSIST_OUTLET) {
    switch (arg) {
    case 0: sprintf(str, "Outlet 0: All purpose messages"); break;
    case 1: sprintf(str, "Outlet 1: Bang on command completion"); break;
    default: break; } }
}

// ====  _DICT_RECURSE_RESET  ====

void _dict_recurse_reset(t_dict_recurse *x)
{
  TRACE("_dict_recurse_reset");

  // Reset the object variables
  x->dict = NULL;
  x->dict_sym = gensym("");
  x->type_iter = VALUE_TYPE_NONE;
  x->dict_iter = NULL;
  x->key_iter = gensym("");
  x->array_iter = NULL;
  x->index_iter = -1;
  x->path[0] = '\0';
  x->command = CMD_NONE;
  x->replace_key_sym = gensym("");
  x->replace_val_sym = gensym("");
  x->replace_dict = NULL;
  x->replace_dict_sym = gensym("");
  x->count = 0;
  x->has_match = false;
  x->is_busy = false;
}

// ====  _DICT_RECURSE_BEGIN_CMD  ====

t_my_err _dict_recurse_begin_cmd(t_dict_recurse *x, t_atom *dict_ato, t_symbol *cmd_sym)
{
  TRACE("_dict_recurse_begin_cmd");

  // Test if the object is already busy 
  MY_ASSERT(x->is_busy, ERR_LOCKED, "%s:  The object is still busy.", cmd_sym->s_name);

  // Arg 0: The name of the dictionary to process
  x->dict_sym = atom_getsym(dict_ato);
  x->dict = dictobj_findregistered_retain(x->dict_sym);
  MY_ASSERT(!x->dict, ERR_DICT_NONE, "%s:  Arg 1:  Unable to reference the dictionary named \"%s\".",
    cmd_sym->s_name, x->dict_sym->s_name);

  // Copy the name of the root dictionary into the path
  strncpy_zero(x->path, x->dict_sym->s_name, x->path_len_max);

  // Set the trailing variables
  x->type_iter = VALUE_TYPE_DICT;
  x->dict_iter = x->dict;

  // Set the object to busy status
  x->is_busy = true;

  return ERR_NONE;
}

// ====  _DICT_RECURSE_END_CMD  ====

void _dict_recurse_end_cmd(t_dict_recurse *x)
{
  TRACE("_dict_recurse_end_cmd");

  // Release the dictionary or dictionaries
  if (x->dict) { dictobj_release(x->dict); }
  if (x->replace_dict) { dictobj_release(x->replace_dict); }

  // Reset the object variables
  _dict_recurse_reset(x);

  // Indicate the completion of the cmd with a bang
  outlet_bang(x->outl_bang);
}

// ====  DICT_RECURSE_ALL  ====
/**
all (sym: dictionary)
*/
void dict_recurse_all(t_dict_recurse *x, t_symbol *dict_sym)
{
  TRACE("dict_recurse_all");

  // Initialize the object variables
  x->command = CMD_ALL;

  t_atom dict_ato[1];
  atom_setsym(dict_ato, dict_sym);
  if (_dict_recurse_begin_cmd(x, dict_ato, gensym("all")) != ERR_NONE) { return; }

  // Start the recursion
  _dict_recurse_dict(x, x->dict, 0);

  // Post a summary for the command
  POST("all:  %i reference%s found in \"%s\".", x->count, (x->count == 1) ? "" : "s", x->dict_sym->s_name);

  // End the command
  _dict_recurse_end_cmd(x);
}

// ====  DICT_RECURSE_FIND  ====

/**
find key (sym: dictionary) (sym: search key)
find key_in (sym: dictionary) (sym: search key)
find value (sym: dictionary) (sym: search value)
find entry (sym: dictionary) (sym: search key) (sym: search value)
find dict_cont_entry (sym: dictionary) (sym: search key) (sym: search value)
*/
void dict_recurse_find(t_dict_recurse *x, t_symbol *sym, long argc, t_atom *argv)
{
  TRACE("dict_recurse_find");

  t_symbol *search_key_sym = gensym("");
  t_symbol *search_val_sym = gensym("");

  // Arg 0: Find a key, all entries with the key in, a value, or a dictionary
  t_symbol *cmd_arg = atom_getsym(argv);
  t_symbol *cmd_sym = gensym("");
  if (cmd_arg == gensym("key")) { x->command = CMD_FIND_KEY; cmd_sym = gensym("find key"); }
  else if (cmd_arg == gensym("key_in")) { x->command = CMD_FIND_KEY_IN; cmd_sym = gensym("find key_in"); }
  else if (cmd_arg == gensym("value")) { x->command = CMD_FIND_VALUE_SYM; cmd_sym = gensym("find value"); }
  else if (cmd_arg == gensym("entry")) { x->command = CMD_FIND_ENTRY; cmd_sym = gensym("find entry"); }
  else if (cmd_arg == gensym("dict_cont_entry")) { x->command = CMD_FIND_DICT_CONT_ENTRY; cmd_sym = gensym("find dict_cont_entry"); }
  else { MY_ASSERT(1, , "find:  Arg 0:  Invalid argument."); }

  switch (x->command) {

  // find key (sym: dictionary) (sym: search key)
  // find key_in (sym: dictionary) (sym: search key)
  case CMD_FIND_KEY:
  case CMD_FIND_KEY_IN:
    search_key_sym = atom_getsym(argv + 2);
    MY_ASSERT(search_key_sym == gensym(""), , "%s:  Arg 2:  Invalid argument.", cmd_sym->s_name);
    regexpr_set(x->search_key_expr, search_key_sym);
    break;

  // find value (sym: dictionary) (sym: search value)
  case CMD_FIND_VALUE_SYM:
    search_val_sym = atom_getsym(argv + 2);
    MY_ASSERT(search_val_sym == gensym(""), , "%s:  Arg 2:  Invalid argument.", cmd_sym->s_name);
    regexpr_set(x->search_val_expr, search_val_sym);
    break;

  // find entry (sym: dictionary) (sym: search key) (sym: search value)
  // find dict_cont_entry (sym: dictionary) (sym: search key) (sym: search value)
  case CMD_FIND_ENTRY:
  case CMD_FIND_DICT_CONT_ENTRY:
    search_key_sym = atom_getsym(argv + 2);
    search_val_sym = atom_getsym(argv + 3);
    MY_ASSERT(search_key_sym == gensym(""), , "%s:  Arg 2:  Invalid argument.", cmd_sym->s_name);
    MY_ASSERT(search_val_sym == gensym(""), , "%s:  Arg 3:  Invalid argument.", cmd_sym->s_name);
    regexpr_set(x->search_key_expr, search_key_sym);
    regexpr_set(x->search_val_expr, search_val_sym);
    break;
  
    default: break; }

  // Initialize the object variables
  if (_dict_recurse_begin_cmd(x, argv + 1, cmd_sym) != ERR_NONE) { return; }

  // Start the recursion
  _dict_recurse_dict(x, x->dict, 0);

  // Post a summary for the command
  POST("%s:  %i reference%s found in \"%s\".", cmd_sym->s_name, x->count, (x->count == 1) ? "" : "s", x->dict_sym->s_name);

  // End the command
  _dict_recurse_end_cmd(x);
}

// ====  DICT_RECURSE_REPLACE  ====

/**
replace key (sym: dictionary) (sym: search key) (sym: replace key)
replace value (sym: dictionary) (sym: search value) (sym: replace value)
replace entry (sym: dictionary) (sym: search key) (sym: search value) (sym: replace key) (sym: replace value)
replace dict_cont_entry (sym: dictionary) (sym: search key) (sym: search value) (sym: replace dict)
replace value_from_dict (sym: dictionary) (sym: search key) (sym: replace dict)
*/
void dict_recurse_replace(t_dict_recurse *x, t_symbol *sym, long argc, t_atom *argv)
{
  TRACE("dict_recurse_replace");
  
  t_symbol *search_key_sym = gensym("");
  t_symbol *search_val_sym = gensym("");

  // Arg 1: Find a key, all entries with the key in, a value, or a dictionary
  t_symbol *cmd_arg = atom_getsym(argv);
  t_symbol *cmd_sym = gensym("");
  if (cmd_arg == gensym("key")) { x->command = CMD_REPLACE_KEY; cmd_sym = gensym("replace key"); }
  else if (cmd_arg == gensym("value")) { x->command = CMD_REPLACE_VALUE_SYM; cmd_sym = gensym("replace value"); }
  else if (cmd_arg == gensym("entry")) { x->command = CMD_REPLACE_ENTRY; cmd_sym = gensym("replace entry"); }
  else if (cmd_arg == gensym("dict_cont_entry")) { x->command = CMD_REPLACE_DICT_CONT_ENTRY; cmd_sym = gensym("replace dict_cont_entry"); }
  else if (cmd_arg == gensym("value_from_dict")) { x->command = CMD_REPLACE_VALUE_FROM_DICT; cmd_sym = gensym("replace value_from_dict"); }
  else { MY_ASSERT(1, , "replace:  Arg 0:  Invalid argument."); }

  switch (x->command) {

  // replace key (sym: dictionary) (sym: search key) (sym: replace key)
  case CMD_REPLACE_KEY:
    search_key_sym = atom_getsym(argv + 2);
    x->replace_key_sym = atom_getsym(argv + 3);
    MY_ASSERT(search_key_sym == gensym(""), , "%s:  Arg 2:  Invalid argument.", cmd_sym->s_name);
    MY_ASSERT(x->replace_key_sym == gensym(""), , "%s:  Arg 3:  Invalid argument.", cmd_sym->s_name);
    regexpr_set(x->search_key_expr, search_key_sym);
    break;

  // replace value (sym: dictionary) (sym: search value) (sym: replace value)
  case CMD_REPLACE_VALUE_SYM:
    search_val_sym = atom_getsym(argv + 2);
    x->replace_val_sym = atom_getsym(argv + 3);
    MY_ASSERT(search_val_sym == gensym(""), , "%s:  Arg 2:  Invalid argument.", cmd_sym->s_name);
    MY_ASSERT(x->replace_val_sym == gensym(""), , "%s:  Arg 3:  Invalid argument.", cmd_sym->s_name);
    regexpr_set(x->search_val_expr, search_val_sym);
    break;

  // replace dict_cont_entry (sym: dictionary) (sym: search key) (sym: search value) (sym: replace dict)
  case CMD_REPLACE_DICT_CONT_ENTRY:
    search_key_sym = atom_getsym(argv + 2);
    search_val_sym = atom_getsym(argv + 3);
    x->replace_dict_sym = atom_getsym(argv + 4);
    MY_ASSERT(search_key_sym == gensym(""), , "%s:  Arg 2:  Invalid argument.", cmd_sym->s_name);
    MY_ASSERT(search_val_sym == gensym(""), , "%s:  Arg 3:  Invalid argument.", cmd_sym->s_name);
    MY_ASSERT(x->replace_dict_sym == gensym(""), , "%s:  Arg 4:  Invalid argument.", cmd_sym->s_name);
    regexpr_set(x->search_key_expr, search_key_sym);
    regexpr_set(x->search_val_expr, search_val_sym);

    x->replace_dict = dictobj_findregistered_retain(x->replace_dict_sym);
    MY_ASSERT(!x->replace_dict, , "%s:  Arg 4:  Unable to reference the dictionary named \"%s\".",
      cmd_sym->s_name, x->replace_dict_sym->s_name);
    break;

  // replace value_from_dict (sym: dictionary) (sym: search key) (sym: replace dict)
  case CMD_REPLACE_VALUE_FROM_DICT:
    search_key_sym = atom_getsym(argv + 2);
    x->replace_dict_sym = atom_getsym(argv + 3);
    MY_ASSERT(search_key_sym == gensym(""), , "%s:  Arg 2:  Invalid argument.", cmd_sym->s_name);
    MY_ASSERT(x->replace_dict_sym == gensym(""), , "%s:  Arg 3:  Invalid argument.", cmd_sym->s_name);
    regexpr_set(x->search_key_expr, search_key_sym);

    x->replace_dict = dictobj_findregistered_retain(x->replace_dict_sym);
    MY_ASSERT(!x->replace_dict, , "%s:  Arg 3:  Unable to reference the dictionary named \"%s\".",
      cmd_sym->s_name, x->replace_dict_sym->s_name);
    break;
  
  // replace entry (sym: dictionary) (sym: search key) (sym: search value) (sym: replace key) (sym: replace value)
  case CMD_REPLACE_ENTRY:
    search_key_sym = atom_getsym(argv + 2);
    search_val_sym = atom_getsym(argv + 3);
    x->replace_key_sym = atom_getsym(argv + 4);
    x->replace_val_sym = atom_getsym(argv + 5);
    MY_ASSERT(search_key_sym == gensym(""), , "%s:  Arg 2:  Invalid argument.", cmd_sym->s_name);
    MY_ASSERT(search_val_sym == gensym(""), , "%s:  Arg 3:  Invalid argument.", cmd_sym->s_name);
    MY_ASSERT(x->replace_key_sym == gensym(""), , "%s:  Arg 4:  Invalid argument.", cmd_sym->s_name);
    MY_ASSERT(x->replace_val_sym == gensym(""), , "%s:  Arg 5:  Invalid argument.", cmd_sym->s_name);
    regexpr_set(x->search_key_expr, search_key_sym);
    regexpr_set(x->search_val_expr, search_val_sym);
    break;
  
  default: break; }

  // Initialize the object variables
  if (_dict_recurse_begin_cmd(x, argv + 1, cmd_sym) != ERR_NONE) { return; }

  // Start the recursion
  _dict_recurse_dict(x, x->dict, 0);

  // Notify that the dictionary has been modified
  if (x->count > 0) {
    object_notify(x->dict, gensym("modified"), NULL); }

  // Post a summary for the command
  POST("%s:  %i replacement%s made in \"%s\".", cmd_sym->s_name, x->count, (x->count == 1) ? "" : "s", x->dict_sym->s_name);

  // End the command
  _dict_recurse_end_cmd(x);
}

// ====  DICT_RECURSE_APPEND  ====

/**
append in_dict_cont_entry (sym: dictionary) (sym: search key) (sym: search value) (sym: replace key) (sym: replace value)
append in_dict_cont_entry_d (sym: dictionary) (sym: search key) (sym: search value) (sym: replace key) (sym: replace dict)
append in_dict_from_key (sym: dictionary) (sym: search key) (sym: replace key) (sym: replace dict)
*/
void dict_recurse_append(t_dict_recurse *x, t_symbol *sym, long argc, t_atom *argv)
{
  TRACE("dict_recurse_append");
  
  t_symbol *search_key_sym = gensym("");
  t_symbol *search_val_sym = gensym("");

  // Arg 1: Find a key, all entries with the key in, a value, or a dictionary
  t_symbol *cmd_arg = atom_getsym(argv);
  t_symbol *cmd_sym = gensym("");
  if (cmd_arg == gensym("in_dict_cont_entry")) { x->command = CMD_APPEND_IN_DICT_CONT_ENTRY; cmd_sym = gensym("append in_dict_cont_entry"); }
  else if (cmd_arg == gensym("in_dict_cont_entry_d")) { x->command = CMD_APPEND_IN_DICT_CONT_ENTRY_D; cmd_sym = gensym("append in_dict_cont_entry_d"); }
  else if (cmd_arg == gensym("in_dict_from_key")) { x->command = CMD_APPEND_IN_DICT_FROM_KEY; cmd_sym = gensym("append in_dict_from_key"); }
  else { MY_ASSERT(1, , "replace:  Arg 0:  Invalid argument."); }

  switch (x->command) {

  // append in_dict_cont_entry (sym: dictionary) (sym: search key) (sym: search value) (sym: replace key) (sym: replace value)
  case CMD_APPEND_IN_DICT_CONT_ENTRY:
    search_key_sym = atom_getsym(argv + 2);
    search_val_sym = atom_getsym(argv + 3);
    x->replace_key_sym = atom_getsym(argv + 4);
    x->replace_val_sym = atom_getsym(argv + 5);
    MY_ASSERT(search_key_sym == gensym(""), , "%s:  Arg 2:  Invalid argument.", cmd_sym->s_name);
    MY_ASSERT(search_val_sym == gensym(""), , "%s:  Arg 3:  Invalid argument.", cmd_sym->s_name);
    MY_ASSERT(x->replace_key_sym == gensym(""), , "%s:  Arg 4:  Invalid argument.", cmd_sym->s_name);
    MY_ASSERT(x->replace_val_sym == gensym(""), , "%s:  Arg 5:  Invalid argument.", cmd_sym->s_name);
    regexpr_set(x->search_key_expr, search_key_sym);
    regexpr_set(x->search_val_expr, search_val_sym);
    break;
  
  // append in_dict_cont_entry_d (sym: dictionary) (sym: search key) (sym: search value) (sym: replace key) (sym: replace dict)
  case CMD_APPEND_IN_DICT_CONT_ENTRY_D:
    search_key_sym = atom_getsym(argv + 2);
    search_val_sym = atom_getsym(argv + 3);
    x->replace_key_sym = atom_getsym(argv + 4);
    x->replace_dict_sym = atom_getsym(argv + 5);
    MY_ASSERT(search_key_sym == gensym(""), , "%s:  Arg 2:  Invalid argument.", cmd_sym->s_name);
    MY_ASSERT(search_val_sym == gensym(""), , "%s:  Arg 3:  Invalid argument.", cmd_sym->s_name);
    MY_ASSERT(x->replace_key_sym == gensym(""), , "%s:  Arg 4:  Invalid argument.", cmd_sym->s_name);
    MY_ASSERT(x->replace_dict_sym == gensym(""), , "%s:  Arg 5:  Invalid argument.", cmd_sym->s_name);
    regexpr_set(x->search_key_expr, search_key_sym);
    regexpr_set(x->search_val_expr, search_val_sym);

    x->replace_dict = dictobj_findregistered_retain(x->replace_dict_sym);
    MY_ASSERT(!x->replace_dict, , "%s:  Arg 5:  Unable to reference the dictionary named \"%s\".",
      cmd_sym->s_name, x->replace_dict_sym->s_name);
    break;

  // append in_dict_from_key (sym: dictionary) (sym: search key) (sym: replace key) (sym: replace dict)
  case CMD_APPEND_IN_DICT_FROM_KEY:
    search_key_sym = atom_getsym(argv + 2);
    x->replace_key_sym = atom_getsym(argv + 3);
    x->replace_dict_sym = atom_getsym(argv + 4);
    MY_ASSERT(search_key_sym == gensym(""), , "%s:  Arg 2:  Invalid argument.", cmd_sym->s_name);
    MY_ASSERT(x->replace_key_sym == gensym(""), , "%s:  Arg 3:  Invalid argument.", cmd_sym->s_name);
    MY_ASSERT(x->replace_dict_sym == gensym(""), , "%s:  Arg 4:  Invalid argument.", cmd_sym->s_name);
    regexpr_set(x->search_key_expr, search_key_sym);

    x->replace_dict = dictobj_findregistered_retain(x->replace_dict_sym);
    MY_ASSERT(!x->replace_dict, , "%s:  Arg 4:  Unable to reference the dictionary named \"%s\".",
      cmd_sym->s_name, x->replace_dict_sym->s_name);
    break;
  
  default: break; }

  // Initialize the object variables
  if (_dict_recurse_begin_cmd(x, argv + 1, cmd_sym) != ERR_NONE) { return; }

  // Start the recursion
  _dict_recurse_dict(x, x->dict, 0);

  // Notify that the dictionary has been modified
  if (x->count > 0) {
    object_notify(x->dict, gensym("modified"), NULL); }

  // Post a summary for the command
  POST("%s:  %i entr%s appended in \"%s\".", cmd_sym->s_name, x->count, (x->count == 1) ? "y" : "ies", x->dict_sym->s_name);

  // End the command
  _dict_recurse_end_cmd(x);
}

// ====  DICT_RECURSE_DELETE  ====

/**
delete key (sym: dictionary) (sym: search key)
delete value (sym: dictionary) (sym: search value)
delete entry (sym: dictionary) (sym: search key) (sym: search value)
delete dict_cont_entry (sym: dictionary) (sym: search key) (sym: search value)
*/
void dict_recurse_delete(t_dict_recurse *x, t_symbol *sym, long argc, t_atom *argv)
{
  TRACE("dict_recurse_delete");

  t_symbol *search_key_sym = gensym("");
  t_symbol *search_val_sym = gensym("");

  // Arg 1: Find a key, all entries with the key in, a value, or a dictionary
  t_symbol *cmd_arg = atom_getsym(argv);
  t_symbol *cmd_sym = gensym("");
  if (cmd_arg == gensym("key")) { x->command = CMD_DELETE_KEY; cmd_sym = gensym("delete key"); }
  else if (cmd_arg == gensym("value")) { x->command = CMD_DELETE_VALUE_SYM; cmd_sym = gensym("delete value"); }
  else if (cmd_arg == gensym("entry")) { x->command = CMD_DELETE_ENTRY; cmd_sym = gensym("delete entry"); }
  else if (cmd_arg == gensym("dict_cont_entry")) { x->command = CMD_DELETE_DICT_CONT_ENTRY; cmd_sym = gensym("delete dict_cont_entry"); }
  else { MY_ASSERT(1, , "delete:  Arg 0:  Invalid argument."); }

  switch (x->command) {

  // delete key (sym: dictionary) (sym: search key)
  case CMD_DELETE_KEY:
    search_key_sym = atom_getsym(argv + 2);
    MY_ASSERT(search_key_sym == gensym(""), , "%s:  Arg 2:  Invalid argument.", cmd_sym->s_name);
    regexpr_set(x->search_key_expr, search_key_sym);
    break;

  // delete value (sym: dictionary) (sym: search value)
  case CMD_DELETE_VALUE_SYM:
    search_val_sym = atom_getsym(argv + 2);
    MY_ASSERT(search_val_sym == gensym(""), , "%s:  Arg 2:  Invalid argument.", cmd_sym->s_name);
    regexpr_set(x->search_val_expr, search_val_sym);
    break;

  // delete entry (sym: dictionary) (sym: search key) (sym: search value)
  // delete dict_cont_entry (sym: dictionary) (sym: search key) (sym: search value)
  case CMD_DELETE_ENTRY:
  case CMD_DELETE_DICT_CONT_ENTRY:
    search_key_sym = atom_getsym(argv + 2);
    search_val_sym = atom_getsym(argv + 3);
    MY_ASSERT(search_key_sym == gensym(""), , "%s:  Arg 2:  Invalid argument.", cmd_sym->s_name);
    MY_ASSERT(search_val_sym == gensym(""), , "%s:  Arg 3:  Invalid argument.", cmd_sym->s_name);
    regexpr_set(x->search_key_expr, search_key_sym);
    regexpr_set(x->search_val_expr, search_val_sym);
    break;
  
  default: break; }

  // Initialize the object variables
  if (_dict_recurse_begin_cmd(x, argv + 1, cmd_sym) != ERR_NONE) { return; }

  // Start the recursion
  _dict_recurse_dict(x, x->dict, 0);

  // Notify that the dictionary has been modified
  if (x->count > 0) {
    object_notify(x->dict, gensym("modified"), NULL); }

  // Post a summary for the command
  POST("%s:  %i deletion%s made in \"%s\".", cmd_sym->s_name, x->count, (x->count == 1) ? "" : "s", x->dict_sym->s_name);

  // End the command
  _dict_recurse_end_cmd(x);
}

// ====  _DICT_RECURSE_MATCH_SYM  ====

void  dict_recurse_test_match(t_dict_recurse *x, t_symbol *sym, long argc, t_atom *argv)
{
  t_symbol *expr = atom_getsym(argv);
  t_symbol *key_sym = atom_getsym(argv + 1);

  regexpr_set(x->search_key_expr, expr);
  regexpr_match(x->search_key_expr, key_sym);
  regexpr_reset(x->search_key_expr);
}

// ====  _DICT_RECURSE_MATCH_DICT  ====
/**
Look ahead function to test whether a dictionary matches a condition
*/
t_bool _dict_recurse_match_dict(t_dict_recurse *x, t_dictionary *dict, t_symbol **key_match, t_symbol **value_match)
{
  TRACE("_dict_recurse_match_dict");

  // Get the dictionary keys
  long  key_cnt = 0;
  t_symbol **key_arr = NULL;
  t_symbol *key = gensym("");
  t_atom value[1];

  *key_match = gensym("");
  *value_match = gensym("");

  dictionary_getkeys(dict, &key_cnt, &key_arr);

  t_bool test = false;
  for (t_int32 ind = 0; ind < key_cnt; ind++) {

    key = key_arr[ind];

    if (!regexpr_match(x->search_key_expr, key)) { continue; }

    dictionary_getatom(dict, key, value);
    if (regexpr_match(x->search_val_expr, atom_getsym(value))) {
      test = true;
      *key_match = key;
      *value_match = atom_getsym(value);
      break; } }

  if (key_arr) { dictionary_freekeys(dict, key_cnt, key_arr); }
  
  return test;
}

// ====  _DICT_RECURSE_DICT  ====

void _dict_recurse_dict(t_dict_recurse *x, t_dictionary *dict, t_int32 depth)
{
  TRACE("_dict_recurse_dict");

  t_atom atom[1];

  // ==== Store the state variables on the beginning of the function
  t_bool has_match_ini = x->has_match;
  t_int32 path_len_ini = (t_int32)strlen(x->path);
  t_value_type type_iter_ini = x->type_iter;
  t_dictionary *dict_iter_ini = x->dict_iter;
  t_symbol *key_iter_ini = x->key_iter;
  
  // ==== Add :: to the path
  strncat_zero(x->path, "::", x->path_len_max);

  // ==== Get the dictionary keys
  long  key_cnt = 0;
  t_symbol **key_arr = NULL;
  dictionary_getkeys(dict, &key_cnt, &key_arr);

  // ==== Increment the depth
  depth++;

  // ==== Set the trailing variables before for the recursions
  x->type_iter = VALUE_TYPE_DICT;
  x->dict_iter = dict;

  // ==== Loop through the keys
  for (t_int32 ind = 0; ind < key_cnt; ind++) {

    x->key_iter = key_arr[ind];

    // == Opening actions depending on which command is being processed
    switch (x->command) {
    
    case CMD_FIND_KEY_IN:
    case CMD_FIND_KEY:
      if (regexpr_match(x->search_key_expr, x->key_iter)) {
        x->has_match = true; x->count++; }    // x->has_match changed
      break;

    case CMD_REPLACE_KEY:
      if (regexpr_match(x->search_key_expr, x->key_iter)) {
        dictionary_getatom(dict, x->key_iter, atom);
        dictionary_chuckentry(dict, x->key_iter);
        dictionary_appendatom(dict, x->replace_key_sym, atom);
        x->count++;

        if (x->a_verbose == true) {
          POST("  %s%s  replaced by  \"%s\"",
            x->path, x->key_iter->s_name, x->replace_key_sym->s_name); }
        x->key_iter = x->replace_key_sym; }
      break;
    
    case CMD_DELETE_KEY:
      if (regexpr_match(x->search_key_expr, x->key_iter)) {
        dictionary_deleteentry(dict, x->key_iter);
        x->count++;

        if (x->a_verbose == true) {
          POST("  %s%s  deleted",
            x->path, x->key_iter->s_name, x->replace_key_sym->s_name); }

        continue; }    // NB: No further recursion
      break;

    case CMD_REPLACE_VALUE_FROM_DICT:
      if (regexpr_match(x->search_key_expr, x->key_iter)
          && dictionary_hasentry(x->replace_dict, x->key_iter)) {

        t_symbol *key_iter[2]; key_iter[0] = x->key_iter; key_iter[1] = NULL;
        dictionary_copyentries(x->replace_dict, dict, key_iter);    // NB: Strange it does not require the array size
        x->count++;

        if (x->a_verbose == true) {
          POST("  %s%s  replaced from  \"%s\"",
            x->path, x->key_iter->s_name, x->replace_dict_sym->s_name); }

        continue; }    // NB: No further recursion
      break;
        
    default: break;
    }    // >>>> END switch through potential commands

    // == Set the trailing variables before for the recursion
    strncat_zero(x->path, x->key_iter->s_name, x->path_len_max);    // NB: x->path changed
    
    // == Get the value and recurse to it
    dictionary_getatom(dict, x->key_iter, atom);    // NB: This creates a copy
    _dict_recurse_value(x, atom, depth);
    
    // == Reset the values that changed in this loop
    x->path[path_len_ini + 2] = '\0';
    x->has_match = has_match_ini; }    // >>>> END of loop through the keys
  
  // ==== Restore the remaining state variables to their beginning values
  x->type_iter = type_iter_ini;
  x->dict_iter = dict_iter_ini;
  x->key_iter = key_iter_ini;
  
  if (key_arr) { dictionary_freekeys(dict, key_cnt, key_arr); }
}

// ====  _DICT_RECURSE_VALUE_FIND  ====

void _dict_recurse_value_find(t_dict_recurse *x, t_atom *value, const char* str)
{
  TRACE("_dict_recurse_value_find");
  
  switch (x->command) {

  case CMD_FIND_KEY:
    if (x->has_match) { POST("  %s  %s", x->path, str); }
    x->has_match = false;    // reset matching state

  case CMD_FIND_KEY_IN:
    if (x->has_match) { POST("  %s  %s", x->path, str); }
    break;
  
  default: break; }
}

// ====  _DICT_RECURSE_VALUE  ====
/**
Called by _dict_recurse_dict() for each entry and _dict_recurse_array() for each index
*/
t_int32 _dict_recurse_value(t_dict_recurse *x, t_atom *value, t_int32 depth)
{
  TRACE("_dict_recurse_value");

  long type = atom_gettype(value);

  // ====  INT  ====
  if (type == A_LONG) {
    snprintf_zero(x->str_tmp, MAX_LEN_NUMBER, "%i", atom_getlong(value));
    _dict_recurse_value_find(x, value, x->str_tmp); }

  // ====  FLOAT  ====
  else if (type == A_FLOAT) {
    snprintf_zero(x->str_tmp, MAX_LEN_NUMBER, "%i", atom_getfloat(value));
    _dict_recurse_value_find(x, value, x->str_tmp); }
    
  // ====  SYMBOL / STRING  ====
  // NB: values in arrays are not A_SYM but A_OBJ 
  else if ((type == A_SYM) || atomisstring(value)) {

    t_symbol *value_sym = atom_getsym(value);

    switch (x->command) {
    
    // == FIND A SYMBOL VALUE
    case CMD_FIND_VALUE_SYM:

      if (regexpr_match(x->search_val_expr, value_sym)) {
        POST("  %s  \"%s\"", x->path, value_sym->s_name); x->count++; }
      break;

    // == FIND AN ENTRY
    case CMD_FIND_ENTRY:

      if (regexpr_match(x->search_key_expr, x->key_iter)
          && regexpr_match(x->search_val_expr, value_sym)
          && (x->type_iter == VALUE_TYPE_DICT)) {

        POST("  %s  \"%s\"", x->path, value_sym->s_name); x->count++; }
      break;

    // == REPLACE A SYMBOL VALUE
    case CMD_REPLACE_VALUE_SYM:
      
      if (regexpr_match(x->search_val_expr, value_sym)) {

        // If the value is from a dictionary entry
        if (x->type_iter == VALUE_TYPE_DICT) {
          dictionary_chuckentry(x->dict_iter, x->key_iter);
          dictionary_appendsym(x->dict_iter, x->key_iter, x->replace_val_sym); }

        // If the value is from an array
        else if (x->type_iter == VALUE_TYPE_ARRAY) { atom_setsym(value, x->replace_val_sym); }

        x->count++;
        if (x->a_verbose) {
          POST("  %s  \"%s\"  replaced by  \"%s\"",
            x->path, value_sym->s_name, x->replace_val_sym->s_name); } }
      break;

    // == REPLACE AN ENTRY
    case CMD_REPLACE_ENTRY:
      
      if (regexpr_match(x->search_key_expr, x->key_iter)
          && regexpr_match(x->search_val_expr, value_sym)
          && (x->type_iter == VALUE_TYPE_DICT)) {

        dictionary_chuckentry(x->dict_iter, x->key_iter);
        dictionary_appendsym(x->dict_iter, x->replace_key_sym, x->replace_val_sym);

        x->count++;
        if (x->a_verbose) {
          POST("  %s  \"%s\"  replaced by  (%s : %s)",
            x->path, value_sym->s_name, x->replace_key_sym->s_name, x->replace_val_sym->s_name); } }
      break;

    // == DELETE A SYMBOL VALUE
    case CMD_DELETE_VALUE_SYM:

      if (regexpr_match(x->search_val_expr, value_sym)) {

        // If the value is from a dictionary entry
        if (x->type_iter == VALUE_TYPE_DICT) {
          dictionary_deleteentry(x->dict_iter, x->key_iter); }

        // If the value is from an array
        else if (x->type_iter == VALUE_TYPE_ARRAY) {
          atomarray_chuckindex(x->array_iter, x->index_iter); }

        x->count++;
        if (x->a_verbose) {
          POST("  %s  \"%s\"  deleted",
            x->path, value_sym->s_name); }
        return VALUE_DEL; }
      break;

    // == DELETE A SYMBOL VALUE
    case CMD_DELETE_ENTRY:

      if (regexpr_match(x->search_key_expr, x->key_iter)
          && regexpr_match(x->search_val_expr, value_sym)
          && (x->type_iter == VALUE_TYPE_DICT)) {

        dictionary_deleteentry(x->dict_iter, x->key_iter);

        x->count++;
        if (x->a_verbose) {
          POST("  %s  \"%s\"  deleted",
            x->path, value_sym->s_name); }
        return VALUE_DEL; }
      break;

    // == DEFAULT: CMD_FIND_KEY or CMD_FIND_KEY_IN
    default:
      snprintf_zero(x->str_tmp, MAX_LEN_NUMBER, "\"%s\"", atom_getsym(value)->s_name);
      _dict_recurse_value_find(x, value, x->str_tmp);
      break; } }

  // ====  DICTIONARY  ====
  else if (atomisdictionary(value)) {

    t_dictionary *sub_dict = (t_dictionary *)atom_getobj(value);
    t_symbol *key_match = gensym("");
    t_symbol *value_match = gensym("");

    switch (x->command) {

    case CMD_FIND_DICT_CONT_ENTRY:
      
      if (_dict_recurse_match_dict(x, sub_dict, &key_match, &value_match)) {

        x->count++;
        POST("  %s:  dict containing  (%s : %s)",
          x->path, key_match->s_name, value_match->s_name); }
      break;

    case CMD_REPLACE_DICT_CONT_ENTRY:

      if (_dict_recurse_match_dict(x, sub_dict, &key_match, &value_match)) {

        t_dictionary *dict_cpy = dictionary_new();
        dictionary_clone_to_existing(x->replace_dict, dict_cpy);
        
        // If the value is from a dictionary entry
        if (x->type_iter == VALUE_TYPE_DICT) {
          dictionary_deleteentry(x->dict_iter, x->key_iter);
          dictionary_appenddictionary(x->dict_iter, x->key_iter, (t_object *)dict_cpy); }

        // If the value is from an array
        else if (x->type_iter == VALUE_TYPE_ARRAY) {
          long array_len;
          t_atom *atom_arr;
          atomarray_getatoms(x->array_iter, &array_len, &atom_arr);
          atom_setobj(atom_arr + x->index_iter, dict_cpy); } 

        x->count++;
        if (x->a_verbose) {
          POST("  %s:  dict containing  (%s : %s):  replaced by  \"%s\"",
            x->path, key_match->s_name, value_match->s_name, x->replace_dict_sym->s_name); } 
      
        return VALUE_NO_DEL; }
      break;

    case CMD_DELETE_DICT_CONT_ENTRY:

      if (_dict_recurse_match_dict(x, sub_dict, &key_match, &value_match)) {

        // If the value is from a dictionary entry
        if (x->type_iter == VALUE_TYPE_DICT) {
          dictionary_deleteentry(x->dict_iter, x->key_iter); }

        // If the value is from an array
        else if (x->type_iter == VALUE_TYPE_ARRAY) {
          atomarray_chuckindex(x->array_iter, x->index_iter);
          object_free(sub_dict); } 

        x->count++;
        if (x->a_verbose) {
          POST("  %s:  dict containing  (%s : %s):  deleted",
            x->path, key_match->s_name, value_match->s_name); }

        return VALUE_DEL; }
      break;

    case CMD_APPEND_IN_DICT_CONT_ENTRY:

      if (_dict_recurse_match_dict(x, sub_dict, &key_match, &value_match)) {

        dictionary_appendsym(sub_dict, x->replace_key_sym, x->replace_val_sym);

        x->count++;
        if (x->a_verbose) {
          POST("  %s:  dict containing  (%s : %s):  appended  (%s : %s)",
            x->path, key_match->s_name, value_match->s_name, x->replace_key_sym->s_name, x->replace_val_sym->s_name); } }
      break;

    case CMD_APPEND_IN_DICT_CONT_ENTRY_D:

      if (_dict_recurse_match_dict(x, sub_dict, &key_match, &value_match)
          && dictionary_hasentry(x->replace_dict, x->replace_key_sym)) {

        t_symbol *key[2]; key[0] = x->replace_key_sym; key[1] = NULL;
        dictionary_copyentries(x->replace_dict, sub_dict, key);    // NB: Strange it does not require the array size

        x->count++;
        if (x->a_verbose) {
          POST("  %s:  dict containing  (%s : %s):  appended entry  (%s : ...)  from \"%s\"",
            x->path, key_match->s_name, value_match->s_name, x->replace_key_sym->s_name, x->replace_dict_sym->s_name); } }
      break;

    case CMD_APPEND_IN_DICT_FROM_KEY:

      if (regexpr_match(x->search_key_expr, x->key_iter)
          && (x->type_iter == VALUE_TYPE_DICT)
          && dictionary_hasentry(x->replace_dict, x->replace_key_sym)) {

        t_symbol *key[2]; key[0] = x->replace_key_sym; key[1] = NULL;
        dictionary_copyentries(x->replace_dict, sub_dict, key);    // NB: Strange it does not require the array size
        
        x->count++;
        if (x->a_verbose) {
          POST("  %s:  dict value:  appended entry  (%s : ...)  from \"%s\"",
            x->path, x->replace_key_sym->s_name, x->replace_dict_sym->s_name); } }
      break;
    
    default:
      _dict_recurse_value_find(x, value, "_DICT_"); }    // End of command "switch ..."

    _dict_recurse_dict(x, sub_dict, depth); }    // End of dictionary "else if ..."

  // ====  ARRAY  ====
  else if (atomisatomarray(value)) {

    _dict_recurse_value_find(x, value, "_ARRAY_");

    t_atomarray *atomarray = (t_atomarray *)atom_getobj(value);
    _dict_recurse_array(x, atomarray, depth);  }

  return VALUE_NO_DEL;
}

// ====  _DICT_RECURSE_ARRAY  ====

void _dict_recurse_array(t_dict_recurse *x, t_atomarray *atomarray, t_int32 depth)
{
  TRACE("_dict_recurse_array");

  t_atom *value;

  // ==== Store the state variables on the beginning of the function
  t_int32 path_len = (t_int32)strlen(x->path);
  t_value_type type_iter_ini = x->type_iter;
  t_atomarray *array_iter_ini = x->array_iter;
  t_int32 index_iter_ini = x->index_iter;

  // ==== Add [ to the path
  strncat_zero(x->path, "[", x->path_len_max);

  // ==== Set the trailing variables before the recursions
  x->type_iter = VALUE_TYPE_ARRAY;
  x->array_iter = atomarray;

  // ==== Get the array of atoms associated with the atomarray
  long array_len;
  t_atom *atom_arr;
  atomarray_getatoms(atomarray, &array_len, &atom_arr);
  t_int32 test;

  // ==== Loop through the array
  for (t_int32 ind = 0; ind < array_len; ind++) {
    
    value = atom_arr + ind;
    x->index_iter = ind;

    // == Update the path
    snprintf_zero(x->str_tmp, MAX_LEN_NUMBER, "%i]", ind);
    strncat_zero(x->path, x->str_tmp, x->path_len_max);

    // == Recurse to the value
    test = _dict_recurse_value(x, value, depth);

    // == If there was a deletion reset the array and shift the index
    if (test == VALUE_DEL) {
      atomarray_getatoms(atomarray, &array_len, &atom_arr);
      ind--; }
  
    // == In loop resetting of the values that changed in this function
    x->path[path_len +  1] = '\0'; }    // End of the loop through the array

  // ==== Restore the remaining state variables to their beginning values
  x->type_iter = type_iter_ini;
  x->array_iter = array_iter_ini;
  x->index_iter = index_iter_ini;
}

// ====  DICT_RECURSE_BANG  ====

void dict_recurse_bang(t_dict_recurse *x)
{
  TRACE("dict_recurse_bang");
}

// ====  DICT_RECURSE_SET  ====

void dict_recurse_set(t_dict_recurse *x, t_symbol *sym, long argc, t_atom *argv)
{
  TRACE("dict_recurse_set");
}

// ====  DICT_RECURSE_GET  ====

void dict_recurse_get(t_dict_recurse *x, t_symbol *sym, long argc, t_atom *argv)
{
  TRACE("dict_recurse_get");
}

// ====  DICT_RECURSE_ANYTHING  ====

void dict_recurse_anything(t_dict_recurse *x, t_symbol *sym, long argc, t_atom *argv)
{
  TRACE("dict_recurse_anything");
  WARNING("The message \"%s\" is not recognized.", sym->s_name);
}
