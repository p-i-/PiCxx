#pragma once

/*
    This is the heart of the extension object

    For each extension object we must set up and feed a PyTypeObject into Python

    This PyTypeObject contains several tables of function-pointers
    Python will invoke these.
    We place a trampoline at each foo-slot, bouncing the call to a corresponding ExtObjBase::foo()
    The consumer will provide a Final::foo() override for every slot they wish to support
    If they forget, they will get warned by the virtual base implementation

    The second thing this class does is: it attaches FuncMapper

Discussion:
    Why does FuncMapper do different things for old-style class & module and for new-style class?
    
    old-style class
        overrides getattr:

            typeobject().supportGetattr();

        Which is just one of the many slots of the PyTypeObject
        When Python runtime fires the slot, our trampoline bounces us to the virtual ExtObjBase::getattr()
        Which is overridden by OldStyle::getattr

            Object getattr( const std::string name ) override {
                return getattr_default(name);
            }

        which looks through the object's methods() method table, finds the appropriate method and calls it
        Note that we are already in the correct object instance thanks to the trampolining,
        And we have the required method name as parameter.
        So it is a simple look up.
        We then construct and return a Python function object back to Python:
        
            return pmethodmap_item -> ConstructPyFunc(this);
        
        Note that at this point we leave the trampoline, returning control back to the Python runtime.

        To see what ConstructPyFunc does, you can look in FuncMapper::MethodMapItem::ConstructPyFunc
        
        To summarise, it packages up the following things into a Python function object:
            * A capsule containing this instance (as void*) and the method-map-item instance (as void*)
            * A handler function that will get invoked (passing the capsule) when Python executes this function object.

        (NOTE: something incredibly dodgy has to be done to make this work: since there are 3 handlers of different types
        for noarg vararg keyword, that is 3 different function signatures, we typecast the relevant handler to a
        PyCFunction. It's horrible, but Python is able to correctly invoke it.)

        So when Python (immediately) executes the function object, it will call to the handler passing the capsule, 
        which will unpack the data from the capsules and execute the correct C++ method
        
        ok, done! Next!
        
    Module:
        The module, when it initialises, grabs the module-dictionary and populates it with a {StringName:PyFunction} pair
        for each method.
        
        In order to construct the PyFunction, we feed in the module instance.
        So when Python executes that function, again our handler breaks open the capsule to get the module instance,
        And uses that to springboard to the correct module instance method, using the same mechanism as above
        (note that FuncMapper treats old-style class and module equivalently -- as far as it is concerned
        It just needs to package up a Function object on demand for each method)
        
    New-style class:
        This uses yet another technique.
        It creates a PyMethodDef table containing an entry for each method, and feeds this table into 
        the PyTypeObject's table->tp_methods
        In this case we don't get the opportunity to package up the target object instance in a capsule
        No, the first thing we know about it is when one of these tp_methods slots gets hit
        triggering whatever handler we put in the PyMethodDef
        
        The only information we DO get is the PyObject* for the PyObject responsible for the call
        -- that comes in as the first parameter!
        
        To do this, we have to use the bridge trick.
        i.e. When we created the PyObject, we tagged on an extra pointer pointing to the corresponding C++ class instance
        So the handler needs to retrieve this pointer, typecast it back into the base (FuncMapper) object,
        and invoke the appropriate handler on that base object.
        
        Note that unlike the above two cases, we now need a separate handler function for each method.
        Because there is no no way of the handler function knowing WHICH method invoked it.
        The only parameter it gets is the PyObject* responsible.
        
    For future consideration:
        Figure out whether we should be using the final technique in all cases
        That would certainly make that code more manageable.
        Note that in the new style technique, we need one separate handler for each method.
        So it would have a bigger footprint four classes with a lot of methods.
        But I really can't imagine that being any kind of obstacle.
*/

namespace Py
{
    template< typename Final >
    class ExtObject : public FuncMapper<Final> , public ExtObjBase
    {
    protected:
        /*
          Notice how the TypeObject single-instance gets created the first time it is referenced
          Notice also that the amount of memory for Python to allocate for each instance depends on
          whether we are using old or new style classes.

          A full discussion of that is at the top of this file.
         */
        // MARKER_STARTUP__3.2b typeobject() single-instance
        static TypeObject& typeobject()
        {
            static TypeObject* t{ nullptr };
            if( ! t ) {
                class NewStyle;
                bool is_newstyle = std::is_base_of<NewStyle, Final>::value;
                size_t finalsize = is_newstyle ? sizeof(Bridge) : sizeof(Final);

                std::string finalname = typeid(Final).name();

                t = new TypeObject{ finalname, finalsize };

                COUT( "NEWTypeObject: " << finalname << "@" << ADDR(t) );
            }
            
            return *t;
        }

    public:
        static PyTypeObject* table()                    { return typeobject().table(); }
        static Object        type()                     { return Object{ charge(  (PyObject*)(table())  )  }; }

        static bool          check( PyObject* p )       { return p->ob_type == table(); }
        static bool          check( const Object& ob )  { return check(ob.ptr()); }
    };
}
