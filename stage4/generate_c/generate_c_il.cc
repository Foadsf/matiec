/*
 * (c) 2003 Mario de Sousa
 *
 * Offered to the public under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details.
 *
 * This code is made available on the understanding that it will not be
 * used in safety-critical situations without a full and competent review.
 */

/*
 * An IEC 61131-3 IL and ST compiler.
 *
 * Based on the
 * FINAL DRAFT - IEC 61131-3, 2nd Ed. (2001-12-10)
 *
 */


/*
 * Conversion of il statements (i.e. IL code).
 *
 * This is part of the 4th stage that generates
 * a c++ source program equivalent to the IL and ST
 * code.
 */




/***********************************************************************/
/***********************************************************************/
/***********************************************************************/
/***********************************************************************/

/* Returns the data type of an il_operand.
 *
 * Note that the il_operand may be a variable, in which case
 * we return the type of the variable instance.
 * The il_operand may also be a constant, in which case
 * we return the data type of that constant.
 *
 * The variable instance may be a member of a structured variable,
 * or an element in an array, or any combination of the two.
 *
 * The class constructor must be given the search scope
 * (function, function block or program within which
 * the possible il_operand variable instance was declared).
 */

class search_il_operand_type_c {

  private:
    search_varfb_instance_type_c search_varfb_instance_type;
    search_constant_type_c search_constant_type;

  public:
    search_il_operand_type_c(symbol_c *search_scope): search_varfb_instance_type(search_scope) {}

  public:
    symbol_c *get_type(symbol_c *il_operand) {
      symbol_c *res;

      /* We first assume that it is a constant... */
      res = search_constant_type.get_type(il_operand);
      if (res != NULL) return res;

      /* Nope, now we assume it is a variable, and determine its type... */
      res = search_varfb_instance_type.get_type(il_operand);
      if (NULL != res) return res;

      /* not found */
      return NULL;
    }
};



/***********************************************************************/
/***********************************************************************/
/***********************************************************************/
/***********************************************************************/


/* A new class to ouput the il default variable to c++ code
 * We use this class, inheriting from symbol_c, so it may be used
 * as any other symbol_c object in the intermediate parse tree,
 * more specifically, so it can be used as any other il operand.
 * This makes the rest of the code much easier...
 *
 * Nevertheless, the basic visitor class visitor_c does not know
 * how to visit this new il_default_variable_c class, so we have
 * to extend that too.
 * In reality extending the basic symbols doesn't quite work out
 * as cleanly as desired (we need to use dynamic_cast in the
 * accept method of the il_default_variable_c), but it is cleaner
 * than the alternative...
 */
class il_default_variable_c;

/* This visitor class is not really required, we could place the
 * visit() method directly in genertae_cc_il_c, but doing it in
 * a seperate class makes the architecture more evident...
 */
class il_default_variable_visitor_c {
  public:
    virtual void *visit(il_default_variable_c *symbol) = 0;

    virtual ~il_default_variable_visitor_c(void) {return;}
};


/* A class to print out to the resulting C++ code
 * the IL default variable name.
 *
 * It includes a reference to its name,
 * and the data type of the data currently stored
 * in this C++ variable... This is required because the
 * C++ variable is a union, and we must know which member
 * of the union top reference!!
 *
 * Note that we also need to keep track of the data type of
 * the value currently being stored in the default variable.
 * This is required so we can process parenthesis,
 *
 * e.g. :
 *         LD var1
 *         AND (
 *         LD var2
 *         OR var3
 *         )
 *
 * Note that we only execute the 'AND (' operation when we come across
 * the ')', i.e. once we have evaluated the result of the
 * instructions inside the parenthesis.
 * When we do execute the 'AND (' operation, we need to know the data type
 * of the operand, which in this case is the result of the evaluation of the
 * instruction list inside the parenthesis. We can only know this if we
 * keep track of the data type currently stored in the default variable!
 *
 * We use the current_type inside the generate_c_il::default_variable_name variable
 * to track this!
 */
class il_default_variable_c: public symbol_c {
  public:
    symbol_c *var_name;  /* in principle, this should point to an indentifier_c */
    symbol_c *current_type;

  public:
    il_default_variable_c(const char *var_name_str, symbol_c *current_type);
    virtual void *accept(visitor_c &visitor);
};


/***********************************************************************/
/***********************************************************************/
/***********************************************************************/
/***********************************************************************/



class generate_c_il_c: public generate_c_typedecl_c, il_default_variable_visitor_c {

  private:
    /* When compiling il code, it becomes necessary to determine the
     * data type of il operands. To do this, we must first find the
     * il operand's declaration, within the scope of the function block
     * or function currently being processed.
     * The following object does just that...
     * This object instance will then later be called while the
     * remaining il code is being handled.
     */
    //search_il_operand_type_c *search_il_operand_type;
    search_expression_type_c *search_expression_type;

    /* The initial value that should be given to the IL default variable
     * imediately after a parenthesis is opened.
     * This variable is only used to pass data from the
     * il_expression_c visitor to the simple_instr_list_c visitor.
     *
     * e.g.:
     *         LD var1
     *         AND ( var2
     *         OR var3
     *         )
     *
     * In the above code sample, the line 'AND ( var2' constitutes
     * an il_expression_c, where var2 should be loaded into the
     * il default variable before continuing with the expression
     * inside the parenthesis.
     * Unfortunately, only the simple_instr_list_c may do the
     * initial laoding of the var2 bariable following the parenthesis,
     * so the il_expression_c visitor will have to pass 'var2' as a
     * parameter to the simple_instr_list_c visitor.
     * Ergo, the existance of the following parameter...!
     */
    symbol_c *il_default_variable_init_value;

    /* Operand to the IL operation currently being processed... */
    /* These variables are used to pass data from the
     * il_simple_operation_c and il_expression_c visitors
     * to the il operator visitors (i.e. LD_operator_c,
     * LDN_operator_c, ST_operator_c, STN_operator_c, ...)
     */
    symbol_c *current_operand;
    symbol_c *current_operand_type;

    /* Label to which the current IL jump operation should jump to... */
    /* This variable is used to pass data from the
     * il_jump_operation_c visitor
     * to the il jump operator visitors (i.e. JMP_operator_c,
     * JMPC_operator_c, JMPCN_operator_c, ...)
     */
    symbol_c *jump_label;

    /* The result of the comparison IL operations (GT, EQ, LT, ...)
     * is a boolean variable.
     * This class keeps track of the current data type stored in the
     * il default variable. This is usually done by keeping a reference
     * to the data type of the last operand. Nevertheless, in the case of
     * the comparison IL operators, the data type of the result (a boolean)
     * is not the data type of the operand. We therefore need an object
     * of the boolean data type to keep as a reference of the current
     * data type.
     * The following object is it...
     */
    bool_type_name_c bool_type;

    /* the data type of the IL default variable... */
    #define IL_DEFVAR_T VAR_LEADER "IL_DEFVAR_T"
    /* The name of the IL default variable... */
    #define IL_DEFVAR   VAR_LEADER "IL_DEFVAR"
    /* The name of the variable used to pass the result of a
     * parenthesised instruction list to the immediately preceding
     * scope ...
     */
    #define IL_DEFVAR_BACK   VAR_LEADER "IL_DEFVAR_BACK"
    il_default_variable_c default_variable_name;
    il_default_variable_c default_variable_back_name;

    /* Some function calls in the body of functions or function blocks
     * may leave some parameters to their default values, and
     * ignore some output parameters of the function being called.
     * Our conversion of ST functions to C++ does not contemplate that,
     * i.e. each called function must get all it's input and output
     * parameters set correctly.
     * For input parameters we merely need to call the function with
     * the apropriate default value, but for output parameters
     * we must create temporary variables to hold the output value.
     *
     * We declare all the temporary output variables at the begining of
     * the body of each function or function block, and use them as
     * in function calls later on as they become necessary...
     * Note that we cannot create these variables just before a function
     * call, as the function call itself may be integrated within an
     * expression, or another function call!
     *
     * The variables are declared in the exact same order in which they
     * will be used later on during the function calls, which allows us
     * to simply re-create the name that was used for the temporary variable
     * instead of keeping it in some list.
     * The names are recreated by the temp_var_name_factory, after reset()
     * has been called!
     *
     * This function will genertae code similar to...
     *
     *     INT __TMP_0 = 23;
     *     REAL __TMP_1 = 45.5;
     *     ...
     */
    temp_var_name_c temp_var_name_factory;

    /* When calling a function block, we must first find it's type,
     * by searching through the declarations of the variables currently
     * in scope.
     * This class does just that...
     * A new class is instantiated whenever we begin generating the code
     * for a function block type declaration, or a program declaration.
     * This object instance will then later be called while the
     * function block's or the program's body is being handled.
     *
     * Note that functions cannot contain calls to function blocks,
     * so we do not create an object instance when handling
     * a function declaration.
     */
    search_fb_instance_decl_c *search_fb_instance_decl;


  public:
    generate_c_il_c(stage4out_c *s4o_ptr, symbol_c *scope, const char *variable_prefix = NULL)
    : generate_c_typedecl_c(s4o_ptr),
      default_variable_name(IL_DEFVAR, NULL),
      default_variable_back_name(IL_DEFVAR_BACK, NULL)
    {
      //search_il_operand_type  = new search_il_operand_type_c(scope);
      search_expression_type = new search_expression_type_c(scope);
      search_fb_instance_decl = new search_fb_instance_decl_c(scope);
      current_operand = NULL;
      current_operand_type = NULL;
      il_default_variable_init_value = NULL;
      this->set_variable_prefix(variable_prefix);
    }

    virtual ~generate_c_il_c(void) {
      delete search_fb_instance_decl;
      //delete search_il_operand_type;
      delete search_expression_type;
    }

    void generate(instruction_list_c *il) {
      generate_c_tempvardecl_c generate_c_tempvardecl(&s4o);
      generate_c_tempvardecl.generate(il, &temp_var_name_factory);
      il->accept(*this);
    }

    /* Declare the backup to the default variable, that will store the result
     * of the IL operations executed inside a parenthesis...
     */
    void declare_backup_variable(void) {
      s4o.print(s4o.indent_spaces);
      s4o.print(IL_DEFVAR_T);
      s4o.print(" ");
      print_backup_variable();
      s4o.print(";\n");
    }
    
    void print_backup_variable(void) {
      this->default_variable_back_name.accept(*this);
    }

  private:
    /* A helper function... */
    /*
    bool is_bool_type(symbol_c *type_symbol) {
      return (NULL != dynamic_cast<bool_type_name_c *>(type_symbol));
    }
    */

    /* A helper function... */
    void *XXX_operator(symbol_c *lo, const char *op, symbol_c *ro) {
      if ((NULL == lo) || (NULL == ro)) ERROR;
      if (NULL == op) ERROR;

      lo->accept(*this);
      s4o.print(op);
      ro->accept(*this);
      return NULL;
    }

    /* A helper function... */
    void *XXX_function(const char *func, symbol_c *lo, symbol_c *ro) {
      if ((NULL == lo) || (NULL == ro)) ERROR;
      if (NULL == func) ERROR;

      lo->accept(*this);
      s4o.print(" = ");
      s4o.print(func);
      s4o.print("(");
      lo->accept(*this);
      s4o.print(", ");
      ro->accept(*this);
      s4o.print(")");
      return NULL;
    }

    /* A helper function... */
    void *XXX_CAL_operator(const char *param_name, symbol_c *fb_name) {
      if (NULL == fb_name) ERROR;
      symbolic_variable_c *sv = dynamic_cast<symbolic_variable_c *>(fb_name);
      if (NULL == sv) ERROR;
      identifier_c *id = dynamic_cast<identifier_c *>(sv->var_name);
      if (NULL == id) ERROR;

      identifier_c param(param_name);

      //SYM_REF3(il_param_assignment_c, il_assign_operator, il_operand, simple_instr_list)
      il_param_assignment_c il_param_assignment(&param, &this->default_variable_name, NULL);
      // SYM_LIST(il_param_list_c)
      il_param_list_c il_param_list;
      il_param_list.add_element(&il_param_assignment);
      CAL_operator_c CAL_operator;
      // SYM_REF4(il_fb_call_c, il_call_operator, fb_name, il_operand_list, il_param_list)
      il_fb_call_c il_fb_call(&CAL_operator, id, NULL, &il_param_list);

      il_fb_call.accept(*this);
      return NULL;
    }

    /* A helper function... */
    void *CMP_operator(symbol_c *o, const char *operation) {
      if (NULL == o) ERROR;
      if (NULL == this->default_variable_name.current_type) ERROR;

      symbol_c *backup = this->default_variable_name.current_type;
      this->default_variable_name.current_type = &(this->bool_type);
      this->default_variable_name.accept(*this);
      this->default_variable_name.current_type = backup;

      s4o.print(" = ");
      s4o.print(operation);
      this->default_variable_name.current_type->accept(*this);
      s4o.print("(2, ");
      this->default_variable_name.accept(*this);
      s4o.print(", ");
      o->accept(*this);
      s4o.print(")");

      /* the data type resulting from this operation... */
      this->default_variable_name.current_type = &(this->bool_type);
      return NULL;
    }


    /* A helper function... */
    void C_modifier(void) {
      if (search_expression_type->is_bool_type(default_variable_name.current_type)) {
        s4o.print("if (");
        this->default_variable_name.accept(*this);
        s4o.print(") ");
      }
      else {ERROR;}
    }

    /* A helper function... */
    void CN_modifier(void) {
      if (search_expression_type->is_bool_type(default_variable_name.current_type)) {
        s4o.print("if (!");
        this->default_variable_name.accept(*this);
        s4o.print(") ");
      }
      else {ERROR;}
    }


public:
void *visit(il_default_variable_c *symbol) {
  //s4o.print("il_default_variable_c VISITOR!!\n");
  symbol->var_name->accept(*this);
  if (NULL != symbol->current_type) {
    s4o.print(".");
    symbol->current_type->accept(*this);
    s4o.print("var");
  }
  return NULL;
}


private:

/********************************************/
/* B.1.4.1   Directly Represented Variables */
/********************************************/
// direct_variable: direct_variable_token   {$$ = new direct_variable_c($1);};
void *visit(direct_variable_c *symbol) {
  TRACE("direct_variable_c");
  /* Do not use print_token() as it will change everything into uppercase */
  if (strlen(symbol->value) == 0) ERROR;
  s4o.print("*(");
  this->print_variable_prefix();
  s4o.printlocation(symbol->value + 1);
  s4o.print(")");
  return NULL;
}

/****************************************/
/* B.2 - Language IL (Instruction List) */
/****************************************/

/***********************************/
/* B 2.1 Instructions and Operands */
/***********************************/

/* please see the comment before the RET_operator_c visitor for details... */
#define END_LABEL VAR_LEADER "end"

/*| instruction_list il_instruction */
void *visit(instruction_list_c *symbol) {
  
  /* Declare the backup to the default variable, that will store the result
   * of the IL operations executed inside a parenthesis...
   */
  declare_backup_variable();
  
  /* Declare the default variable, that will store the result of the IL operations... */
  s4o.print(s4o.indent_spaces);
  s4o.print(IL_DEFVAR_T);
  s4o.print(" ");
  this->default_variable_name.accept(*this);
  s4o.print(";\n\n");

  print_list(symbol, s4o.indent_spaces, ";\n" + s4o.indent_spaces, ";\n");

  /* write the label marking the end of the code block */
  /* please see the comment before the RET_operator_c visitor for details... */
  s4o.print("\n");
  s4o.print(s4o.indent_spaces);
  s4o.print(END_LABEL);
  s4o.print(":\n");
  s4o.indent_right();
  /* since every label must be followed by at least one statement, and
   * only the functions will introduce the return statement after this label,
   * function blocks written in IL would result in invalid C++ code.
   * To work around this we introduce the equivalent of a 'nop' operation
   * to humour the compiler...
   */
  s4o.print(s4o.indent_spaces);
  s4o.print("/* to humour the compiler, we insert a nop */\n");
  s4o.print(s4o.indent_spaces);
  this->default_variable_name.accept(*this);
  s4o.print(" = ");
  this->default_variable_name.accept(*this);
  s4o.print(";\n");
  s4o.indent_left();

  return NULL;
}


/* | label ':' [il_incomplete_instruction] eol_list */
// SYM_REF2(il_instruction_c, label, il_instruction)
void *visit(il_instruction_c *symbol) {
  if (NULL != symbol->label) {
    symbol->label->accept(*this);
    s4o.print(":\n");
    s4o.print(s4o.indent_spaces);
  }
  symbol->il_instruction->accept(*this);
  return NULL;
}

/* | il_simple_operator [il_operand] */
//SYM_REF2(il_simple_operation_c, il_simple_operator, il_operand)
void *visit(il_simple_operation_c *symbol) {
  this->current_operand = symbol->il_operand;
  if (NULL == this->current_operand) {
    this->current_operand_type = NULL;
  } else {
    this->current_operand_type = search_expression_type->get_type(this->current_operand);
    if (NULL == this->current_operand_type) ERROR;
  }

  symbol->il_simple_operator->accept(*this);

  this->current_operand = NULL;
  this->current_operand_type = NULL;
  return NULL;
}


/* | function_name [il_operand_list] */
// SYM_REF2(il_function_call_c, function_name, il_operand_list)
void *visit(il_function_call_c *symbol) {
  function_declaration_c *f_decl = function_symtable.find_value(symbol->function_name);

  if (f_decl == function_symtable.end_value()) {
    /* should never occur. The function being called MUST be in the symtable... */
    function_type_t current_function_type = get_function_type((identifier_c *)symbol->function_name);
    if (current_function_type == function_none) ERROR;
    
    symbol_c *param_data_type = default_variable_name.current_type;
    symbol_c *return_data_type = (symbol_c *)search_expression_type->compute_standard_function_il(symbol, param_data_type);
    if (NULL == return_data_type) ERROR;
    
    default_variable_name.current_type = return_data_type;
    this->default_variable_name.accept(*this);
    default_variable_name.current_type = param_data_type;
    s4o.print(" = ");
    
    function_call_param_iterator_c function_call_param_iterator(symbol);
    
    int nb_param = 1;
    if (symbol->il_operand_list != NULL)
      nb_param += ((list_c *)symbol->il_operand_list)->n;

#include "il_code_gen.c"

#if 0
    for(int current_param = 0; current_param < nb_param; current_param++) {
      symbol_c *param_value;
      if (current_param == 0)
        param_value = &this->default_variable_name;
      else {
        symbol_c *param_name = NULL;
        switch (current_function_type) {
          default: ERROR;
        }
        
         
        /* Get the value from a foo(<param_name> = <param_value>) style call */
        param_value = function_call_param_iterator.search(param_name);
        delete param_name;
        
        /* Get the value from a foo(<param_value>) style call */
        if (param_value == NULL)
          param_value = function_call_param_iterator.next();
        
        if (param_value == NULL) ERROR;
      }
      
      switch (current_function_type) {
        case (function_sqrt):
          if (current_param == 0) {
            s4o.print("sqrt(");
            param_value->accept(*this);
            s4o.print(")");
          }
          else ERROR;
          break;
        default: ERROR;
      }
    } /* for(...) */
#endif

    /* the data type returned by the function, and stored in the il default variable... */
    default_variable_name.current_type = return_data_type;
  }
  else {
    /* determine the base data type returned by the function being called... */
    search_base_type_c search_base_type;
    symbol_c *return_data_type = (symbol_c *)f_decl->type_name->accept(search_base_type);
    symbol_c *param_data_type = default_variable_name.current_type;
    if (NULL == return_data_type) ERROR;
  
    default_variable_name.current_type = return_data_type;
    this->default_variable_name.accept(*this);
    default_variable_name.current_type = param_data_type;
    s4o.print(" = ");
  
    symbol->function_name->accept(*this);
    s4o.print("(");
  
    /* loop through each function parameter, find the value we should pass
     * to it, and then output the c equivalent...
     */
  
    function_param_iterator_c fp_iterator(f_decl);
    identifier_c *param_name;
    function_call_param_iterator_c function_call_param_iterator(symbol);
    for(int i = 1; (param_name = fp_iterator.next()) != NULL; i++) {
      if (i != 1)
        s4o.print(", ");
  
      symbol_c *param_type = fp_iterator.param_type();
      if (param_type == NULL) ERROR;
  
      function_param_iterator_c::param_direction_t param_direction = fp_iterator.param_direction();
  
  
      symbol_c *param_value = NULL;
  
      /* if it is the first parameter, semantics specifies that we should
       * get the value off the IL default variable!
       */
     if (1 == i)
       param_value = &this->default_variable_name;
  
      /* Get the value from a foo(<param_name> = <param_value>) style call */
      /* NOTE: the following line of code is not required in this case, but it doesn't
       * harm to leave it in, as in the case of a non-formal syntax function call,
       * it will always return NULL.
       * We leave it in in case we later decide to merge this part of the code together
       * with the function calling code in generate_c_st_c, which does require
       * the following line...
       */
      if (param_value == NULL)
        param_value = function_call_param_iterator.search(param_name);
  
      /* Get the value from a foo(<param_value>) style call */
      if (param_value == NULL)
        param_value = function_call_param_iterator.next();
  
      switch (param_direction) {
        case function_param_iterator_c::direction_in:
          if (param_value == NULL) {
            /* No value given for parameter, so we must use the default... */
            /* First check whether default value specified in function declaration...*/
            param_value = fp_iterator.default_value();
          }
          if (param_value == NULL) {
            /* If not, get the default value of this variable's type */
            param_value = (symbol_c *)param_type->accept(*type_initial_value_c::instance());
          }
          if (param_value == NULL) ERROR;
          param_value->accept(*this);
          break;
        case function_param_iterator_c::direction_out:
        case function_param_iterator_c::direction_inout:
          if (param_value == NULL) {
  	  /* no parameter value given, so we pass a previously declared temporary variable. */
            std::string *temp_var_name = temp_var_name_factory.new_name();
            s4o.print(*temp_var_name);
            delete temp_var_name;
          } else {
            param_value->accept(*this);
          }
          break;
        case function_param_iterator_c::direction_extref:
          /* TODO! */
          ERROR;
          break;
      } /* switch */
    } /* for(...) */
  
    s4o.print(")");
    /* the data type returned by the function, and stored in the il default variable... */
    default_variable_name.current_type = return_data_type;
  }
  
  return NULL;
}


/* | il_expr_operator '(' [il_operand] eol_list [simple_instr_list] ')' */
//SYM_REF4(il_expression_c, il_expr_operator, il_operand, simple_instr_list, unused)
void *visit(il_expression_c *symbol) {
  /* We will be recursevely interpreting an instruction list,
   * so we store a backup of the data type of the value currently stored
   * in the default variable, and set the current data type to NULL
   */
  symbol_c *old_current_default_variable_data_type = this->default_variable_name.current_type;
  this->default_variable_name.current_type = NULL;

 /* Pass the symbol->il_operand to the simple_instr_list visitor
  * using the il_default_variable_init_value parameter...
  * Note that the simple_instr_list_c visitor will set this parameter
  * to NULL as soon as it does not require it any longer,
  * so we don't do it here again after the
  *   symbol->simple_instr_list->accept(*this);
  * returns...
  */
  this->il_default_variable_init_value = symbol->il_operand;

  /* Now do the parenthesised instructions... */
  /* NOTE: the following code line will get the variable
   * this->default_variable_name.current_type updated!
   */
  symbol->simple_instr_list->accept(*this);

  /* Now do the operation, using the previous result! */
  /* NOTE: The result of the previous instruction list will be stored
   * in a variable named IL_DEFVAR_BACK. This is done in the visitor
   * to instruction_list_c objects...
   */
  this->current_operand = &(this->default_variable_back_name);
  this->current_operand_type = this->default_variable_back_name.current_type;

  this->default_variable_name.current_type = old_current_default_variable_data_type;
  if (NULL == this->current_operand_type) ERROR;

  symbol->il_expr_operator->accept(*this);

  this->current_operand = NULL;
  this->current_operand_type = NULL;
  this->default_variable_back_name.current_type = NULL;
  return NULL;
}

/*  il_jump_operator label */
// SYM_REF2(il_jump_operation_c, il_jump_operator, label)
void *visit(il_jump_operation_c *symbol) {
 /* Pass the symbol->label to the il_jump_operation visitor
  * using the jump_label parameter...
  */
  this->jump_label = symbol->label;
  symbol->il_jump_operator->accept(*this);
  this->jump_label = NULL;

  return NULL;
}

/*   il_call_operator prev_declared_fb_name
 * | il_call_operator prev_declared_fb_name '(' ')'
 * | il_call_operator prev_declared_fb_name '(' eol_list ')'
 * | il_call_operator prev_declared_fb_name '(' il_operand_list ')'
 * | il_call_operator prev_declared_fb_name '(' eol_list il_param_list ')'
 */
// SYM_REF4(il_fb_call_c, il_call_operator, fb_name, il_operand_list, il_param_list)
void *visit(il_fb_call_c *symbol) {
  symbol->il_call_operator->accept(*this);
  s4o.print("{\n");
  s4o.indent_right();
  s4o.print(s4o.indent_spaces);

  /* first figure out what is the name of the function block type of the function block being called... */
  symbol_c *function_block_type_name = this->search_fb_instance_decl->get_type_name(symbol->fb_name);
    /* should never occur. The function block instance MUST have been declared... */
  if (function_block_type_name == NULL) ERROR;

  /* Now find the declaration of the function block type being called... */
  function_block_declaration_c *fb_decl = function_block_type_symtable.find_value(function_block_type_name);
    /* should never occur. The function block type being called MUST be in the symtable... */
  if (fb_decl == function_block_type_symtable.end_value()) ERROR;

  /* loop through each function block parameter, find the value we should pass
   * to it, and then output the c equivalent...
   */
  function_param_iterator_c fp_iterator(fb_decl);
  identifier_c *param_name;
  function_call_param_iterator_c function_call_param_iterator(symbol);
  for(int i = 1; (param_name = fp_iterator.next()) != NULL; i++) {
    function_param_iterator_c::param_direction_t param_direction = fp_iterator.param_direction();

    /* Get the value from a foo(<param_name> = <param_value>) style call */
    symbol_c *param_value = function_call_param_iterator.search(param_name);

    /* Get the value from a foo(<param_value>) style call */
    if (param_value == NULL)
      param_value = function_call_param_iterator.next();

    /* now output the value assignment */
    if (param_value != NULL)
      if ((param_direction == function_param_iterator_c::direction_in) ||
          (param_direction == function_param_iterator_c::direction_inout)) {
        symbol->fb_name->accept(*this);
        s4o.print(".");
        param_name->accept(*this);
        s4o.print(" = ");
        param_value->accept(*this);
        s4o.print(";\n" + s4o.indent_spaces);
      }
  } /* for(...) */

  /* now call the function... */
  function_block_type_name->accept(*this);
  s4o.print(FB_FUNCTION_SUFFIX);
  s4o.print("(&");
  symbol->fb_name->accept(*this);
  s4o.print(")");

  /* loop through each function parameter, find the variable to which
   * we should atribute the value of all output or inoutput parameters.
   */
  fp_iterator.reset();
  function_call_param_iterator.reset();
  for(int i = 1; (param_name = fp_iterator.next()) != NULL; i++) {
    function_param_iterator_c::param_direction_t param_direction = fp_iterator.param_direction();

    /* Get the value from a foo(<param_name> = <param_value>) style call */
    symbol_c *param_value = function_call_param_iterator.search(param_name);

    /* Get the value from a foo(<param_value>) style call */
    if (param_value == NULL)
      param_value = function_call_param_iterator.next();

    /* now output the value assignment */
    if (param_value != NULL)
      if ((param_direction == function_param_iterator_c::direction_out) ||
          (param_direction == function_param_iterator_c::direction_inout)) {
        s4o.print(";\n"+ s4o.indent_spaces);
        param_value->accept(*this);
        s4o.print(" = ");
        symbol->fb_name->accept(*this);
        s4o.print(".");
        param_name->accept(*this);
      }
  } /* for(...) */

  s4o.print(";\n");
  s4o.indent_left();
  s4o.print(s4o.indent_spaces);
  s4o.print("}");

  return NULL;
}



/* | function_name '(' eol_list [il_param_list] ')' */
// SYM_REF2(il_formal_funct_call_c, function_name, il_param_list)
void *visit(il_formal_funct_call_c *symbol) {
  function_declaration_c *f_decl = function_symtable.find_value(symbol->function_name);

  if (f_decl == function_symtable.end_value())
    /* should never occur. The function being called MUST be in the symtable... */
    ERROR;

  symbol->function_name->accept(*this);
  s4o.print("(");

  /* loop through each function parameter, find the value we should pass
   * to it, and then output the c equivalent...
   */

  function_param_iterator_c fp_iterator(f_decl);
  identifier_c *param_name;
  function_call_param_iterator_c function_call_param_iterator(symbol);
  for(int i = 1; (param_name = fp_iterator.next()) != NULL; i++) {
    if (i != 1)
      s4o.print(", ");

    symbol_c *param_type = fp_iterator.param_type();
    if (param_type == NULL) ERROR;

    function_param_iterator_c::param_direction_t param_direction = fp_iterator.param_direction();


    symbol_c *param_value = NULL;

    /* Get the value from a foo(<param_name> = <param_value>) style call */
    if (param_value == NULL)
      param_value = function_call_param_iterator.search(param_name);

    /* Get the value from a foo(<param_value>) style call */
    /* NOTE: the following line of code is not required in this case, but it doesn't
     * harm to leave it in, as in the case of a formal syntax function call,
     * it will always return NULL.
     * We leave it in in case we later decide to merge this part of the code together
     * with the function calling code in generate_c_st_c, which does require
     * the following line...
     */
    if (param_value == NULL)
      param_value = function_call_param_iterator.next();

    switch (param_direction) {
      case function_param_iterator_c::direction_in:
        if (param_value == NULL) {
          /* No value given for parameter, so we must use the default... */
          /* First check whether default value specified in function declaration...*/
          param_value = fp_iterator.default_value();
        }
        if (param_value == NULL) {
          /* If not, get the default value of this variable's type */
          param_value = (symbol_c *)param_type->accept(*type_initial_value_c::instance());
        }
        if (param_value == NULL) ERROR;
        param_value->accept(*this);
	break;
      case function_param_iterator_c::direction_out:
      case function_param_iterator_c::direction_inout:
        if (param_value == NULL) {
	  /* no parameter value given, so we pass a previously declared temporary variable. */
          std::string *temp_var_name = temp_var_name_factory.new_name();
          s4o.print(*temp_var_name);
          delete temp_var_name;
	} else {
          param_value->accept(*this);
	}
	break;
      case function_param_iterator_c::direction_extref:
        /* TODO! */
        ERROR;
	break;
    } /* switch */
  } /* for(...) */

  // symbol->parameter_assignment->accept(*this);
  s4o.print(")");
  return NULL;
}


/* | il_operand_list ',' il_operand */
// SYM_LIST(il_operand_list_c)
void *visit(il_operand_list_c *symbol) {ERROR; return NULL;} // should never get called!


/* | simple_instr_list il_simple_instruction */
// SYM_LIST(simple_instr_list_c)
void *visit(simple_instr_list_c *symbol) {
  /* A simple_instr_list_c is used to store a list of il operations
   * being done within parenthesis...
   *
   * e.g.:
   *         LD var1
   *         AND ( var2
   *         OR var3
   *         OR var4
   *         )
   *
   * This will be converted to C++ by defining a new scope
   * with a new il default variable, and executing the il operands
   * within this new scope.
   * At the end of the scope the result, i.e. the value currently stored
   * in the il default variable is copied to the variable used to take this
   * value to the outside scope...
   *
   * The above example will result in the following C++ code:
   * {__IL_DEFVAR_T __IL_DEFVAR_BACK;
   *  __IL_DEFVAR_T __IL_DEFVAR;
   *
   *  __IL_DEFVAR.INTvar = var1;
   *  {
   *    __IL_DEFVAR_T __IL_DEFVAR;
   *
   *    __IL_DEFVAR.INTvar = var2;
   *    __IL_DEFVAR.INTvar |= var3;
   *    __IL_DEFVAR.INTvar |= var4;
   *
   *    __IL_DEFVAR_BACK = __IL_DEFVAR;
   *  }
   *  __IL_DEFVAR.INTvar &= __IL_DEFVAR_BACK.INTvar;
   *
   * }
   *
   *  The intial value of the il default variable (in the above
   * example 'var2') is passed to this simple_instr_list_c visitor
   * using the il_default_variable_init_value parameter.
   * Since it is possible to have parenthesis inside other parenthesis
   * recursively, we reset the il_default_variable_init_value to NULL
   * as soon as we no longer require it, as it may be used once again
   * in the line
   *  print_list(symbol, s4o.indent_spaces, ";\n" + s4o.indent_spaces, ";\n");
   *
   */

  /* Declare the default variable, that will store the result of the IL operations... */
  s4o.print("{\n");
  s4o.indent_right();

  s4o.print(s4o.indent_spaces);
  s4o.print(IL_DEFVAR_T);
  s4o.print(" ");
  this->default_variable_name.accept(*this);
  s4o.print(";\n\n");

  /* Check whether we should initiliase the il default variable... */
  if (NULL != this->il_default_variable_init_value) {
    /* Yes, we must... */
    /* We will do it by instatiating a LD operator, and having this
     * same generate_c_il_c class visiting it!
     */
    LD_operator_c ld_oper;
    il_simple_operation_c il_simple_oper(&ld_oper, this->il_default_variable_init_value);

    s4o.print(s4o.indent_spaces);
    il_simple_oper.accept(*this);
    s4o.print(";\n");
  }

  /* this parameter no longer required... */
  this->il_default_variable_init_value = NULL;

  print_list(symbol, s4o.indent_spaces, ";\n" + s4o.indent_spaces, ";\n");

  /* copy the result in the default variable to the variable
   * used to pass the data out to the scope enclosing
   * the current scope!
   *
   * We also need to update the data type currently stored within
   * the variable used to pass the data to the outside scope...
   */
  this->default_variable_back_name.current_type = this->default_variable_name.current_type;
  s4o.print("\n");
  s4o.print(s4o.indent_spaces);
  this->default_variable_back_name.accept(*this);
  s4o.print(" = ");
  this->default_variable_name.accept(*this);
  s4o.print(";\n");

  s4o.indent_left();
  s4o.print(s4o.indent_spaces);
  s4o.print("}\n");
  s4o.print(s4o.indent_spaces);
  return NULL;
}

/* | il_initial_param_list il_param_instruction */
// SYM_LIST(il_param_list_c)
void *visit(il_param_list_c *symbol) {ERROR; return NULL;} // should never get called!

/*  il_assign_operator il_operand
 * | il_assign_operator '(' eol_list simple_instr_list ')'
 */
// SYM_REF4(il_param_assignment_c, il_assign_operator, il_operand, simple_instr_list, unused)
void *visit(il_param_assignment_c *symbol) {ERROR; return NULL;} // should never get called!

/*  il_assign_out_operator variable */
// SYM_REF2(il_param_out_assignment_c, il_assign_out_operator, variable);
void *visit(il_param_out_assignment_c *symbol) {ERROR; return NULL;} // should never get called!

/*******************/
/* B 2.2 Operators */
/*******************/

void *visit(LD_operator_c *symbol)	{
  /* the data type resulting from this operation... */
  this->default_variable_name.current_type = this->current_operand_type;
  XXX_operator(&(this->default_variable_name), " = ", this->current_operand);
  return NULL;
}

void *visit(LDN_operator_c *symbol)	{
  /* the data type resulting from this operation... */
  this->default_variable_name.current_type = this->current_operand_type;
  XXX_operator(&(this->default_variable_name),
               search_expression_type->is_bool_type(this->current_operand_type)?" = !":" = ~",
               this->current_operand);
  return NULL;
}

void *visit(ST_operator_c *symbol)	{
  XXX_operator(this->current_operand, " = ",&(this->default_variable_name));
  /* the data type resulting from this operation is unchamged. */
  return NULL;
}

void *visit(STN_operator_c *symbol)	{
  XXX_operator(this->current_operand,
               search_expression_type->is_bool_type(this->current_operand_type)?" = !":" = ~",
               &(this->default_variable_name));
  /* the data type resulting from this operation is unchamged. */
  return NULL;
}

void *visit(NOT_operator_c *symbol)	{
  if ((NULL != this->current_operand) || (NULL != this->current_operand_type)) ERROR;
  XXX_operator(&(this->default_variable_name),
               search_expression_type->is_bool_type(this->default_variable_name.current_type)?" = !":" = ~",
               &(this->default_variable_name));
  /* the data type resulting from this operation is unchanged. */
  return NULL;
}

void *visit(S_operator_c *symbol)	{
  if ((NULL == this->current_operand) || (NULL == this->current_operand_type)) ERROR;

  C_modifier();
  this->current_operand->accept(*this);
  s4o.print(search_expression_type->is_bool_type(this->current_operand_type)?" = true":" = 1");
  /* the data type resulting from this operation is unchanged! */
  return NULL;
}

void *visit(R_operator_c *symbol)	{
  if ((NULL == this->current_operand) || (NULL == this->current_operand_type)) ERROR;

  C_modifier();
  this->current_operand->accept(*this);
  s4o.print(search_expression_type->is_bool_type(this->current_operand_type)?" = false":" = 0");
  /* the data type resulting from this operation is unchanged! */
  return NULL;
}

void *visit(S1_operator_c *symbol)	{return XXX_CAL_operator("S1", this->current_operand);}
void *visit(R1_operator_c *symbol)	{return XXX_CAL_operator("R1", this->current_operand);}
void *visit(CLK_operator_c *symbol)	{return XXX_CAL_operator("CLK", this->current_operand);}
void *visit(CU_operator_c *symbol)	{return XXX_CAL_operator("CU", this->current_operand);}
void *visit(CD_operator_c *symbol)	{return XXX_CAL_operator("CD", this->current_operand);}
void *visit(PV_operator_c *symbol)	{return XXX_CAL_operator("PV", this->current_operand);}
void *visit(IN_operator_c *symbol)	{return XXX_CAL_operator("IN", this->current_operand);}
void *visit(PT_operator_c *symbol)	{return XXX_CAL_operator("PT", this->current_operand);}

void *visit(AND_operator_c *symbol)	{
  if (search_expression_type->is_binary_type(this->default_variable_name.current_type) &&
      search_expression_type->is_same_type(this->default_variable_name.current_type, this->current_operand_type)) {
    XXX_operator(&(this->default_variable_name), " &= ", this->current_operand);
    /* the data type resulting from this operation... */
    this->default_variable_name.current_type = this->current_operand_type;
  }
  else {ERROR;}
  return NULL;
}

void *visit(OR_operator_c *symbol)	{
  if (search_expression_type->is_binary_type(this->default_variable_name.current_type) &&
      search_expression_type->is_same_type(this->default_variable_name.current_type, this->current_operand_type)) {
    XXX_operator(&(this->default_variable_name), " |= ", this->current_operand);
    /* the data type resulting from this operation... */
    this->default_variable_name.current_type = this->current_operand_type;
  }
  else {ERROR;}
  return NULL;
}

void *visit(XOR_operator_c *symbol)	{
  if (search_expression_type->is_binary_type(this->default_variable_name.current_type) &&
      search_expression_type->is_same_type(this->default_variable_name.current_type, this->current_operand_type)) {
    // '^' is a bit by bit exclusive OR !! Also seems to work with boolean types!
    XXX_operator(&(this->default_variable_name), " ^= ", this->current_operand);
    /* the data type resulting from this operation... */
    this->default_variable_name.current_type = this->current_operand_type;
  }
  else {ERROR;}
  return NULL;
}

void *visit(ANDN_operator_c *symbol)	{
  if (search_expression_type->is_binary_type(this->default_variable_name.current_type) &&
      search_expression_type->is_same_type(this->default_variable_name.current_type, this->current_operand_type)) {
    XXX_operator(&(this->default_variable_name),
                 search_expression_type->is_bool_type(this->current_operand_type)?" &= !":" &= ~",
                 this->current_operand);
    /* the data type resulting from this operation... */
    this->default_variable_name.current_type = this->current_operand_type;
  }
  else {ERROR;}
  return NULL;
}

void *visit(ORN_operator_c *symbol)	{
  if (search_expression_type->is_binary_type(this->default_variable_name.current_type) &&
      search_expression_type->is_same_type(this->default_variable_name.current_type, this->current_operand_type)) {
    XXX_operator(&(this->default_variable_name),
                 search_expression_type->is_bool_type(this->current_operand_type)?" |= !":" |= ~",
                 this->current_operand);
    /* the data type resulting from this operation... */
    this->default_variable_name.current_type = this->current_operand_type;
  }
  else {ERROR;}
  return NULL;
}

void *visit(XORN_operator_c *symbol)	{
  if (search_expression_type->is_binary_type(this->default_variable_name.current_type) &&
      search_expression_type->is_same_type(this->default_variable_name.current_type, this->current_operand_type)) {
    XXX_operator(&(this->default_variable_name),
                 // bit by bit exclusive OR !! Also seems to work with boolean types!
                 search_expression_type->is_bool_type(this->current_operand_type)?" ^= !":" ^= ~",
                 this->current_operand);
    /* the data type resulting from this operation... */
    this->default_variable_name.current_type = this->current_operand_type;
  }
  else {ERROR;}
  return NULL;
}

void *visit(ADD_operator_c *symbol)	{
  if (search_expression_type->is_time_type(this->default_variable_name.current_type) &&
      search_expression_type->is_time_type(this->current_operand_type)) {
    XXX_function("__time_add", &(this->default_variable_name), this->current_operand);
    /* the data type resulting from this operation... */
    this->default_variable_name.current_type = this->current_operand_type;
    return NULL;
  }
  if (search_expression_type->is_num_type(this->default_variable_name.current_type) &&
      search_expression_type->is_same_type(this->default_variable_name.current_type, this->current_operand_type)) {
    XXX_operator(&(this->default_variable_name), " += ", this->current_operand);
    /* the data type resulting from this operation... */
    this->default_variable_name.current_type = this->current_operand_type;
    return NULL;
  }
  ERROR;
  return NULL;
}

void *visit(SUB_operator_c *symbol)	{
  if (search_expression_type->is_time_type(this->default_variable_name.current_type) &&
      search_expression_type->is_time_type(this->current_operand_type)) {
    XXX_function("__time_sub", &(this->default_variable_name), this->current_operand);
    /* the data type resulting from this operation... */
    this->default_variable_name.current_type = this->current_operand_type;
    return NULL;
  }
  if (search_expression_type->is_num_type(this->default_variable_name.current_type) &&
      search_expression_type->is_same_type(this->default_variable_name.current_type, this->current_operand_type)) {
    XXX_operator(&(this->default_variable_name), " -= ", this->current_operand);
    /* the data type resulting from this operation... */
    this->default_variable_name.current_type = this->current_operand_type;
    return NULL;
  }
  ERROR;
  return NULL;
}

void *visit(MUL_operator_c *symbol)	{
  if (search_expression_type->is_time_type(this->default_variable_name.current_type) &&
      search_expression_type->is_integer_type(this->current_operand_type)) {
    XXX_function("__time_mul", &(this->default_variable_name), this->current_operand);
    /* the data type resulting from this operation... */
    this->default_variable_name.current_type = this->current_operand_type;
    return NULL;
  }
  if (search_expression_type->is_num_type(this->default_variable_name.current_type) &&
      search_expression_type->is_same_type(this->default_variable_name.current_type, this->current_operand_type)) {
    XXX_operator(&(this->default_variable_name), " *= ", this->current_operand);
    /* the data type resulting from this operation... */
    this->default_variable_name.current_type = this->current_operand_type;
    return NULL;
  }
  ERROR;
  return NULL;
}

void *visit(DIV_operator_c *symbol)	{
  if (search_expression_type->is_num_type(this->default_variable_name.current_type) &&
      search_expression_type->is_same_type(this->default_variable_name.current_type, this->current_operand_type)) {
    XXX_operator(&(this->default_variable_name), " /= ", this->current_operand);
    /* the data type resulting from this operation... */
    this->default_variable_name.current_type = this->current_operand_type;
  }
  else {ERROR;}
  return NULL;
}

void *visit(MOD_operator_c *symbol)	{
  if (search_expression_type->is_num_type(this->default_variable_name.current_type) &&
      search_expression_type->is_same_type(this->default_variable_name.current_type, this->current_operand_type)) {
    XXX_operator(&(this->default_variable_name), " %= ", this->current_operand);
    /* the data type resulting from this operation... */
    this->default_variable_name.current_type = this->current_operand_type;
  }
  else {ERROR;}
  return NULL;
}

void *visit(GT_operator_c *symbol)	{
  return CMP_operator(this->current_operand, "__gt_");
}

void *visit(GE_operator_c *symbol)	{
  return CMP_operator(this->current_operand, "__ge_");
}

void *visit(EQ_operator_c *symbol)	{
  return CMP_operator(this->current_operand, "__eq_");
}

void *visit(LT_operator_c *symbol)	{
  return CMP_operator(this->current_operand, "__lt_");
}

void *visit(LE_operator_c *symbol)	{
  return CMP_operator(this->current_operand, "__le_");
}

void *visit(NE_operator_c *symbol)	{
  return CMP_operator(this->current_operand, "__ne_");
}


//SYM_REF0(CAL_operator_c)
// This method will be called from within the il_fb_call_c visitor method
void *visit(CAL_operator_c *symbol) {return NULL;}

//SYM_REF0(CALC_operator_c)
// This method will be called from within the il_fb_call_c visitor method
void *visit(CALC_operator_c *symbol) {C_modifier(); return NULL;}

//SYM_REF0(CALCN_operator_c)
// This method will be called from within the il_fb_call_c visitor method
void *visit(CALCN_operator_c *symbol) {CN_modifier(); return NULL;}

/* NOTE: The semantics of the RET operator requires us to return a value
 *       if the IL code is inside a function, but simply return no value if
 *       the IL code is inside a function block or program!
 *       Nevertheless, it is the generate_c_c class itself that
 *       introduces the 'reaturn <value>' into the c++ code at the end
 *       of every function. This class does not know whether the IL code
 *       is inside a function or a function block.
 *       We work around this by jumping to the end of the code,
 *       that will be marked by the END_LABEL label in the
 *       instruction_list_c visitor...
 */
// SYM_REF0(RET_operator_c)
void *visit(RET_operator_c *symbol) {
  s4o.print("goto ");s4o.print(END_LABEL);
  return NULL;
}

// SYM_REF0(RETC_operator_c)
void *visit(RETC_operator_c *symbol) {
  C_modifier();
  s4o.print("goto ");s4o.print(END_LABEL);
  return NULL;
}

// SYM_REF0(RETCN_operator_c)
void *visit(RETCN_operator_c *symbol) {
  CN_modifier();
  s4o.print("goto ");s4o.print(END_LABEL);
  return NULL;
}

//SYM_REF0(JMP_operator_c)
void *visit(JMP_operator_c *symbol)	{
  if (NULL == this->jump_label) ERROR;

  s4o.print("goto ");
  this->jump_label->accept(*this);
  /* the data type resulting from this operation is unchanged! */
  return NULL;
}

// SYM_REF0(JMPC_operator_c)
void *visit(JMPC_operator_c *symbol)	{
  if (NULL == this->jump_label) ERROR;

  C_modifier();
  s4o.print("goto ");
  this->jump_label->accept(*this);
  /* the data type resulting from this operation is unchanged! */
  return NULL;
}

// SYM_REF0(JMPCN_operator_c)
void *visit(JMPCN_operator_c *symbol)	{
  if (NULL == this->jump_label) ERROR;

  CN_modifier();
  s4o.print("goto ");
  this->jump_label->accept(*this);
  /* the data type resulting from this operation is unchanged! */
  return NULL;
}

#if 0
/*| [NOT] any_identifier SENDTO */
SYM_REF2(il_assign_out_operator_c, option, variable_name)
#endif

}; /* generate_c_il_c */









/* The implementation of the single visit() member function
 * of il_default_variable_c.
 * It can only come after the full declaration of
 * generate_c_il_c. Since we define and declare
 * generate_c_il_c simultaneously, it can only come
 * after the definition...
 */
void *il_default_variable_c::accept(visitor_c &visitor) {
  /* An ugly hack!! */
  /* This is required because we need to over-ride the base
   * accept(visitor_c &) method of the class symbol_c,
   * so this method may be called through a symbol_c *
   * reference!
   *
   * But, the visitor_c does not include a visitor to
   * an il_default_variable_c, which means that we couldn't
   * simply call visitor.visit(this);
   *
   * We therefore need to use the dynamic_cast hack!!
   *
   * Note too that we can't cast a visitor_c to a
   * il_default_variable_visitor_c, since they are not related.
   * Nor may the il_default_variable_visitor_c inherit from
   * visitor_c, because then generate_c_il_c would contain
   * two visitor_c base classes, one each through
   * il_default_variable_visitor_c and generate_c_type_c
   *
   * We could use virtual inheritance of the visitor_c, but it
   * would probably create more problems than it is worth!
   */
  generate_c_il_c *v;
  v = dynamic_cast<generate_c_il_c *>(&visitor);
  if (v == NULL) ERROR;

  return v->visit(this);
}




il_default_variable_c::il_default_variable_c(const char *var_name_str, symbol_c *current_type) {
  if (NULL == var_name_str) ERROR;
  /* Note: current_type may start off with NULL */

  this->var_name = new identifier_c(var_name_str);
  if (NULL == this->var_name) ERROR;

  this->current_type = current_type;
}