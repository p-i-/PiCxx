#pragma once

namespace Py
{
    template<typename Final>
    class NewStyle : public ExtObject<Final>
    {
    private:
        Bridge* m_bridge;

    protected:
        static TypeObject& typeobject() { return ExtObject<Final>::typeobject(); }
        static PyTypeObject* table()    { return typeobject().table(); }

        static typename FuncMapper<Final>::method_map_t& method_map() {
            return FuncMapper<Final>::methods();
        }

    public:
        PyObject*    selfPtr() override { return reinterpret_cast<PyObject*>(m_bridge); }
        Object       self()    override { return Object{ charge(selfPtr()) }; }

        /*
          MARKER_STARTUP__3.1b new-style class one_time_setup()
          
          Same basic story as _old, we just set the type-object up differently
          and use a different mechanism for registering the object's methods.
         */

        static void one_time_setup()
        {
            COUT( "NewStyle::one_time_setup()" );

            //TypeObject& typeobject{ ExtObject<Final>::typeobject() };

            // 2-stage init'n: http://stackoverflow.com/questions/573275/python-c-api-object-allocation
            table()->tp_new = new_func;
            table()->tp_init = init_func;

            table()->tp_dealloc = dealloc_func;

            // this should be named supportInheritance, or supportUseAsBaseType
            // only the new style class allows this
            typeobject().supportClass();

            // always support get and set attr
            typeobject().supportGetattro();
            typeobject().supportSetattro();

            method_map().clear();

            Final::setup();

            // add our methods to the extension type's method table
            {
                auto py_method_table = new PyMethodDef[ method_map().size() + 1 ] {}; // +1 for sentinel, which must be 0'd, which it is thx to {}

                int i=0;
                for( auto& m : method_map() ) {
                    COUT( "    Importing method: " << m.first );
                    py_method_table[ i++ ] = *m.second;
                }

                table()->tp_methods = py_method_table;
            }

            typeobject().readyType();
        }


    private:
        // this will get called when we readyType() on the associated PyTypeObject
        static PyObject* new_func( PyTypeObject* subtype, PyObject* args, PyObject* kwds )
        {
            // check to make sure subtype is either equal to our PyTypeObject, or derives from it
            //const TypeObject&  typeObject = ExtObject<Final>::typeobject();
            if( ! PyType_IsSubtype( subtype, typeobject().table() ) )
                throw Exception( "wtf happened?" );

            // First we create the Python object.
            // The type-object's tp_basicsize is set to sizeof(Bridge)
            // (Note: We could maybe use PyType_GenericNew for this:
            //   http://stackoverflow.com/questions/573275/python-c-api-object-allocation )
            //
//            COUT( "NewStyle: subtype->tp_alloc: " << ADDR(subtype->tp_alloc) );
            PyObject* pyob = subtype->tp_alloc(subtype,0);
            // ^ Yhg1s: PyType_Ready() fills various tp_* fields depending on the bases of your type.
            // ^ Wooble: the tp_alloc documentation says the refcount is set to 1 and the memory block is zeroed.
            Bridge* bridge = reinterpret_cast<Bridge*>(pyob);

            COUT( "NewStyle(): " << ADDR(pyob) );
            // We construct the C++ object later in init_func (below)
            bridge->m_pycxx_object = nullptr;

            return pyob;
        }


        static int init_func( PyObject* self, PyObject* args, PyObject* kwds )
        {
            try
            {
                //
                //PyObject_Init( self, table() );

                Bridge* bridge{ reinterpret_cast<Bridge*>(self) };

                // NOTE: observe this is where we invoke the constructor, but indirectly (i.e. through final)
                if( bridge->m_pycxx_object == nullptr )
                    bridge->m_pycxx_object = new Final{ bridge, to_tuple(args), to_dict(kwds) };

                else
                    bridge->m_pycxx_object->reinit( to_tuple(args), to_dict(kwds) );
            }
            catch ( const Exception& e )
            {
                e.set_or_modify_python_error_indicator();
                return -1;
            }
            catch( ... )
            {
                Exception e{ TRACE, "Unknown exception in NewStyle::init_func" };
                e.set_or_modify_python_error_indicator();
                return -1;
            }
            
            return 0;
        }

    protected:
        explicit NewStyle( Bridge* self, /*Tuple*/const Object& args, /*Dict*/const Object& kwds )
        : m_bridge{self}
        {
            COUT( "NewStyle::NewStyle()" );
        }

    private:
        // http://stackoverflow.com/questions/26961000/c-api-allocating-pytypeobject-extension
        // TODO: http://stackoverflow.com/questions/24468667/whats-the-difference-between-tp-clear-tp-dealloc-and-tp-free/
        static void dealloc_func( PyObject* pyob )
        {
            COUT( "tp_dealloc for NEW-STYLE: " << ADDR(pyob) );

            auto final = static_cast<Final*>( cxxbase_for(pyob) );

            delete final;
            PyMem_Free(pyob);

            //pyob->ob_type->tp_free(self);
        }

        // prevent the compiler generating these unwanted functions
        explicit NewStyle( const NewStyle<Final> &other ) = delete;
        void operator=     ( const NewStyle<Final> &rhs   ) = delete;
    };
    
} // Namespace Py
