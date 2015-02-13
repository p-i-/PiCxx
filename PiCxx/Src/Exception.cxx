
#include "Objects.hxx"

namespace Py 
{
    void Exception::set_or_modify_python_error_indicator() const
    {
        COUT( "Throwing exception, reason{" << m_message << "}, trace{" << m_trace << "}" );

        Object ob_errtype, ob_message, ob_trace;

        PyObject* exception_type = PyErr_Occurred();

        if( exception_type == nullptr )
        {
            COUT( "Python Error-Indicator wasn't set, setting..." );

            ob_errtype  = Object{ charge(PyExc_RuntimeError) };
            ob_message  = Object{ m_message };
            ob_trace    = Object{ m_trace };
        }
        else
        {
            COUT( "Tagging onto existing PyError" );

            PyObject *p_errtype, *p_reason, *p_trace;
            PyErr_Fetch( &p_errtype, &p_reason, &p_trace ); // PyErr_Fetch charges

            // tag our own message and trace onto the existing exception.
            // note that it is possible the existing exception is missing either or both of these.
            ob_errtype  =                                                                 Object{ p_errtype };
            ob_message  = Object{" PiCxx reason{ " + m_message + "},  Python reason: "} + Object{ p_reason };
            ob_trace    = Object{" PiCxx trace{ "  + m_trace   + "},  Python trace: " } + Object{ p_trace };
        }

        PyErr_Restore( charge(*ob_errtype), charge(*ob_message), charge(*ob_trace) ); // PyErr_Restore eats charge
    }

} // Py
