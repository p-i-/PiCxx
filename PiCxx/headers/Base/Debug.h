#pragma once

#if PICXX_DEBUG==1
#   define IF_DEBUG( x )        x
#else
#   define IF_DEBUG( x )
#endif

#define COUT( x )               IF_DEBUG( std::cout << "   " << x << std::endl )

#define COUT_0(f)               COUT( "   '" << f << "' invoked with no Args or Keywords" )
#define COUT_A(f,a)             COUT( "   '" << f << "' invoked with Args:" << a )
#define COUT_AK(f,a,k)          COUT( "   '" << f << "' invoked with Args:" << a << ", Keywords:" << k )

// cout and execute
#define XCOUT( x )              COUT( "\n   EXEC: " << #x << std::endl << (x) )

#define ADDR( obj )             "0x" << std::hex << (uintptr_t)obj << std::dec

// notice Object overloads * operator to return the associated PyObject*
#define COUT_DATA(x)            COUT( #x " = " << x << " {PyObject " << ADDR(*x) << ", refcount " << (*x)->ob_refcnt )

#define TYPEOF(x)               std::string( x.ptr()->ob_type->tp_name )
