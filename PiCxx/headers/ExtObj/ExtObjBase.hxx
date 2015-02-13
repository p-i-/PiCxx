#pragma once

// MARKER_STARTUP__3.2d trampolining the PyTypeObject's slots to consumer-methods
// although this needs to be a separate walk-through, it's worth a quick glance now.

/*
     To create on Your own custom Python extension TYPE, you inherit from OldStyle or NewStyle
     Both of which inherit from ExtObject
     ExtObject derives from ExtObjBase which derives from PyObject, 
     
     ...which means it can be typecast back into a PyObject
     However, you should NEVER rely on this, as NewStyle doesn't make use of this PyObject base.
     Instead use the cxxbase_for() function in Bridge.hxx
     
     ExtObject contains default implementations for all of the functions in the PyTypeObject's
     function-pointer table.
     
     The final extension object must override all of those it wishes to use.
     The default implementation gives an error saying "override me!"

     /Users/pi/Downloads/Python-3.4.2/Include/object.h

 Note on destruction (from PyCXX):
     There are two ways that extension objects can get destroyed.
        (1) Their reference count goes to zero
        (2) Someone does an explicit delete on a pointer.
    
     In (1) the problem is to get the destructor called.
        To do this we register a special deallocator
         in the Python type object (see typeobject()).
     
     In (2) there is no problem, the d'tor gets called.

     OldStyle does not use the usual Python heap allocator, 
     instead using new/delete. We do the setting of the type object
     and reference count, usually done by PyObject_New, in the 
     base class c'tor.

     This special deallocator does a delete on the pointer.
*/


namespace Py
{
    class ExtObjBase : public PyObject
    {
    private:
        using O = const Object;

    public:
        ExtObjBase()           {         ob_refcnt = 0; }
        virtual ~ExtObjBase()  { assert( ob_refcnt == 0 ); }

        /*
         All of these are virtual ExtObject functions.
         When you make an OldStyle (or _new) object, you need to provide overrides
         for all of these functions that may be used (by Python runtime, or us I guess).
         If one gets used for which no override is provided, this default implementation 
         just flags an error reminding us to provide it.
         */

        // http://en.cppreference.com/w/cpp/preprocessor/replace
        #define WARN(FUNC, RET) \
                        THROW( "Hit base: " #FUNC " -- Extension object MUST provide override! " ); \
                        return RET;

        // object basics
        #ifdef PYCXX_PYTHON_2TO3
        virtual int print( FILE*, int )                     { WARN(print, -1); }
        #endif

        // MARKER_STARTUP__3.3c trampolines to base

        virtual Object getattr( std::string )               { WARN(getattr, None()); }
        virtual int    setattr( std::string, O )            { WARN(setattr, -1); }
        virtual int    compare( O )                         { WARN(compare, -1); }
        virtual Object richcompare( O , int )               { WARN(richcompare, None()); }
        virtual Object repr( )                              { WARN(repr, None()); }
        virtual Object str( )                               { WARN(str, None()); }
        virtual long   hash( )                              { WARN(hash, -1); }
        virtual Object call( O , O )                        { WARN(call, None()); }
        virtual Object iter( )                              { WARN(iter, None()); }
        virtual Object iternext( )                          { WARN(iternext, None()); }

        virtual Object  getattro         ( O name)          { return genericGetAttro(name); }
        virtual int     setattro         ( O name, O value) { return genericSetAttro(name, value); }

                Object  genericGetAttro  ( O name)          { return Object{  charge(PyObject_GenericGetAttr( selfPtr(), *name         )) }; }
                int     genericSetAttro  ( O name, O value) { return                 PyObject_GenericSetAttr( selfPtr(), *name, *value )   ; }

        // Sequence methods
        virtual int    sequence_length( )                   { WARN(sequence_length, -1); }
        virtual Object sequence_concat( O )                 { WARN(sequence_concat, None()); }
        virtual Object sequence_repeat( Py_ssize_t )        { WARN(sequence_repeat, None()); }
        virtual Object sequence_item( Py_ssize_t )          { WARN(sequence_item, None()); }
        virtual int    sequence_ass_item( Py_ssize_t , O )  { WARN(sequence_ass_item, -1); }


        // Mapping
        virtual int    mapping_length( )                    { WARN(mapping_length, -1); }
        virtual Object mapping_subscript( O )               { WARN(mapping_subscript, None()); }
        virtual int    mapping_ass_subscript( O , O )       { WARN(mapping_ass_subscript, -1); }

        // Number
        virtual Object number_negative( )    { WARN(number_negative, None()); }
        virtual Object number_positive( )    { WARN(number_positive, None()); }
        virtual Object number_absolute( )    { WARN(number_absolute, None()); }
        virtual Object number_invert( )      { WARN(number_invert, None()); }
        virtual Object number_int( )         { WARN(number_int, None()); }
        virtual Object number_float( )       { WARN(number_float, None()); }
        virtual Object number_long( )        { WARN(number_long, None()); }

        virtual Object number_add( O )       { WARN(number_add, None()); }
        virtual Object number_subtract( O )  { WARN(number_subtract, None()); }
        virtual Object number_multiply( O )  { WARN(number_multiply, None()); }
        virtual Object number_remainder( O ) { WARN(number_remainder, None()); }
        virtual Object number_divmod( O )    { WARN(number_divmod, None()); }
        virtual Object number_lshift( O )    { WARN(number_lshift, None()); }
        virtual Object number_rshift( O )    { WARN(number_rshift, None()); }
        virtual Object number_and( O )       { WARN(number_and, None()); }
        virtual Object number_xor( O )       { WARN(number_xor, None()); }
        virtual Object number_or( O )        { WARN(number_or, None()); }

        virtual Object number_power( O,O )   { WARN(number_power, None()); }

        // Buffer
        virtual int buffer_get( Py_buffer* , int flags)  { WARN(buffer_get, -1); }

        virtual int buffer_release( Py_buffer* buf ) { return 0; }
        // ^ This method is optional and only required if the buffer's memory is dynamic.
        
#undef WARN

        virtual void reinit( const Object& args, const Object& kwds) {
            THROW( "Must not call __init__ twice on this class" );
        }
        
    public:

        // helper functions to call function fn_name with 0 or more args
        // ref: http://stackoverflow.com/questions/26217558/succinctly-rewrite-a-set-of-functions-with-variable-number-of-arguments

        template <typename ... Arg>
        Object callOnSelf( const std::string &fn_name,  Arg&&... arg )
        {
            Object args{ 'T', std::forward<Arg>(arg)... };
            return  self().callMemberFunction( fn_name, args );
        }
        
        virtual PyObject* selfPtr() = 0;
        virtual Object self() = 0;
    };

}

