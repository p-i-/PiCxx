#pragma once

/*
Python Errors:
    Say we call some Python API function which causes a Python error
    Python Will set its error indicator, and return -1/nullptr to indicate failure
        
    Querying the error indicator with PyErr_Occurred() will return non-NULL 
    if indeed an error occurred (just in case -1/nullptr was a valid return value)

    If an error DID occur we can inspect it:

        PyObject *errtype, *value, *traceback;
        PyErr_Fetch( &errtype, &value, &traceback );
        :
        PyErr_Restore( errtype, value, traceback );
    
    Note that we could even modify it, or use PyErr_Restore on its own to SET the indicator
    

We have 4 different exception scenarios:
    Inside trampoline:
        Cxx error       - set Python error indicator and throw
        Python error    - modify existing indicator and throw

    Outside trampoline:
        Cxx error       - don't need to implement anything generic
        Python error    - maybe provide a function for checking if there is an error, printing it, resetting it

    'Inside trampoline' means that we have filled some slot with a function pointer,
    and Python has executed this function pointer.

    If we are not inside a trampoline, it must be part of the init-sequence, 
    which should maybe have try/catch around it (TODO?).

    (Python error meaning that our Cxx code calls to Python Runtime and an error occurs there.
    We should always inspect the return type as per above.
    We should also check for PyErr_Occurred() if there is any possibility of missing an error).
    NOTE: if you want to check whether an error occurred on a particular line, be sure to
    call PyErr_Occurred() before and after. If the first call is coming back non-NULL
    this means you have some prior error.

In a trampoline:
    Every trampoline has an encompassing try catch block
    If we are inside the trampoline, 
        1. we try calling some Python API function which causes a Python error
            in which case we MODIFY the Python error indicator and throw an Exception
        
        2a. our own C++ code causes an error that we trap
            in which case we SET the Python error indicator and throw an Exception
        
        2b. our C++ code indirectly causes an error
            Which we should handled the same way as 2a
    
    The trampoline will first catch Exception objects
    Then it will have a secondary catch for 2b, in which it will set the Python error indicator
    
    Any catch means our trampoline returns failure (-1/nullptr)

    NOTE: our trampoline should also check for:
                PyErr_Occurred()
            ...in case some Python error happened that we didn't correctly trap.

Otherwise:
    If we are not in a trampoline, it will be up to US to put try/catch blocks around our code
    So we can handle 1,2a,2b (i.e. the three scenarios in which an error may occur) however we choose.
    
    We set/modify the Python error indicator with:
        PyObject *errtype, *value, *traceback;
        PyErr_Fetch( &errtype, &value, &traceback );
        :
        PyErr_Restore( errtype, value, traceback );


Example:
    So, for example, if our library has an add function, but the consumer tries to add 1 + "2", Python's
    PyNumber_Add will return nullptr, PyErr_Occurred() would return true, and PyErr_Print() would print

        "TypeError: unsupported operand type(s) for +: 'float' and 'bytes'"

    So πcxx checks for 'return of nullptr and PyErr_Occurred()' and throws:
        Exception{ "πcxx do_op error:" }
        
    Construction of that object modifies the Python error indicator to stick "πcxx do_op error:" in front of the trace.

    If we are in a trampoline, the trampoline's catch will catch it, and return -1/nullptr back to Python
    which would then be able to make use of that error message.
    
    Otherwise we should supply our try/catch.
 */


#include <string>
#include <iostream>

void _set_or_modify_python_error_indicator(const std::string& in_trace, const std::string& in_reason );

namespace Py
{
    class Exception
    {
    private:
        const std::string m_trace;
        const std::string m_message;
    public:
        Exception(const std::string& trace, const std::string& message = "")
        : m_trace{trace}, m_message{ "PiCxx Exception:"+message }
        { }

        void set_or_modify_python_error_indicator()  const;
    };

    #define TRACE \
                    "Trace line:" + std::to_string(__LINE__) \
                  + ", func:"     + __func__ \
                  + ", file:"     + __FILE__

    #define THROW( message )    throw Exception{ TRACE, message }


    inline void throw_if_pyerr(const std::string& trace, const std::string& message = "")
    {
        PyObject* exception_type = PyErr_Occurred();
        if( exception_type != nullptr ) {
            COUT( "throw_if_pyerr: PyErr_Occurred [" << message << "] ... " );
            IF_DEBUG( PyErr_Print(); )
            throw Exception{ trace, message };
        }
    }
    inline bool is_errorcode( int x )       { return x==-1; }
    inline bool is_errorcode( PyObject* x ) { return x==nullptr; }
    #define ENSURE_OK( cond ) \
        if( is_errorcode(cond) ) \
            throw_if_pyerr( TRACE, #cond )

    /*
       Links:
        http://stackoverflow.com/questions/27143297/invoke-final-class-constructor-from-base-class
     */
}
