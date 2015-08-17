#pragma once

#include <functional>
#include <vector>
#include <map>

/*

 The consumer may add attributes to their extension modules and {old/new}-style extension objects.
 
 In Python, everything is a PyObject, and a PyObject has ATTRIBUTES
 C++ classes have METHODS

 Say the consumer wishes to add a 'cxxfoo' method, so that if Python encounters 'myob.foo()'
 it will invoke mycxxob.cxxfoo()
 
 The consumer just has to implement CxxOb::cxxfoo, and then call:

        register_method<&CxxOb::cxxfoo>( "foo" );

 ...at the appropriate points during initialisation.

 FuncMapper takes care of trampolining to mycxxob.cxxfoo() whenever Python encounters 'myob.foo()'

 http://codereview.stackexchange.com/questions/69876/static-to-instance-method-trampolining-with-templates
 http://codereview.stackexchange.com/questions/69545/recode-c-c-trampoline-function-macros-using-templates

 ToDo:
   * http://stackoverflow.com/questions/26941960/remove-duplication-in-c11-try-catch-pattern
   * http://stackoverflow.com/questions/26745234/error-explicit-specialization-of-foo-in-class-scope

   std::bind JOS 4.4
*/

// ----------------------------------------------------------------------

namespace Py
{
    /*
        Derive your final extension class or extension module from FuncMapper.
        Purpose of this class is to allow Python to call instance-methods in final.

        1. register_method<&Final::FuncMatchingF0_F1_or_F2>( "pyFuncName", "docstring" );
            (F0 F1 F2 ~ No-Args, Var-Args and Keyword, see below)
        
        2. m = methods()["pyFuncName"] to retrieve the associated MethodMapItem
            that got constructed and stored by register_method.
            
        3. m.ConstructPyFunc( instance_of_final ) to construct and return a Python Callable object
            that when invoked will call this method on instance_of_final.
     */

    template< class Final >
    class FuncMapper
    {
        using C = const char*;
        using S = const std::string;

    private:

        // Final may attempt to register functions with the following signatures
        // (so as to expose them to Python as No-Args, Var-Args and Keyword methods)
        using F0 = auto (Final::*)()                               -> Object;
        using F1 = auto (Final::*)(const Object&)                  -> Object;
        using F2 = auto (Final::*)(const Object&, const Object&)   -> Object;

        // This is our structure for storing one such function, in 2 forms:
        //    1. A function pointer to the actual C++ method of Final
        //    2. A PyMethodDef binding the name that Python is to use for this method together with a handler Python will call when that name is invoked
        // Note we derive from PyMethodDef, hence can conveniently typecast back to PyMethodDef when we feed this data into Python
        // Note also that the handler must be an "extern C" function pointer, as Python is written in C
        struct MethodMapItem : PyMethodDef
        {
        private:
            const char* copy(const char* s) {
                return s ? strdup(s) : nullptr;
            }

        public:
            ~MethodMapItem() {
                if( ml_name ) free(ml_name);
                if( ml_doc ) free(ml_doc);
            }

            // whichever signature matches, that entry gets used
            F0 f0{nullptr};
            F1 f1{nullptr};
            F2 f2{nullptr};

            // Final calls one of these three constructor overloads
            MethodMapItem( C name, F0 func, PyCFunction handler, C doc ) : PyMethodDef{ copy(name), handler, METH_NOARGS               , copy(doc) }, f0{func}  {}
            MethodMapItem( C name, F1 func, PyCFunction handler, C doc ) : PyMethodDef{ copy(name), handler, METH_VARARGS              , copy(doc) }, f1{func}  {}
            MethodMapItem( C name, F2 func, PyCFunction handler, C doc ) : PyMethodDef{ copy(name), handler, METH_VARARGS|METH_KEYWORDS, copy(doc) }, f2{func}  {}
            
            // Construct a PyFunction_Type object that can call the handler for this method
            //   on a particular instance of the final object (passed in)
            //
            // The handler will take care of running the method while trapping errors.
            //
            // Note that we don't execute the handler at this point
            // Instead we return this Python function back to the Python runtime
            // Maybe the Python coder didn't want the function executed but just wanted to grab a pointer to it:
            //    fp = MyExtnObj.func;
            // http://bytes.com/topic/python/answers/36081-pycfunction_new
            // http://stackoverflow.com/questions/13536669/embedding-python-with-c
            //
            Object ConstructPyFunc( void* inst )
            {
                // package both the instance calling this method, and this method itself...
                PyObject* inst_capsule = PyCapsule_New( (void*)inst, nullptr, nullptr ); // PyCapsule_New returns CHARGED pointer
                PyObject* item_capsule = PyCapsule_New( (void*)this, nullptr, nullptr );

                // ... into a Tuple
                Object capsules{ 'T', inst_capsule, item_capsule };

                // construct & return a Python callable object that will invoke this handler (passing capsules as first parameter)
                // The handler will take care of running the method while trapping errors.
                //
                // Note that we don't execute the handler at this point
                // Instead we return this Python function back to the Python runtime
                // Maybe the Python coder didn't want the function executed but just wanted to grab a pointer to it:
                //    fp = MyExt.func;
                // http://bytes.com/topic/python/answers/36081-pycfunction_new
                // http://stackoverflow.com/questions/13536669/embedding-python-with-c
                // https://github.com/python/cpython/blob/master/Objects/methodobject.c#L19-L48

                // ^ By charging capsules, we are making sure that the tuple object persists
                // Which will in turn ensure that the contained capsules persist
                // http://stackoverflow.com/questions/26716711/documentation-for-pycfunction-new-pycfunction-newex
                PyObject* func = PyCFunction_New( (PyMethodDef*)this, charge(*capsules) );

                // nedbat: don't all functions that create new Python objects have to return new references?
                return func;
            }
        };

    protected:

        // Each templated Final will have its own (static) method map, note that it is ITS duty to copy this into its method-table
        using method_map_t = std::map< std::string, MethodMapItem* >;


        static method_map_t& methods() {
            static method_map_t m;
            return m;
        }

    private:
        // The final class must call register_method for every method it wishes to expose to Python
        // for those, see below: they will invoke these internal methods.
        template<typename F>
        static void internal_register_method( C name,  F f,  PyCFunction h,  C doc )
        {
            // Check that all methods added are unique -- Python doesn't support overload.
            if( methods().find(name) != methods().end() )
                THROW(  std::string{"internal_register_method: '"} + std::string{name} + std::string{"' is already used"}  );
            else
                methods()[ name ] = new MethodMapItem{ name, f, h, doc };
        }

#pragma mark OLD-style class and MODULE
    private:
        // The Python runtime will call one of these handlers (whenever a particular method is invoked, it will look up & call the PyMethodDef in the object's method table)
        // note that Python is only able to store a C function-pointer (so handlers must be static)
        static PyObject* h0( PyObject* t, PyObject*  )                   { return handler( 0, t           ); }
        static PyObject* h1( PyObject* t, PyObject* args )               { return handler( 1, t, args     ); }
        static PyObject* h2( PyObject* t, PyObject* args, PyObject* kw ) { return handler( 2, t, args, kw ); }

        // The three above handlers all pipe into this one (to avoid code duplication)
        static PyObject* handler(
                                   int       h_012,
                                   PyObject* capsules,
                                   PyObject* args      = nullptr,
                                   PyObject* keywords  = nullptr
                                   )
        {
            COUT( "\n   Old-Style Handler #" << h_012 );

            /*
            Three separate objects require function mapping (and thus inherit from this class)
            
            ExtModule (which constructs a dictionary of Callable objects)
            OldStyle (which intercepts get_attr, and constructs the required Callable object)
            
            Both of them use ConstructPyFunc(...), which packages (into a Tuple) a pair of capsules that get received here as the first parameter.

            However, NewStyle directly writes a PyMethodDef table for the object,
            which requires a separate static handler for each slot.
            When Python invokes one of these methods, it feeds the calling py-object as the first parameter
            
            The following handler doesn't handle NewStyle methods.
            */

            try
            {
                // Break open the capsules bound to this PyMethodDef and extract...
                Object t = charge(capsules);
                
                PyObject* self_capsule = t[0].ptr(); //  0. a pointer to the Python-object instance that invoked this method
                PyObject* item_capsule = t[1].ptr(); //  1. a pointer to the MethodMapItem

                void* self_as_void = PyCapsule_GetPointer( self_capsule, nullptr );
                void* item_as_void = PyCapsule_GetPointer( item_capsule, nullptr );

                if( self_as_void == nullptr  ||  item_as_void == nullptr )
                    return nullptr;
                
                // Note that (0) is a pointer to an instance of our custom Python object Final,
                //   which ultimately derives from FuncMapper<Final>
                // So we can typecast it back to retrieve the associated base
                auto base = static_cast<FuncMapper*>(self_as_void);

                // Trigger invoke_method on this instance-base feeding in the (1) (which identifies the method that is to be invoked)
                Object result = base->invoke_method( item_as_void, h_012, args, keywords );

                // Give the result back to Python
                return charge(result.ptr());
            }
            catch ( const Exception& e )
            {
                COUT ("CAUGHT exception in OLD-style-class call-handler");
                e.set_or_modify_python_error_indicator();
                return nullptr;
            }
            // Note how we catch any C++ error that might occur, allowing Python to deal with it, rather than crashing!
            catch (...)
            {
                COUT ("Unknown exception in OLD-style-class call-handler");
                Exception e{ TRACE, "Unknown exception in OLD-style-class call-handler" };
                e.set_or_modify_python_error_indicator();
                return nullptr;
            }
        }


        Object invoke_method( void* item_as_void, int flag, PyObject* args, PyObject* kwds )
        {
            // typecast base back up to final class
            Final* self = static_cast<Final*>(this);

            // from OUR method map table (not Python's)...
            MethodMapItem* item = static_cast<MethodMapItem*>(item_as_void);

            COUT( "Invoking: " << item->ml_name );
            
            // ...invoke the method that got registered initially by the final class, returning it's return value
            return flag == 0 ? ( self ->* item->f0 )( )
                 : flag == 1 ? ( self ->* item->f1 )( to_tuple(args) )
                             : ( self ->* item->f2 )( to_tuple(args), to_dict(kwds) );

            // !!! Section 11.5 of "C++ Tour" may give a better way of doing this
        }

    protected:
        static void register_method( C name, F0 f, C doc=nullptr ) { internal_register_method( name, f, (PyCFunction)h0, doc ); }
        static void register_method( C name, F1 f, C doc=nullptr ) { internal_register_method( name, f, (PyCFunction)h1, doc ); }
        static void register_method( C name, F2 f, C doc=nullptr ) { internal_register_method( name, f, (PyCFunction)h2, doc ); }


#pragma mark NEW-STYLE CLASS

    private:

        static Final* final(PyObject* o) {
            return static_cast<Final*>(cxxbase_for(o));
        }

        static PyObject* handlerX( int h_012, std::function<Object()> lambda )
        {
            COUT( "\n   NewStyle handler #" << h_012 );
            try
            {
                return charge( *lambda() ); // feed charged ref back to Python
            }
            catch ( const Exception& e )
            {
                COUT ("CAUGHT exception in NEW-style-class call-handler");
                e.set_or_modify_python_error_indicator();
                return nullptr;
            }
            catch (...)
            {
                COUT ("Unknown exception in NEW-style-class call-handler");
                Exception e{ TRACE, "Unknown exception in NEW-style-class call-handler" };
                e.set_or_modify_python_error_indicator();
                return nullptr;
            }
        }

        // Note how we package and pass a lambda, so that we can reuse error trapping rather than have to write the code out three times.
        // to understand what the code does, imagine no handlerX and no lambda, just executing (final(o) ->* f)(whatever) and forwarding
        // whatever IT returns back to Python.
        #define P PyObject*
        template< F0 f > static P handler( P o, P   )      { return handlerX( 0, [&] ()->Object { return (final(o) ->* f)(                         ); }  ); }
        template< F1 f > static P handler( P o, P a )      { return handlerX( 1, [&] ()->Object { return (final(o) ->* f)( to_tuple(a)             ); }  ); }
        template< F2 f > static P handler( P o, P a, P k ) { return handlerX( 2, [&] ()->Object { return (final(o) ->* f)( to_tuple(a), to_dict(k) ); }  ); }
        #undef P
        
    protected:
        // overloads for a new-style class (notice that each method gets its own handler)
        template <F0 f> static void register_method( C name, C doc=nullptr )  { internal_register_method( name, f, (PyCFunction)&handler<f>, doc ); }
        template <F1 f> static void register_method( C name, C doc=nullptr )  { internal_register_method( name, f, (PyCFunction)&handler<f>, doc ); }
        template <F2 f> static void register_method( C name, C doc=nullptr )  { internal_register_method( name, f, (PyCFunction)&handler<f>, doc ); }


    };

} // Namespace Py
