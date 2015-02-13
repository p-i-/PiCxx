#ifdef _MSC_VER
// disable warning C4786: symbol greater than 255 character,
// nessesary to ignore as <map> causes lots of warning
#pragma warning(disable: 4786)
#endif

#include "ExtModule.hxx"

#include <assert.h>

// = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =

using namespace Py;

#define Tuple Object
#define Dict Object
#define List Object
#define String Object
#define Bytes Object
#define Callable Object


class new_style_class: public NewStyle< new_style_class >
{
private:
    String m_value;

public:
    // the user should not create an instance directly from C++
    // they must instead create an instance through the Python runtime, which will trigger this constructor (look in the extension-object-new file)
    new_style_class( Bridge* self, const Tuple& args, const Dict& kwds )
        : NewStyle< new_style_class >::NewStyle( self, args, kwds )
    , m_value( "default value" )
    {
        COUT_AK("new_style_class", args, kwds);
    }


    virtual ~new_style_class()
    {
        COUT_0( "~new_style_class" );
    }


    static void setup()
    {
        COUT( "new_style_class::setup()" );
        
        typeobject().setName( "new_style_class" );
        typeobject().setDoc( "documentation for new_style_class class" );
        typeobject().supportGetattro();
        typeobject().supportSetattro();

        register_method< & new_style_class::f0_noargs     >( "func_noargs"    /*, "docs for func_noargs"*/    ) ;
        register_method< & new_style_class::f1_varargs    >( "func_varargs"   , "docs for func_varargs"   ) ;
        register_method< & new_style_class::f2_keyword    >( "func_keyword"   , "docs for func_keyword"   ) ;
        register_method< & new_style_class::f0_exception  >( "func_exception" , "docs for func_exception" ) ;
    }


    Object f0_noargs( void )
    {
        COUT_0( "f0_noargs" );
        COUT( "value ref count " << m_value.reference_count()  );
        return None();
    }

    
    Object f1_varargs( const Tuple& a )
    {
        COUT_A( "f1_varargs", a );
        return None();
    }

    
    Object f2_keyword( const Tuple& a, const Dict& k )
    {
        COUT_AK( "f2_keyword", a, k );
        return None();
    }

    
    Object f0_exception( void )
    {
        COUT_0( "f0_exception" );
        THROW( "f0_exception::RuntimeError!!!" );
        return None();
    }

    
    Object getattro( const String name ) override
    {
        std::string sname{ name.dump_utf8string() };

        if( sname == "value" )
            return m_value;
        else
            return NewStyle<new_style_class>::getattro( name ); // genericGetAttro( name );
    }


    int setattro( const String name, const Object value ) override
    {
        std::string sname{ name.dump_utf8string() };

        if( sname == "value" ) {
            m_value = value;
            return 0;
        }
        else
            return genericSetAttro( name, value );
    }
};

// = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =

class old_style_class: public OldStyle< old_style_class >
{
public:
    old_style_class()
    { }

    virtual ~old_style_class()
    { }

    static void setup()
    {
        typeobject().setName( "old_style_class" );
        typeobject().setDoc( "documentation for old_style_class class" );
        typeobject().supportGetattr();

        register_method( "func_noargs" , & old_style_class::f0_noargs  );
        register_method( "func_varargs", & old_style_class::f1_varargs );
        register_method( "func_keyword", & old_style_class::f2_keyword );
    }

    Object f0_noargs( void )
    {
        COUT_0( "f0_noargs" );
        return None();
    }

    Object f1_varargs( const Tuple& a )
    {
        COUT_A( "f1_varargs", a );
        return None();
    }

    Object f2_keyword( const Tuple& a, const Dict& k )
    {
        COUT_AK( "f2_keyword", a, k );
        return None();
    }
};

// = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =

/*
 The user should NOT create a module object directly
 This Final class gets created within the base class in a static start_up method
 It isn't exactly a singleton pattern, as start_up can be called multiple times,
 Each time will delete the module object and create a new one.
 */

class module_test_funcmapper : public ExtModule<module_test_funcmapper>
{
public:
    module_test_funcmapper() : ExtModule<module_test_funcmapper>::ExtModule{ "test_funcmapper", "doc for test_funcmapper" }
    {
        /*
          MARKER_STARTUP___1.2b module final c'tor
            
            Final constructor invokes base constructor before executing its own body
            geordi: -w {D d;}  struct B{B(){BARK;}}; struct D:B{D(){BARK;}};  // B::B() D::D()

            jumpnext
            
            
            
            , which will invoke
            a static 'register_methods_and_classes' method that the consumer has to supply (see below)
            and use the collected data to create a Python module object and register it with the Python runtime.

            In so doing, it will be providing custom functionality for this object,
            Specifically overriding its getattro.
            So it is possible to, from Python, invoke our C++ module & object methods
            
             >>> myModule.myModuleMethod()

            'myModuleMethod' was added to the module's dictionary during start-up.
            Python does a lookup and executes it.

             >>> from myModule import myObj
             >>> myObj.myMethod()
             
            Python sees that we are attempting to invoke a 'myMethod' attribute on myObj
            First it will look through myObj's PyTypeObject's m_table->tp_methods
            If myObj is a new-style class, it will find the appropriate method in this table

            However, if myObj is an OLD-style class then this will be empty, as old-style class
            uses a different mechanism.
            
            Why? No particularly good reason ... It demonstrates an alternative solution path.
            It's slightly more lightweight in that the same handler can be reused for everything
            (as opposed to the new style class which requires a separate handler for each method)
            
            TODO: Take this out at some point in the future as it adds unnecessary complexity

            Ok, here is how it works:

            After the Python runtime has failed the tp_methods lookup,
            It will invoke myObj's PyTypeObject's tp_getattro function pointer
            Which we have intercepted with a trampoline that bounces us to ExtObjBase::getattro, 
            For which ExtModule has provided an override
            This override will attempt to locate and return to Python the PyFunction object associated with 'myMethod'
            Python will then invoke this PyFunction object
            Which will get caught by the appropriate handler in FuncMapper
            Which will trampoline to the consumer's C++ method

            Awesomely complicated huh? This is from the original PyCXX.
            */


        // after initialize the moduleDictionary will exist
        //initialize( "documentation for the test_funcmapper module" );

        Dict d{ moduleDictionary() };

        String s42{ "s42" };
        d["meaning_of_life"] = s42;

        COUT( "meaning_of_life: " << d["meaning_of_life"] << std::endl
                << d << std::endl << "- - - - - - - " );

        // Add new_style_class a different way, for the sake of demonstration
        Object x = new_style_class::type();
        d["new_style_class"] = x;

        throw_if_pyerr(TRACE);
    }

    ~module_test_funcmapper()
    {
        COUT("~module_test_funcmapper()");
    }

public:
    // MARKER_STARTUP___2b register_methods_and_classes() consumer def'n
    static void register_methods_and_classes()
    {
        register_method("old_style_class", &module_test_funcmapper::factory_old_style_class,  "documentation for old_style_class()");
        register_method("func"           , &module_test_funcmapper::func,                     "documentation for func()");
        
        // MARKER_STARTUP___3 one_time_setup() on each extention class
        // For every custom PythonType extension object, invoke its one-time setup
        // which creates a new PyTypeObject and registers it with the Python runtime.
        old_style_class::one_time_setup();
        new_style_class::one_time_setup();
        // ^ foo:one_time_setup() requires foo to implement a static 'foo::setup()'
        //    
    }

//public:
//    virtual ~module_test_funcmapper()
//    { }

private:

    Object func( const Tuple& a, const Dict& k )
    {
        COUT_AK( "func", a, k );
        return None();
    }

//    Object make_instance( const Tuple& a, const Dict& k )
//    {
//        COUT_AK( "make_instance", a, k );
//
//        Callable class_type{ new_style_class::type() };
//
//        //ExtensionObject_new<new_style_class> new_style_obj{ class_type(a,k) };
//
//        Object new_style_obj{ class_type(a,k) };
//        return new_style_obj;
//    }

    Object factory_old_style_class( const Tuple& a )
    {
        COUT_A( "factory_old_style_class", a );
        Object obj = new old_style_class;
        return obj;
    }
};

// = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =

/*
  MARKER_STARTUP___1.1 PyInit_test_funcmapper()
    If you are writing an extension for Python, this code will be compiled
    into a library and placed in some folder Python can see. When the Python
    Runtime initialises, it will search every visible library for any function
    of the form:
    
        PyInit_xxx

    adding it to it's init-table (using PyImport_AppendInittab)

    Then whenever it encounters "import xxx" it will look up xxx
    and execute the associated function.

    In my demos, I am *embedding* Python in C++ code as opposed to
    writing a C++ extension, so I must do PyImport_AppendInittab manually.

    By keeping the PyInit_xxx form, I can in the future compile my test code 
    into an extension library without modification, and demonstrate
    extensions. I haven't done this yet!
    */

#if defined( _WIN32 )
#   define EXPORT_SYMBOL __declspec( dllexport )
#else
#   define EXPORT_SYMBOL
#endif

extern "C" EXPORT_SYMBOL PyObject* PyInit_test_funcmapper()
{
    /*
        This static reset() method:
            - creates a new instance of our module object,
            - fires the static register_methods_and_classes() method, which our module object MUST implement,
            - creates a module PyObject, returning a wrapped pointer (which we unwrap with the overloaded * operator)
     */
    return *module_test_funcmapper::reset(); // jumpnext
}

// - - - - - - - - - - - - - - - - - - - - - - - - -

extern void run_file( const char* );

void test_funcmapper()
{
    // This makes a new entry in Python's "module init table"
    // So that when Python encounters "import test_funcmapper" it invokes PyInit_test_funcmapper
    // NOTE: we must do this BEFORE Py_Initialize
    // NOTE: we only do this ONCE, even if multiple Py_Initialize/Py_Finalize pairs follow
    PyImport_AppendInittab( "test_funcmapper", &PyInit_test_funcmapper );

    // Allows us to test memory leaks by setting e.g. 1000
    // Note there seems to be an intermittent bug that comes up in Py_Finalize once in a blue moon.
    const int NUM_TRIES = 1;

    for( int i=0; i < NUM_TRIES; i++ )
    {
        Py_Initialize();

        // In Xcode -> {project at root of Project Explorer}

        // ... Ensure top left pulldown is Targets -> test_PiCxx
        // ... -> build phases -> copy files:
        //      - specify /py/ for folder
        //      - destination: Resources
        //      - copy: UNCHECK "only when installing"
        //      - add relevant .py (in this case just test_funcmapper.py)
        // 
        // "import test_funcmapper" in this file calls PyInit_test_funcmapper
        Py::run_file( "./py/test_funcmapper.py" );

        Py_Finalize();
    }
}
