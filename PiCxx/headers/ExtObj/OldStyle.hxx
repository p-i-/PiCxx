#pragma once

namespace Py
{
    template<typename Final>
    class OldStyle : public ExtObject<Final>
    {
    public:
        PyObject* selfPtr() override  { return this; }
        Object    self()    override  { return Object{ charge(this) }; }
        // ^ Notice inheritance -- OldStyle : ExtObject<Final> : ExtObjBase : PyObject

        /*
          As this intermediate class is templated upon Final, and the base class is ALSO
          templated upon Final (CRTP-esque), the bass class is considered incomplete.
          i.e. It can change shape depending on Final
          Which seems to mean that we can't just inherit its static typeobject() method by typing typeobject()
          Instead we have to type ExtObject<Final>::typeobject()
          I'm not sure why it has to be this way, I think it's something along the lines of:
                The base class is incomplete without throwing in the Final templated parameter
                And you can't access members of an incomplete type
          Hence this convenience function to save that ungainly kerfuffle
         */
        static TypeObject& typeobject() { return ExtObject<Final>::typeobject(); }
        static PyTypeObject* table() { return typeobject().table(); }

        static typename FuncMapper<Final>::method_map_t& method_map() {
            return FuncMapper<Final>::methods();
        }

        // MARKER_STARTUP__3.1a old-style class one_time_setup()
        static void one_time_setup()
        {
            COUT( "OldStyle::one_time_setup()" );
            // MARKER_STARTUP__3.2a create typeobject()
            // This is our opportunity to create our own PyTypeObject.
            // it will get created the first time it is referenced,
            // which is by calling table(). jumpnext.
            table()->tp_dealloc =
                [] (PyObject* t)
                {
                    COUT( "tp_dealloc for OLD-STYLE: " << ADDR(t) );
                    // Don't do PyMem_Free(t); as Python never actually allocated space, WE did!
                    delete (Final*)(t);
                };

            //  MARKER_STARTUP__3.3a  typeobject().supportGetattr()
            typeobject().supportGetattr(); // every object must support getattr

            method_map().clear();

            Final::setup();
            typeobject().readyType();
        }

        /*
          MARKER_STARTUP__3.3d C++ bounces base -> final
            ...which is here!
         */
        // every object needs getattr implemented to support methods
        Object getattr( const std::string name ) override {
            COUT( "Hit old-style::getattr override!" );
            return getattr_default(name);
        }

    protected:
        explicit OldStyle()
        {
            // http://stackoverflow.com/questions/4163018/create-an-object-using-pythons-c-api
            // ^ only skip allocation, as C++ has already done allocated space (via the PyObject bass class)
            PyObject_Init( this, table() );
            COUT( "OldStyle(): " << ADDR((PyObject*)this) );

            // ^ INCREF-s (actually sets refcount to 1 via _Py_NewReference)
            // ALERT: Python runtime WON'T decref upon termination
        }

        virtual ~OldStyle()
        { }

        // support the default attributes, __name__, __doc__ and methods
        virtual Object getattr_default( const std::string name )
        {
            //PyTypeObject* table = table();

            if( name == "__name__" && table()->tp_name != nullptr ) return Object{ table()->tp_name };
            if( name == "__doc__"  && table()->tp_doc  != nullptr ) return Object{ table()->tp_doc };

            // trying to fake out being a class for help()
            else if( name == "__bases__"  )     return Object{'T'};
            else if( name == "__module__"  )    return None();
            else if( name == "__dict__"  )      return Object{'D'};

            return getattr_methods( name );
        }

        // turn a name into function object
        virtual Object getattr_methods( const std::string& name )
        {
            // see if name exists and get entry with method
            auto i = method_map().find(name);

            // // name doesn't exist in our method map...
            if( i == method_map().end() ) {
                COUT( "old-style: No match!" );
                if( name == "__methods__" ) {
                    // return List of all methods in map
                    Object L{'L'};
                    for( const auto& j : method_map() )
                        L.append(Object/*String*/{j.first});
                    return L;
                }
                else
                    THROW( std::string{"Attribute error:"} + name );
            }

            // ok, so name WAS found in the method map.
            // return to python a callable object that will invoke this method on this particular instance
            COUT( "old-style: Got match!" );
            return i -> second -> ConstructPyFunc(this);
        }

    private:
        // prevent the compiler generating these unwanted functions
        explicit OldStyle( const OldStyle<Final>& other ) = delete;
        void operator=     ( const OldStyle<Final>& rhs   ) = delete;
    };

} // Namespace Py

