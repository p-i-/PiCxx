#pragma once

#include "ExtObj.hxx"


/*
 NOTES:
    (saved excerpts from previous comments, so this won't read coherently as a whole.
    However, I'm keeping any chunk that may provide some insight)

    So the programmer wants to create their own (Python extension) module MyModule, which will derive from this class.
    
        class MyModule : ExtModule<MyModule> { ... }    // Note MyModule gets passed in via CRTP)

    And let us say they create a MyModule::myFunc_cxx() so that from the Python runtime one can run it by typing

        import MyModule
        x = MyModule.myFunc()

    When the Python runtime encounters 'MyModule.myFunc()', it searches MyModule's PyMethodDef table for 'myFunc()',
    retrieving the associated function-pointer, and running that function.

    Note that the Python runtime is C code, and hence can't call
    myModule::myFunc_cxx() i.e. a method on an instance of a C++ class.
    Because it doesn't have a "C-style function pointer"
    (note that a static myModule::someStaticFunc() DOES have a "C-style function pointer"
    hence Python CAN call it)

    However, if within our .cpp we declare
    extern "C" void foo() { ... }

    ...then the Python runtime CAN run foo().

    So the trick is: we then have to get foo() to call myModule::myFunc_cxx()
    http://stackoverflow.com/questions/1588788/wrapping-c-class-api-for-c-consumption
    ^ that's how to do it

    Also, we need to put a try/catch block around it in case a C++ exception gets thrown during execution
    Then we can gracefully return a "C++land error occurred" value back to the Python runtime.

    - - -

    Note that the method map table is part of a Python object.
    The python runtime may decide to execute a function from this module,
    In which case it will find the function pointer in the method map table, and run it.

    - - -

    We have to call through to the appropriate ExtModuleBase::ExtModule::MyModule::someFunc()

    The cunning thing here is that we have no idea what particular extension module is responsible for this function
    However, when we moduleBase->invoke_method_noargs(...) it will find the correct override
    Therefore inside the invoke_method_noargs(...) function, clearly we will HAVE ACCESS to the MyModule type

    http://stackoverflow.com/questions/26745234/error-explicit-specialization-of-foo-in-class-scope

 TODO: merge the latter half of this doc into FuncMapper, as it refers to code that has since been moved there

Links:
    http://stackoverflow.com/questions/26947037/design-pattern-for-static-base-constructor-that-calls-static-method-in-final-cla
*/

namespace Py
{
    template<typename Final>
    class ExtModule : public FuncMapper<Final>
    {
    private:
        // from nonstatic methods we could have also used this->methods()
        // but that would be somewhat misleading as methods() is static
        static typename FuncMapper<Final>::method_map_t& method_map() {
            return FuncMapper<Final>::methods();
        }

    protected:
        const std::string   m_name;
        const std::string   m_doc;

        const std::string   m_full_module_name;

        PyModuleDef         m_module_def;
        PyObject*           m_module;

    public:
        //virtual ~ExtModule() { };

        // called by the consumer ONCE per 'Py_Initialize ... Py_Finalize' session
        // MARKER_STARTUP__1.2a module_test_funcmapper::reset()
        static const Object reset()
        {
            COUT( "ExtModule::start_up()" );

            static Final* single_inst = nullptr;

            if(single_inst) delete(single_inst);
            // ^ this allows us to initialise the module multiple times

            single_inst = new Final; // jumpnext
            // ^ invokes Final constructor, in which we will provide the consumer
            //   with a callback in which they must add all methods

            return single_inst->module();
        }

        /*
            MARKER_STARTUP___1.2c module base c'tor
            
            The consumer (Final class) has to supply a static 'register_methods_and_classes' 
            method (inspect 01_e)

            >>> import myModule                 # invokes this code
            
            >>> myModule.moduleFunc()           # ... now this will succeed, assuming moduleFunc() was registered
            >>> c = myModule.someExtnClass()    # ... and this likewise!
         */
        ExtModule(const std::string& name, const std::string& doc )
            : m_name{name}
            , m_doc{doc}
            , m_full_module_name{  _Py_PackageContext ? _Py_PackageContext : name  }
        {
            COUT( "ExtModule()" );

            // clear and (re)populate method-map
            
            method_map().clear();

            // Consumer should implement a static method with this name.
            // Note: We can't invoke Final *instance* methods from base constructor
            //  as final object is not constructed yet. But that's okay, logically
            //  it should be static anyway.
            // MARKER_STARTUP___2a call register_methods_and_classes()
            Final::register_methods_and_classes();

            // Load all registered methods into module's dictionary.

            //  - First create the module.
            {
                m_module_def = PyModuleDef{}; // set all to 0
                
                m_module_def.m_name     = (char*)m_name.c_str();
                m_module_def.m_doc      = (char*)m_doc.c_str();

                m_module = PyModule_Create( &m_module_def ); // PyState_RemoveModule later?
                COUT( "ExtModule():" << ADDR(m_module) );
            }

            //  - A Python extension module has a PyDict that stores all of its methods.
            // Note that we are grabbing the module dictionary itself, not making a copy.
            Object dict = moduleDictionary();

            //  - Here we populate this PyDict from our method-map-table for this particular extension module
            for( const auto& i : method_map() ) {
                COUT( "    Importing method: " << i.first);
                dict[ i.first ] = i.second->ConstructPyFunc(this);
            }
        }

        // Both only valid after initialize() has been called
        Object module          () const { return Object{ charge(m_module) }; }
        Object moduleDictionary() const { return Object{ charge(PyModule_GetDict(m_module)) }; }  // PyModule_GetDict returns borrowed reference

    private:
        // prevent the compiler generating these unwanted functions
        ExtModule      ( const ExtModule<Final> & ) = delete;
        void operator= ( const ExtModule<Final> & ) = delete;
    };
} // Namespace Py

