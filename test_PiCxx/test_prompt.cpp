#include "Base.hxx"

// http://bugs.python.org/issue18395
//
#if PY_MAJOR_VERSION == 2
#   define PY_CHAR char
#else
#   define PY_CHAR wchar_t
#endif

extern "C" PyObject* PyInit_test_funcmapper();
void test_funcmapper();

using namespace Py;

extern "C"  int Py_Main(int argc, PY_CHAR** argv); // <-- conflicting types for 'Py_Main'

// produces a Python prompt in the debug window
void test_prompt( int argc, const char* argv[] )
{
    PyImport_AppendInittab( "test_funcmapper", &PyInit_test_funcmapper );
    Py_Initialize();

    PyRun_SimpleString( "print('hello world') \n" );
    
    Py_Main(argc, (PY_CHAR**)argv); // wrong but works(!)

    Py_Finalize();
}
/*
e.g.

 >>> import sys
 >>> sys.modules.keys() <-- FINDS 'test_funcmapper'!
 
 >>> import test_funcmapper
 >>> test_funcmapper.func()
   
   Old-Style Handler #2
   Invoking: func
      'func' invoked with Args:(), Keywords:{}
 */
