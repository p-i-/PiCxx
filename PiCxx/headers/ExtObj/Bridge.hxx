#pragma once

/*
 Notes:
    When we create an instance of an extension object,
        - Python runtime needs a new PyObject instance to represent it
        - also an instance of its corresponding C++ class is created

    Bridge connects these to one another.

    Bridge is only used for new-style classes. Here's why:

    For old-style classes...
        Python stores each instance as a PyObject.
        In C++ we just do:
        
            class OldStyle : PyObject

        Now the consumer builds their own custom old-style class:
        
            class Foo : OldStyle

        And make sure that when Python creates an instance, it allocates enough space
        for an entire Foo instance, the first chunk of which will be its PyObject base.
        (search for tp_basicsize to see how this gets done)

        And then whenever Python fires a slot for this object, it will pass its PyObject* as
        the first parameter, and we can just typecast back to Foo* to recover the Foo instance.
        
        Neat huh?
    
    For new-style classes...
        It isn't so easy.
        Because we wish to support deriving in Python
        e.g. look in test_funcmapper.py:

            class Derived(test_funcmapper.new_style_class):
            
        And the derived class might not be guaranteed to have the same memory 
        footprint as PyObject, i.e. it might be bigger.

        Because of this, we can't use the nice tidy old-style trick.
        (If you have 'class A:B:C' then the memory footprint will be C then B then A
        C's size is fixed, it can't spill over into B).
        
        Instead we tag not an entire C++ class onto the end of the PyObject,
        but a pointer to one.
        
        Now when Python fires a slot for this object, passing the responsible PyObject*,
        we have to grab this pointer and dereference it to recover the corresponding C++ class.
        
        This means that for new-style classes, the PyObject base-class is UNUSED!

    Search ExtObject.hxx for MARKER_extobj_allocation

    http://stackoverflow.com/questions/26961000/c-api-allocating-pytypeobject-extension
    http://stackoverflow.com/questions/27564257/why-does-pycxx-handle-new-style-classes-in-the-way-it-does
    
    http://stackoverflow.com/questions/27650814/set-base-to-object-of-undetermined-size
    http://stackoverflow.com/questions/27662074/how-to-tidy-fix-pycxxs-creation-of-new-style-python-extension-class
    http://stackoverflow.com/questions/27666958/how-does-python-allocate-memory-for-objects-of-classes-derived-from-c-types
 */

namespace Py
{
    struct Bridge
    {
        PyObject_HEAD
        ExtObjBase* m_pycxx_object;
    };


    inline ExtObjBase* cxxbase_for( PyObject* pyob )
    {
        if( pyob->ob_type->tp_flags & Py_TPFLAGS_BASETYPE )
        {
            auto bridge = (const Bridge&)*pyob;
            return bridge.m_pycxx_object;
        }
        else
            return (ExtObjBase*)( pyob );
    }
}

