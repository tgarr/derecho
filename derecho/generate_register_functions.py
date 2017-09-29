
import argparse

OUTPUT_FILENAME = "register_rpc_functions.h"

### Boilerplate Code Templates ###
pragma_once = '#pragma once\n'
includes = ('#include <tuple>\n')
register_functions_begin = '#define REGISTER_RPC_FUNCTIONS{count}(classname, {args_list}) \\\n'
register_functions_declaration = '    static auto register_functions() {\\\n'
register_functions_one = '        return std::make_tuple(derecho::rpc::tag<CT_STRING(a)::hash()>(& classname :: a));\\\n'
register_functions_first_line = '        return std::make_tuple(derecho::rpc::tag<CT_STRING(a)::hash()>(& classname :: a),\\\n'
register_functions_mid_line = '                               derecho::rpc::tag<CT_STRING({method})::hash()>(& classname :: {method}),\\\n'
register_functions_last_line = '                               derecho::rpc::tag<CT_STRING({method})::hash()>(& classname :: {method}));\\\n'
register_functions_end = '    } \n'

### Comment block that goes at the top of the file ###
header_comments = """
/**
 * This is an automatically-generated file that makes it easier for user-created
 * Replicated Objects to register their RPC methods with Derecho by defining some
 * macros. Do not edit this file by hand; you should generate it with 
 * generate_register_functions.py The public interface is at the bottom of the 
 * file.
 */

"""

### Complete snippet of code that goes at the end of the file ###
file_footer = r"""
#define REGISTER_RPC_FUNCTIONS_IMPL2(count, ...) REGISTER_RPC_FUNCTIONS ## count (__VA_ARGS__)
#define REGISTER_RPC_FUNCTIONS_IMPL(count, ...) REGISTER_RPC_FUNCTIONS_IMPL2(count, __VA_ARGS__)

/**
 * This macro automatically generates a register_functions() method for a Derecho
 * Replicated Object, given the name of the class and the names of each method
 * that should be RPC-callable. For example, if you have a class Thing with
 * methods foo() and bar(), put this inside your class definition (in the public
 * section):
 *
 * REGISTER_RPC_FUNCTIONS(Thing, foo, bar);
 */
#define REGISTER_RPC_FUNCTIONS(...) REGISTER_RPC_FUNCTIONS_IMPL(VA_NARGS(__VA_ARGS__), __VA_ARGS__)

/**
 * This macro generates the Derecho-registered name of an RPC function, for use
 * in the template parameter of ordered_send (and other RPC callers), given the
 * name of the corresponding Replicated Object method. For example, if you have
 * a Replicated<Thing> reference named thing_handle, call its registered RPC
 * method foo() like this:
 *
 * thing_handle.ordered_send<RPC_NAME(foo)>(foo_args);
 */
#define RPC_NAME(...) CT_STRING(__VA_ARGS__)::hash()
"""

argparser = argparse.ArgumentParser(description='Generate ' + OUTPUT_FILENAME + \
        ' with support for the specified number of methods.')
argparser.add_argument('num_methods', metavar='N', type=int, help='The maximum number '
        'of RPC-callable methods that the RPC registration macro should support (the ' 
        'larger the number, the more macros will be generated)')
args = argparser.parse_args()

with open(OUTPUT_FILENAME, 'w') as output:
    output.write(pragma_once) 
    output.write(includes)
    output.write(header_comments)
    for curr_num_methods in range(1, args.num_methods + 1):
        method_vars = [chr(i) for i in range(ord('a'), ord('a')+curr_num_methods)]
        output.write(register_functions_begin.format(count=curr_num_methods+1,
            args_list=', '.join(method_vars)))
        output.write(register_functions_declaration)
        if curr_num_methods > 1:
            output.write(register_functions_first_line)
            for method_num in range(1, curr_num_methods - 1):
                output.write(register_functions_mid_line.format(method=method_vars[method_num]))
            output.write(register_functions_last_line.format(method=method_vars[-1]))
        else:
            output.write(register_functions_one)
        output.write(register_functions_end)
    output.write(file_footer)
