#pragma once

/*
 Overview:
     TypeObject wraps a PyTypeObject, placing a trampoline function at every 
     enabled slot on its function-pointer tables, which will bounce to a matching
     virtual instance-method of the C++ ExtObjBase base-class.

     (Python passes a pointer to the active PyObject as the first parameter,
     and we make sure each of these PyObject-s contains extra space for a pointer 
     that will point to the corresponding ExtObjBase instance.  
     Look at Bridge for details.)

     The consumer derives their custom C++ object indirectly from this base class,
     providing an appropriate override for each enabled slot.

     Hence C++ naturally bounces from base up to the override.
     
     Pseudocode:
        class Base {
            virtual PyObject* slotx(...) { cout << "override me" }
            :
        }

        PyObject* trampoline_slotx(PyObject* pyob, ...) {
            Base* baseptr = baseptr_from_pyob(pyob);
            return baseptr->slotx(...);
        }
        my_typeobj->m_table->slotx = trampoline_slotx; 
        
        RegisterWithPython(my_typeobj);  // Python now has access to this m_table and may invoke the slot

        // consumer provides this:
        class MyFinal : Base {
            PyObject* slotx(...) { do stuff with ...; return something; }
            :
        }

     Each trampoline additionally has a try/catch so that if an error occurs en-bounce,
     We ensure the python error indicator is set, and return an error-value 
     (-1 or nullptr depending on return type) back to Python runtime.

 Detail:
     To create a new Python Type we must setup a PyTypeObject and feed (register) it into the Python runtime.
     
     This contains things like the name (tp_name) and how many bytes to allocate for our object (tp_basicsize)
     Mostly it contains SLOTS -- the consumer is expected to put a function pointer in each slot they want to enable.
     
     So if they want their type to support addition, they should create a my_add function with
     appropriate signature, and place a pointer to this function in typeob's tp_as_number->nb_add slot;
     Any of the following methods will accomplish this:
     
        // METHOD 1
        PyObject* my_add(PyObject*,PyObject*) { ... }
        typeob->tp_as_number->nb_add = my_add;

        // METHOD 2: Note that it is possible to assign a captureless lambda to an 'extern C' function pointer:
        typeob->tp_as_number->nb_add = [] (PyObject*, PyObject*) -> PyObject* { ... };

        // METHOD 3: Note also that a static class method has a 'C-style' signature:
        struct Foo {
            static PyObject* my_add(PyObject*, PyObject*) { ... }
        }
        typeob->tp_as_number->nb_add = &Foo::my_add;

     What we do is provide a Base class with a corresponding virtual method for each slot.
     The consumer may derive a Derived from this and override which ever slots they wish to implement.
     
     Then it remains for us to insert a trampoline function into each slot that will:
       * figure out the Base instance of the Final instance that corresponds to the PyObject whose slot got triggered
          (^ a pointer to this PyObject comes in as the first parameter)

       * invoke the corresponding Base::foo_slot virtual member-function passing through the remaining parameters
          Note that PyObject* p gets forwarded as 'Object{charge(p)}'
          We have to charge as Python (unless documented otherwise) feeds us neutral objects.
          (Similarly we must (again, unless documented otherwise) charge any PyObject* we return to Python)
          Also we convert char* and const char* to std::string
          It appears that Python had some early inconsistencies here which remain because "too late to fix now"
        
       * this will invoke Derived::foo_slot override if it exists.
          If one isn't provided, the base implementation will throw a warning

       * we should try/catch around the call, in case there is some error in the consumer's code.

     It would be a lot of ugly duplication if we had to manually write each trampoline.
     Fortunately we can automate the construction of a trampoline, using the above method 3.
     We create a templated class where the template parameters break apart the signatures for
     typeob->foo_slot and Base::foo_slot, deducing the return types, argument lists, and Base member-func.
     This class contains a static call method.

        http://stackoverflow.com/questions/26934036/coding-static-to-instance-method-trampoline-function-with-templates
        http://stackoverflow.com/questions/26941960/remove-duplication-in-c11-try-catch-pattern
        // ...
        http://stackoverflow.com/questions/27821106/using-lambda-template-sfinae-to-automate-try-catch-safeguarding-of-trampoline-fu
        http://stackoverflow.com/questions/27826117/design-pattern-for-exception-safe-trampolines
        http://stackoverflow.com/questions/27842709/perfect-forward-non-t-arguments-while-converting-t-s
        http://stackoverflow.com/questions/27866483/c-func-to-c-instance-member-trampoline-with-non-bijective-argument-type-mappin
        http://stackoverflow.com/questions/27869997/selective-forwarding-function

 Note on errors:
     There are two execution cases: 
        (1) C++ with inner Py-Runtime
        (2) Python running a C++-lib

     In both cases we want to be setting Python's error indicator before returning control back to Python.
*/

#include <typeindex>

namespace Py
{
    /*
     for Debug, so that when Python runtime fires a slot we can log which slot got hit
     http://stackoverflow.com/questions/27913011/generate-unique-identifier-that-can-distinguish-idfooa-from-idfoob
     http://stackoverflow.com/questions/27908849/how-to-pass-a-macro-generated-foo-string-into-a-templated-class
    */
    #define UniqueID(G)  std::type_index(typeid(G))
    static std::map<std::type_index, std::string> names_map;

#pragma mark  Helpers

    template <typename T>
    struct Convert {
        static T to_cxx(T t){ return t; }
        static T to_c  (T t){ return t; }
    };
    template<>  struct Convert<PyObject*> {
        static Object to_cxx(PyObject* p) { return Object{charge(p)}; }  // Python sends us neutral refs
    };
    template<>  struct Convert<Object> {
        static PyObject* to_c(Object ob) { return charge(*ob); } // ... and expects we send back a charged ref
    };

    // - - -
    // http://stackoverflow.com/questions/27621784/c-how-to-make-templatetf-return-1-for-integral-t-nullptr-for-pointer-typ
    template <typename T>
    struct Error {
        static T value(){ return T(-1); }
    };
    template<>  struct Error<PyObject*> {
        static PyObject* value(){ return (PyObject*)nullptr; }
    };

#pragma mark  Trampoline Generator

    template<typename Fc, typename Target, Target target>
    struct Generate;

    template <  typename R      , typename ...Arg       ,
                typename RTarg  , typename ...TargArg   ,
                                               RTarg(ExtObjBase::*target)(TargArg...) >
    struct Generate< R(*)(PyObject*, Arg...),  RTarg(ExtObjBase::*      )(TargArg...),  target >
    {
        static R call( PyObject* self, Arg... carg)
        {
            COUT( "\n   PyObject&:" << ADDR(self) << " SLOT:" <<  names_map[ UniqueID(Generate) ] );
            try
            {
                RTarg r_cxx = (cxxbase_for(self)->*target) (Convert<Arg>::to_cxx(carg) ...);
                return Convert<RTarg>::to_c(r_cxx);
            }
            catch ( const Exception& e )
            {
                COUT ("CAUGHT exception in Generate::call");
                e.set_or_modify_python_error_indicator();
                return Error<R>::value();
            }
            catch (...)
            {
                COUT ("Unknown exception in Generate::call");
                Exception e{ TRACE, "Unknown exception in Generate::call" };
                e.set_or_modify_python_error_indicator();
                return Error<R>::value();
            }
        }
    };

#define BIND(c_slot, cxx_target) \
    { \
        using G = Generate< decltype(c_slot), decltype(&cxx_target), &cxx_target >; \
        c_slot = & G::call; \
        names_map[ UniqueID(G) ] = std::string(#c_slot); \
    }

// = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =

#pragma mark TypeObject

    class TypeObject
    {
    private:
        // MARKER_STARTUP__3.2c Underlying PyTypeObject
        /*
            This is the key. This whole class is built around wrapping the m_table object.
         
            Every PyObject contains a pointer to its associated PyTypeObject
            which is a table of function pointers.  Python Runtime calls these
            functions, e.g. If it requires the NAME of this Type, it will call
            m_table->tp_name.  It is up to us to provide functions for our custom type.
         */
        PyTypeObject*           m_table;

        // these tables fit into the main m_table via pointers
        PySequenceMethods*      sequence_table;
        PyMappingMethods*       mapping_table;
        PyNumberMethods*        number_table;
        PyBufferProcs*          buffer_table;

        std::string             m_name;
        std::string             m_doc;
    public:
        PyTypeObject* table() const
        {
            return m_table;
        }

        // NOTE: if you define one sequence method you must define all of them except the assigns
        
        TypeObject( const std::string& default_name, size_t size_bytes )
            : m_table{ new PyTypeObject{} }  // {} zeros memory
            , sequence_table{}
            , mapping_table{}
            , number_table{}
            , buffer_table{}
        {
            setName( default_name );
            setDoc( "No doc..." );

            {
                PyObject* table_as_object = reinterpret_cast<PyObject*>(m_table);

                *table_as_object = PyObject{ _PyObject_EXTRA_INIT  1, nullptr }; // py_object_initializer -- nullptr because type must be init'd by user
                table_as_object->ob_type = &PyType_Type;
            }

            // QQQ m_table->ob_size = 0;

            m_table->tp_basicsize         = size_bytes;
            m_table->tp_itemsize          = 0;
            
            m_table->tp_dealloc           = 0; // (destructor) [](PyObject* pyob){ PyMem_Free(pyob); };
            m_table->tp_print             = 0;
            m_table->tp_getattr           = 0; // Methods to implement standard operations
            m_table->tp_setattr           = 0;
            m_table->tp_repr              = 0;
            
            m_table->tp_as_number         = 0;
            m_table->tp_as_sequence       = 0; // Method suites for standard classes
            m_table->tp_as_mapping        = 0;
            
            m_table->tp_hash              = 0;
            m_table->tp_call              = 0;
            m_table->tp_str               = 0; // More standard operations (here for binary compatibility)
            m_table->tp_getattro          = 0;
            m_table->tp_setattro          = 0;
            
            m_table->tp_as_buffer         = 0; // Functions to access object as input/output buffer
            
            m_table->tp_flags             = Py_TPFLAGS_DEFAULT; // Flags to define presence of optional/expanded features
            
            
            m_table->tp_traverse          = 0;
            
            m_table->tp_clear             = 0; // delete references to contained objects
            
            // Assigned meaning in release 2.1
            m_table->tp_richcompare       = 0; // rich comparisons
            
            m_table->tp_weaklistoffset    = 0; // weak reference enabler
            
            m_table->tp_iter              = 0; // Iterators
            m_table->tp_iternext          = 0;
            
            m_table->tp_methods           = 0; // Attribute descriptor and subclassing stuff
            m_table->tp_members           = 0;
            m_table->tp_getset            = 0;
            m_table->tp_base              = 0;
            m_table->tp_dict              = 0;
            m_table->tp_descr_get         = 0;
            m_table->tp_descr_set         = 0;
            m_table->tp_dictoffset        = 0;
            m_table->tp_init              = 0;
            m_table->tp_alloc             = 0;
            m_table->tp_new               = 0;
            m_table->tp_free              = 0; // Low-level free-memory routine
            m_table->tp_is_gc             = 0; // For PyObject_IS_GC
            m_table->tp_bases             = 0;
            m_table->tp_mro               = 0; // method resolution order
            m_table->tp_cache             = 0;
            m_table->tp_subclasses        = 0;
            m_table->tp_weaklist          = 0;
            m_table->tp_del               = 0;
            
            m_table->tp_version_tag       = 0; // Type attribute cache version tag. Added in version 2.6
            
#ifdef COUNT_ALLOCS
            m_table->tp_alloc             = 0;
            m_table->tp_free              = 0;
            m_table->tp_maxalloc          = 0;
            m_table->tp_orev              = 0;
            m_table->tp_next              = 0;
#endif
        }
        
        ~TypeObject()
        {
            delete m_table;
            delete sequence_table;
            delete mapping_table;
            delete number_table;
            delete buffer_table;
        }


        const std::string  getName() const { return std::string{m_table->tp_name}; };
        const std::string  getDoc()  const { return std::string{m_table->tp_doc}; }

        void setName( const std::string& s ) { m_name = s;  m_table->tp_name = const_cast<char *>( m_name.c_str() ); }
        void setDoc(  const std::string& s ) { m_doc = s;   m_table->tp_doc  = const_cast<char *>( m_doc.c_str() ); }

        /*
         Me: (just to check I've got this right...) say 'tp_getattro' for example, signature is
           "PyObject*( PyObject* self, PyObject* name)". So when Python invokes this slot, it is 
           giving borrowed reference to self,name. And I need to return the new reference to 
           whatever PyObject* I return?

        Yhg1s: For any function that returns any kind of Python object, it returns a new reference
           unless it is explicitly documented to return a borrowed reference.
         
         For any function called by Python, it will give you a borrowed reference unless it's
           explicitly documented that you receive a reference of your own (I can't imagine any such case.)
        */

        void supportClass      () { m_table->tp_flags |= Py_TPFLAGS_BASETYPE; }


        void supportGetattr    () { BIND(m_table->tp_getattr , ExtObjBase::getattr); }
        void supportSetattr    () { BIND(m_table->tp_setattr , ExtObjBase::setattr); }
        void supportGetattro   () { BIND(m_table->tp_getattro, ExtObjBase::getattro); }
        void supportSetattro   () { BIND(m_table->tp_setattro, ExtObjBase::setattro); }

        /*
          MARKER_STARTUP__3.3b slot trampoline
          Back to the PyTypeObject's function pointer m_table
          A captureless lambda has a C-style signature, so can be used in place of a C function pointer
          So we insert a captureless lambda into every m_table slot that our extension object will make use of
          This lambda uses try{...}catch{}.
          So any C++ exception gets caught, and handed back to Python Runtime as a Python error code
          rather than crashing the process.
        */

        void supportRichCompare() { BIND(m_table->tp_richcompare, ExtObjBase::richcompare ); }
        void supportRepr       () { BIND(m_table->tp_repr       , ExtObjBase::repr        ); }
        void supportStr        () { BIND(m_table->tp_str        , ExtObjBase::str         ); }
        void supportHash       () { BIND(m_table->tp_hash       , ExtObjBase::hash        ); }
        void supportCall       () { BIND(m_table->tp_call       , ExtObjBase::call        ); } // call(Object{args}, Object{'D', kw} )
        void supportIter       () { BIND(m_table->tp_iter       , ExtObjBase::iter        );
                                    BIND(m_table->tp_iternext   , ExtObjBase::iternext    ); }

        #ifdef PYCXX_PYTHON_2TO3
        void supportPrint      () { BIND(m_table->tp_print      , print       ); }
        void supportCompare    () {                                          ; }
        #endif

        // ^ note Object{'d', kw} Will attempts to convert kw to PyDict_Type
        // which it should be anyway, but in case it was nullptr, we will now have an empty dictionary object
        // although, maybe we should do this work inside 'call'? What would call's overrides prefer?

        void supportSequenceType()
        {
            if( !sequence_table )
            {
                sequence_table = new PySequenceMethods{}; // {} ensures new fields are 0
                m_table->tp_as_sequence = sequence_table;
                
                //sequence_table->sq_length       = sequence_length_handler;
                BIND( sequence_table->sq_length     , ExtObjBase::sequence_length   );
                BIND( sequence_table->sq_concat     , ExtObjBase::sequence_concat   );
                BIND( sequence_table->sq_repeat     , ExtObjBase::sequence_repeat   );
                BIND( sequence_table->sq_item       , ExtObjBase::sequence_item     );
                BIND( sequence_table->sq_ass_item   , ExtObjBase::sequence_ass_item );
            }
        }
        
        void supportMappingType()
        {
            if( !mapping_table )
            {
                mapping_table = new PyMappingMethods{};
                m_table->tp_as_mapping = mapping_table;
                
                BIND( mapping_table->mp_length         , ExtObjBase::mapping_length        );
                BIND( mapping_table->mp_subscript      , ExtObjBase::mapping_subscript     );
                BIND( mapping_table->mp_ass_subscript  , ExtObjBase::mapping_ass_subscript );
            }
           
        }
        
        void supportNumberType()
        {
            if( !number_table )
            {
                number_table = new PyNumberMethods{};
                m_table->tp_as_number = number_table;

                BIND( number_table->nb_int      , ExtObjBase::number_int        );
                BIND( number_table->nb_float    , ExtObjBase::number_float      );
                BIND( number_table->nb_negative , ExtObjBase::number_negative   );
                BIND( number_table->nb_positive , ExtObjBase::number_positive   );
                BIND( number_table->nb_absolute , ExtObjBase::number_absolute   );
                BIND( number_table->nb_invert   , ExtObjBase::number_invert     );
                BIND( number_table->nb_add      , ExtObjBase::number_add        );
                BIND( number_table->nb_subtract , ExtObjBase::number_subtract   );
                BIND( number_table->nb_multiply , ExtObjBase::number_multiply   );
                BIND( number_table->nb_remainder, ExtObjBase::number_remainder  );
                BIND( number_table->nb_divmod   , ExtObjBase::number_divmod     );
                BIND( number_table->nb_lshift   , ExtObjBase::number_lshift     );
                BIND( number_table->nb_rshift   , ExtObjBase::number_rshift     );
                BIND( number_table->nb_and      , ExtObjBase::number_and        );
                BIND( number_table->nb_xor      , ExtObjBase::number_xor        );
                BIND( number_table->nb_or       , ExtObjBase::number_or         );
                BIND( number_table->nb_power    , ExtObjBase::number_power      );
            }
        }

        void supportBufferType()
        {
            if( !buffer_table )
            {
                buffer_table = new PyBufferProcs{};
                m_table->tp_as_buffer = buffer_table;
                
                BIND( buffer_table->bf_getbuffer, ExtObjBase::buffer_get );

                // NOTE: bf_releasebuffer has no way to indicate error to Python
                buffer_table->bf_releasebuffer  = [] (PyObject* self, Py_buffer* buf) { cxxbase_for(self)->buffer_release(buf); };
            }
           
        }
        
        // call (once all support functions have been called) to ready the type
        bool readyType() {
            return PyType_Ready(m_table) >= 0;
        }

        // prevent the compiler generating these unwanted functions
        TypeObject    ( const TypeObject& ) = delete;
        void operator=( const TypeObject& ) = delete;
    };

} // Namespace Py
