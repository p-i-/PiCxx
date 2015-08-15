#pragma once

  /*
 Wrap Python stock objects e.g. List String Dict Bool etc
 */

#include "Base.hxx"

#include <iostream>
#include <sstream>
#include <string>
#include <iterator>
#include <utility>
#include <typeinfo>
#include <limits>
#include <stddef.h>

#include <complex>

namespace Py
{
    /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
    
    Object wraps a pointer to a PyObject
    (In Python, everything can be typecast (maybe down) to a PyObject)
    
    It provides a comprehensive C++ interface, so 
        Object py_i{42};                            // creates a 'PyLong_Type' PyObject using PyLong_FromLong
        Object py_f{3.14};                          // creates a 'PyFloat_Type' PyObject using PyFloat_FromDouble
        std::cout << py_i * py_f;                   // all mathematical operations get passed through to Python
                                                    // '<<' is overridden to use Python runtime to str() each object
        Object py_ffromi{PyFloat_Type, 1};          // creates a 'PyLong_Type' from 1 and uses Python runtime to convert to PyFloat_Type

    Also Object performs advanced implicit conversions, e.g.
        float k{ py_i };    
        
    This first leverages the Python runtime to construct a new PyFloat_Type PyObject from the received PyLong_Type
    It then uses PyFloat_AsDouble to convert this to a C++ double type, which then gets converted back to float type
    All intermediary PyObjects get cleared away, as they are themselves each wrapped in an enclosing Object
    
    Object provides all obvious C++type -> PyObject conversions, and all obvious PyObject -> C++type conversions.
    
        Object list{ 'L', 1, 2f, "three" };         // creates a 'PyList_Type' containing 3 items
        Object dict{ 'D', py_i, list, py_f, k};     // creates a 'PyDict_Type' with 2 key-value pairs
        
        dict["newkey"] = list[1];                   // use of [] for container objects

        for( const auto& i : dict.keys() )          // iterators provided to allow fast enumeration etc
            std::cout << dict[i];

    * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

    // !!! π ToDo: http://www.gotw.ca/publications/using_auto_ptr_effectively.htm

    #pragma mark  SFINAE Helpers

    /*
     SFINAE helper constructs for PyLong_Type, PyFloat_Type

     - - - - - - -
     Reduces to 'int' if pred==true, otherwise generates a compiler error
     This is for use in SFINAE template programming, e.g.
          template< typename T, subfail_unless__t<fooPred> = 0 > ...
     The above template would only generate whenever fooPred evaluates to true
     otherwise it would encounter substitution failure.
     */
    template<bool pred>
    using subfail_unless_t = typename std::enable_if< pred, int >::type;

    /*
     We want:
        Object{42}      // create 'PyLong_Type'   PyObject
        Object{3.14}    // create 'PyFloat_Type'  PyObject  ... etc.

     However, the obvious approach (provide one long and one double constructor
     and rely on C++ to promote other types to one of these) doesn't work, as:
        Object(long   l) : Object{ PyLong_FromLong(l)    }  { }
        Object(double d) : Object{ PyFloat_FromDouble(d) }  { } // <-- what happens for Object{42}?

     Unfortunately, int gets promoted to double, not long
     http://stackoverflow.com/questions/27276196/forward-integral-argument-to-one-constructor-floatingpoint-to-another

     Very annoying. Fortunately we can use some SFINAE cunning to pipe integral types
     to one constructor and floatingpoint types to another.  Woot!
     http://codereview.stackexchange.com/questions/71946/use-of-macros-to-aid-visual-parsing-of-sfinae-template-metaprogramming
    */

    // Need to decay e.g. int& -> int, otherwise std::is_integral< int& >::value comes out as false (!)
    template< typename T> using decay_t = typename std::decay<T>::type;

    template< typename T> static constexpr bool is_integral()  { return std::is_integral      < decay_t<T> >::value; }
    template< typename T> static constexpr bool is_floating()  { return std::is_floating_point< decay_t<T> >::value; }

    template< typename T> using subfail_unless_integral_t = subfail_unless_t< is_integral<T>() >;
    template< typename T> using subfail_unless_floating_t = subfail_unless_t< is_floating<T>() >;

    /*
        ^ Note: I wanted to put these inside the Object class, but is_integral() and is_floating() were
                causing "Inline function 'Py::Object::is_floating<_object *const &>' is not defined" compiler warnings.
                Even though the warnings don't seem to affect execution, I would rather have zero warnings.

       Additional relevant questions:
        http://stackoverflow.com/questions/27039035/how-to-use-stdconditional-to-set-type-depending-on-template-parameter-type
        http://stackoverflow.com/questions/27276196/forward-integral-argument-to-one-constructor-floatingpoint-to-another
        http://stackoverflow.com/questions/27429318/sfinae-gives-inheriting-constructor-does-not-inherit-ellipsis-warning
    */

    // - - - - - - -

    /*
     Objects ONLY get created from charged pointers.
     And ONLY the destructor neutralises the pointer.
     It is a failsafe mechanism.

     So long as the consumer ONLY supplies charged pointers, it can't go wrong, even if exceptions occur.
     The destructor will still always fire.

     Occasionally we need to pre-charge the pointer manually.
     
     NOTE: unless documented otherwise, Python Runtime feeds us neutral pointers, and expects us to return a charged pointer
    */
    inline static PyObject* charge( PyObject* pyob ) {
        Py_XINCREF(pyob);
        return pyob;
    }


#pragma mark  O B J E C T

    class Object
    {
    public:
        /*
         The pointer to the Python object
         Only Object sets this directly, hence private
         The default constructor for Object sets it to Py_None and child classes must use "set" to set it
         Note that this is the ONLY data member of the class(!)
         In fact it is the only data member to be found anywhere in this file!
        */
        PyObject* p{nullptr};

    private:
        void release() {
            Py_CLEAR(p);
        }

    public:
        // this is the important one, as it sets p
        // requires a CHARGED PyObject*
        Object( PyObject* pyob ) : p{pyob} { }

        // this is why we require charged pointer!
        ~Object() {
            release();
        }

        PyObject* ptr()         const { return p; }
        PyObject* operator * () const { return p; }
        /*
         ^ Convenience overload allows *myObject as a shorthand for myObject.ptr(), as the latter is slightly clunky
         This is because the operation is so frequently required, and it is a reasonable interpretation of what * does
         However, within this class, I prefer to use .p

         Could have also used:
            explicit operator PyObject* () const { return p; }
         Only this requires static_cast<PyObject*>(myObject), yuck -- it doesn't improve legibility
         And removing explicit keyword is potentially dangerous, allowing all manner of random pointer types
         to be (incorrectly) converted.
        */

        // copy construct from another Object (neutral)
        Object( const Object& ob ) : Object{ charge(ob.p) }  { }

        // default
        Object( ) : Object{ charge(Py_None) }  { }

        // NUMERIC TYPES

        // - - - - - - -

    // A S S I G N M E N T
    private:
        // assume we receive charged object
        void set_ptr( PyObject* pyob_charged )
        {
            if( p != pyob_charged ) {
                if(p) release();
                p = pyob_charged;
            }
        }

#pragma mark  [] ELEMENT ACCESS Py{Dict,List,Set,Tuple,Bytes}_Type

        // NOTE: https://docs.python.org/2.5/api/refcountDetails.html
        //    Generic functions that return object references, like PyObject_GetItem() and PySequence_GetItem(),
        //    always return a new reference (the caller becomes the owner of the reference).

        mutable bool m_resolve_me{false};
        PyObject* m_container{nullptr};
        PyObject* m_key{nullptr};

    public:
        /*
         We use some artful trickery allow both Lvalue and Rvalue access for container types using []
         e.g. 
            myList[1] = myDict["two"] // is valid
            
            http://stackoverflow.com/questions/27680299/implementing-recursive-proxy-pattern-in-c11
         
         I considered and discarded the idea of overloading ->
            http://stackoverflow.com/questions/27689105/overload-operator-to-forward-member-access-through-proxy
            http://stackoverflow.com/questions/27690419/how-to-overload-operator-in-c <- here I fought a troll(!) ;)
         */

        // First case: Object[] where Object is const. This case is simple and can ONLY be Rvalue-access. e.g. 'foo = myObject[42];'
        const Object operator[] (const Object& key)  const {
            return Object{ PyObject_GetItem( p, key.p ) };  // PyObject_GetItem returns CHARGED ptr
        }

        // Second case: unknown whether Lvalue or Rvalue access
        Object operator[] (const Object& key) {
            return Object{ *this, key };
        }

        //void set_item( const Object& key, const Object& value ) {
        //    PyObject_SetItem( p, key.p, charge(value.p) );
        //}

        #if 0
        // First two should get shadowed by third:
        //template<typename T, subfail_unless_integral_t<T> = 0>
        //const Object  operator[] (T index)              const { return Object{ PySequence_GetItem( p, static_cast<Py_ssize_t>(index) ) }; }
        //const Object  operator[] (const char* cstr)     const { return Object{ PyMapping_GetItemString(p,cstr) }; }
        //const Object  operator[] (const std::string& s) const { return (*this)[s.c_str()]; }
        // ^ Note: C++11 guarantees null termination for std::string.c_str()
        #endif


        Object( const Object& c, const Object& k )
            : m_container{c.p}, m_key{k.p}, m_resolve_me{true}
        {
            // the next command might set Python's error indicator
            // so let's check first to make sure the slate is clean at this point
            // (obviously we should always do this check before performing any operation that might set the indicator)
            throw_if_pyerr(TRACE);

            // for everything except lvalue access (ob[idx]=...), ob[idx] will be valid
            p = PyObject_GetItem( m_container, m_key ); // PyObject_GetItem returns CHARGED ptr
            if( p == nullptr ) {
                // ... However in the case of lvalue access, PyObject_GetItem will set Python's error indicator
                // so we must flush that error, as it was expected!
                PyErr_Clear();
                p = charge(Py_None);
            }
            // ^ either way, p ends up charged
        }

        //void maybe_resolve() {
        //    // e.g. obj[idx] = ...
        //    // in that case we currently have a 'floating' object
        //    // created by the [] operator, and initialised just setting m_container and m_key
        //    // now we must resolve it
        //    if( m_resolve_me ) {
        //        PyObject* item = PyObject_GetItem( m_container, m_key );
        //        if( item == nullptr ) throw Exception();
        //        set_ptr(item);
        //        m_resolve_me = false;
        //    }
        //}

    public:
        // this will attempt to convert ANY rhs to Object, which takes advantage of ALL the above constructor overrides
        Object& operator=( const Object& rhs )
        {
            /*
                1) normal situation
                2) this object is m_resolve_me, and we are assigning a normal object to it
                3) this object is m_resolve_me, and we are assigning a m_resolve_me object to it
                4) this object is normal, and we are assigning a m_resolve_me object to it
                
                1) we need to charge p
                2) same
                3) same
                4) same
                
                The only important thing is: we have to be neutral to rhs.p
                That means we have to charge it, as we will be subsequently neutralising it in the destructor
             */
            if( &rhs != this )
                *this = charge(rhs.p);

            return *this;
        }

        // (Always) assume charged pointer
        Object& operator=( PyObject* pyob )
        {
            if( m_resolve_me ) {
                // Yhg1s: the important fact is that PyObject_SetItem() *does not steal your references*.
                //        The call does not affect whether you own the references to 'k' and 'v'.
                PyObject_SetItem( m_container, m_key, pyob );
                // ^ !!! I'm not entirely sure that PyObject_SetItem is guaranteed to incref m_key and then decref it when
                //       either the key changes, the item is removed, or the dictionary is destroyed
                //       note that it might not be a dictionary, it might be a list in which case m_key is the index
                //       and should be an integer
                m_resolve_me = false;
            }

            set_ptr( pyob );

            return *this;
        }

#pragma mark  CONVERTER (PyFoo_Type -> PyBar_Type)

    public:
        /*
         (used in conversion TO various C++ types like float, double, unsigned long long, std::complex, std::string, etc )
         Attempt to convert an arbitrary PyObject* to a PyObject* of the specified type, e.g. PyFloat_Type, PyString_Type
         raise error we receive nullptr, or if the conversion fails
         return a NEW Object, even if that new object is referencing the same PyObject

            http://stackoverflow.com/questions/27383793/duplicate-a-pyobject-using-python-c-api

            http://stackoverflow.com/questions/27341023/force-type-conversions-using-python-c-api
            http://stackoverflow.com/questions/27431969/how-to-construct-a-complex-from-a-string-using-pythons-c-api
            http://stackoverflow.com/questions/27396268/how-does-pynumber-float-handle-an-argument-that-is-already-a-float
        */
        Object convert_to( PyTypeObject& python_typeobject ) const
        {
            // ??? Maybe we should allow this... Just construct default objects of the requested type
            if(!p)
                THROW( "convert_to: can't convert from nullptr" );

            if( &python_typeobject == p->ob_type )
                return Object{ charge(p) };

            if(0)
                COUT( "Converting: " << std::string{p->ob_type->tp_name} << " -> " << std::string{ python_typeobject.tp_name} );


            PyObject* result{ nullptr };

            // We must provide manual Bytes<->Unicode conversion to avoid "TypeError: cannot convert unicode object to bytes"
            if( &python_typeobject == &PyUnicode_Type )
                result = PyObject_Str(p);

            else if( &python_typeobject == &PyBytes_Type )
                result = PyUnicode_Check(p) ? PyUnicode_AsEncodedString(p,"utf-8",nullptr) : PyObject_Bytes(p);

            else
                result = PyObject_CallFunctionObjArgs( reinterpret_cast<PyObject*>(&python_typeobject), p, nullptr );


            throw_if_pyerr(TRACE);

            if( result == p )
                THROW( "(convert_to) result == p" );

            return Object{ result };
        }


#pragma mark PyBool_Type
    public:
        explicit Object( bool b ) : Object{ PyBool_FromLong(b?1:0) }  { }


#pragma mark PyLong_Type, PyFloat_Type
    private:
        // return CHARGED pointer
        template<typename T>
        PyObject* pyob_from_integral( T t ) {
            bool small{ sizeof(T) <= sizeof(long) };
            bool is_signed{ std::is_signed<T>::value };

            return small ? ( is_signed ? PyLong_FromLong(t)     : PyLong_FromUnsignedLong(t) )
                         : ( is_signed ? PyLong_FromLongLong(t) : PyLong_FromUnsignedLongLong(t) );
        }

        // return CHARGED pointer
        template<typename T>
        PyObject* pyob_from_floating(T t) {
            return PyFloat_FromDouble( static_cast<double>(t) ) ;
        }

    public:
        // Construct from C++ integral & floatingpoint types
        template<typename T, subfail_unless_integral_t<T> = 0>  Object( T&& t ) : Object{ pyob_from_integral(t) }  { }
        template<typename T, subfail_unless_floating_t<T> = 0>  Object( T&& t ) : Object{ pyob_from_floating(t) }  { }
            /* ^ Note that the first template encounters a substitution failure for any non-integral type,
                 hence only redirects integral types to init(long(t)).
                 
                 Similarly for floatingpoint types.
                 Alternative (more elaborate) design pattern here: http://ideone.com/oDOSLH
                 Reference for C++ integral & floatingpoint types here: http://en.cppreference.com/w/cpp/language/types
             */

        // Convert to integral types
        template<typename T, subfail_unless_integral_t<T> = 0 >
        explicit operator T() const {
            bool small{ sizeof(T) <= sizeof(long) }, is_signed{ std::is_signed<T>::value };

            Object as_pylong{ convert_to(PyLong_Type) }; // ToDo: Move construct

            if(   is_signed &&   small )    return static_cast<T>( PyLong_AsLong            (as_pylong.p) );
            if(   is_signed && ! small )    return static_cast<T>( PyLong_AsLongLong        (as_pylong.p) );
            if( ! is_signed &&   small )    return static_cast<T>( PyLong_AsUnsignedLong    (as_pylong.p) );
          /*if( ! is_signed && ! small )*/  return static_cast<T>( PyLong_AsUnsignedLongLong(as_pylong.p) );
        }

        // Convert to floatingpoint types
        explicit operator float()   const { return static_cast<float>(   PyFloat_AsDouble(convert_to(PyFloat_Type).p)  ); }
        explicit operator double()  const { return                       PyFloat_AsDouble(convert_to(PyFloat_Type).p)   ; }

        /*
            Object{const char*} -> PyBytes_Type
            Object{std::string&} -> PyUnicode_Type

            Note about language use:
               dump_?  implies  Py-Type    -> CXX-Type
               as_?    implies  PyFoo_Type -> PyBar_Type
        */


#pragma mark PyBytes_Type
        /*
            http://stackoverflow.com/questions/27459480/unicode-friendly-architecture-for-bridging-pythons-string-and-bytes-types-to-c
            http://stackoverflow.com/questions/27460578/what-is-the-difference-between-pybytes-type-and-pystring-type
         */
    public:
        //explicit Object( const char*  cstr )               : Object{ PyBytes_FromString( cstr )           }  { }
        //explicit Object( const char*  cstr, Py_ssize_t N ) : Object{ PyBytes_FromStringAndSize( cstr, N ) }  { }
        //
        //explicit Object( const char*  cstr )               : Object{ std::string{cstr} }  { }
        //explicit Object( const char*  cstr, Py_ssize_t N ) : Object{ std::string{cstr} }  { }

        std::string dump_bytestring() const {
            Object as_bytes{ convert_to(PyBytes_Type) };
            return std::string{ PyBytes_AsString(as_bytes.p) };
        }

        std::string dump_ascii() const {
            Object as_ascii{ PyObject_ASCII(p) }; // PyObject_ASCII creates a 'str' i.e. PyUnicode_Type
            return as_ascii.dump_bytestring();
        }


#pragma mark PyUnicode_Type
    public:
        // Encoding reference: https://docs.python.org/3.4/library/codecs.html
        Object(
               const std::string&  s
              ,long                N=-1
              ,const std::string&  enc="utf-8"
              ,const std::string&  err="" )
            : Object{ PyUnicode_Decode(
                                        s.data(),
                                        // for BYTEcount, use .size not .length
                                        static_cast<Py_ssize_t>( N == -1 ? s.size() : std::min<Py_ssize_t>(N,s.size()) ),
                                        enc.data(),
                                        err.data() ) }
        {
            throw_if_pyerr(TRACE);
        }

        Object(const std::string&  s
              ,const std::string& enc
              ,const std::string&  err="" )
            : Object{ s, -1, enc, err }
        { }

        // require this as const char* promotes to bool not std::string ( YUCK! Bad C++ :| )
        // http://ideone.com/dmf3gM
        Object(const char* cstr) : Object{std::string{cstr}} { }

        std::string dump_utf8string() const {
            // https://docs.python.org/3/c-api/unicode.html#c.PyUnicode_1BYTE_DATA
            // PyUnicode_READY? Py_UCS1* buf{ PyUnicode_1BYTE_DATA(as_utf8.ptr()) }; ?
            Object as_unicode{ convert_to(PyUnicode_Type) };        // ...   -> str
            Object as_utf8{ PyUnicode_AsUTF8String(as_unicode.p) }; // str   -> bytes

            return as_utf8.dump_bytestring();                       // bytes -> std::string
        }

        operator std::string() const { return dump_utf8string(); }

        // not sure about these ones...
        #if 0
        explicit Object( const Py_UNICODE*  u, Py_ssize_t N ) : Object{ PyUnicode_FromUnicode( u, N )                   }  { }
        explicit Object( const std::wstring& ws )             : Object{ ws.data(), static_cast<Py_ssize_t>(ws.length()) }  { }

        explicit operator std::wstring() const {
            Object as_unicode{ convert_to(PyUnicode_Type) };
            return std::wstring( PyUnicode_AS_UNICODE(as_unicode.p), static_cast<size_t>(PyUnicode_GET_SIZE(as_unicode.p)) );
        }
        #endif


#pragma mark PyComplex_Type
        // http://stackoverflow.com/questions/27431969/how-to-construct-a-complex-from-a-string-using-pythons-c-api
    public:
        template<typename T>
        explicit Object( std::complex<T> z ) : Object{
                                                    PyComplex_FromDoubles( static_cast<double>(z.real()), static_cast<double>(z.imag()) )
                                                    }  { }


        template<typename T, subfail_unless_floating_t<T> = 0>
        explicit operator std::complex<T>() const {
            Object as_complex{ convert_to(PyComplex_Type) };
            return std::complex<T>{
                                    static_cast<T>(PyComplex_RealAsDouble(as_complex.p)),
                                    static_cast<T>(PyComplex_ImagAsDouble(as_complex.p))
                                    };
        }


#pragma mark PyFunction_Type
    public:
        // Hope PyRuntime raises an exception if we invoke this on a non-callable object
        Object operator() ( )                                       { return Object{ charge( PyObject_CallObject           (p, nullptr       ) ) }; }
        Object operator() ( const Object& args )                    { return Object{ charge( PyObject_CallObject           (p, args.p        ) ) }; }
        Object operator() ( const Object& args, const Object& kwds ){ return Object{ charge( PyEval_CallObjectWithKeywords (p, args.p, kwds.p) ) }; }


#pragma mark PARAM PACKS FOR LIST DICT ETC

        // http://stackoverflow.com/questions/27418626/using-double-braces-e-g-fooinitializer-list-to-resolve-ambiguity
        // http://stackoverflow.com/questions/27500944/appropriate-syntax-for-initialising-python-c-api-container-types-list-dict-tupl

        template<typename Foo, typename ... More>
        void unpack_to_list( Object& list, Foo&& foo, More&& ... more ) {
            // PyList_Append does not "steal" a reference, i.e. it expects a neutral pointer
            Object ob{foo};
            //COUT(ob);
            PyList_Append( list.p, ob.p ); // PyList_Append INCREFs
            unpack_to_list( list, std::forward<More>(more) ... );
        }

        void unpack_to_list( Object& list )  { } // recursion terminator

        //template< Car car, Cdr ... cdr >
        template<typename ... Arg>
        Object( PyTypeObject& _type, Arg&& ... arg )
        {
            throw_if_pyerr(TRACE);
            Object list{ PyList_New(0) };
            unpack_to_list( list, std::forward<Arg>(arg) ... );
            Py_ssize_t N = PyObject_Length( list.p );

            if( &_type == &PyDict_Type )
            {
                if( N % 2 != 0 ) throw Exception{ "Must supply an even number of arguments to dictionary" };

                Object dict{ PyDict_New() };

                int i=0;
                while( i < N ) {
                    auto k = list[i++].p;
                    auto v = list[i++].p;
                    PyDict_SetItem( dict.p, k, v ); // PyDict_SetItem INCREFs k&v
                    //PyObject_SetItem( dict.p, list[i++].p, list[i++].p );
                }

                *this = dict;
            }

            else if(
                    & _type == & PyList_Type
                 || & _type == & PyTuple_Type
                 || & _type == & PySet_Type    )
            {
                *this = list.convert_to(_type);
            }

            else {
                Object obj( list[0] );
                *this = obj.convert_to(_type);
            }
        }

        // make this explicit, we don't want this constructor to be considered for implicit typecasts
        template<typename ... Arg>
        explicit Object( const char c, Arg&& ... arg )
        {
            if( c=='L' ) *this = Object{  PyList_Type, std::forward<Arg>(arg) ... };
            if( c=='T' ) *this = Object{ PyTuple_Type, std::forward<Arg>(arg) ... };
            if( c=='S' ) *this = Object{   PySet_Type, std::forward<Arg>(arg) ... };
            if( c=='D' ) *this = Object{  PyDict_Type, std::forward<Arg>(arg) ... };
            if( c=='B' ) *this = Object{ PyBytes_Type, std::forward<Arg>(arg) ... };
        }

        void append( const Object& ob ) {
            PyList_Append( p, ob.p ); // assume it doesn't steal reference. So it requires neutral pointer. ??? Answer // PyList_Append INCREFs, so yes!
        }

#pragma mark OPERATORS

        // O P E R A T O R S

        /* Operators, e.g. a+b
         If at least one of a,b derives from Object, we do:
            Object{a} + Object{b} -> Object

         This performs an unnecessary conversion (two in the event that both a,b are objects)
         However, it reduces code and complexity, and it is a lightweight conversion.

         Note that we don't need to define these operators as friend functions within Object
         They could equally well be unfriended and outside the class definition.
         
         However, because they pertain specifically to the Object class, I feel they belong here
        */
        template< typename T>
        static constexpr bool is_object() { return std::is_base_of< Object, T >::value; }

        template< typename T, typename U>
        using subfail_if_neither_is_object_t = subfail_unless_t< is_object<T>() || is_object<U>() >;

        using OpFunc = decltype(PyNumber_Add);
        static Object do_op( OpFunc& op_func, const Object& t, const Object& u ) {
            PyObject* ret{ op_func( t.p, u.p ) };

            throw_if_pyerr(TRACE);
            return Object{ret};
        }


        #define TEMPLATE_TU \
            template < typename T,  typename U,  subfail_if_neither_is_object_t<T,U> = 0 >

        TEMPLATE_TU friend Object operator + ( const T& t, const U& u ) { return do_op( PyNumber_Add        , Object{t}, Object{u} ); }
        TEMPLATE_TU friend Object operator - ( const T& t, const U& u ) { return do_op( PyNumber_Subtract   , Object{t}, Object{u} ); }
        TEMPLATE_TU friend Object operator * ( const T& t, const U& u ) { return do_op( PyNumber_Multiply   , Object{t}, Object{u} ); }
        TEMPLATE_TU friend Object operator / ( const T& t, const U& u ) { return do_op( PyNumber_TrueDivide , Object{t}, Object{u} ); }
        TEMPLATE_TU friend Object operator % ( const T& t, const U& u ) { return do_op( PyNumber_Remainder  , Object{t}, Object{u} ); }

        static bool do_cmp( const Object& t, const Object& u, int cmp ) {
            bool ret = PyObject_RichCompareBool( t.p, u.p, cmp );

            throw_if_pyerr(TRACE);
            return ret;
        }

        TEMPLATE_TU friend bool operator == (  const T& t, const U& u ) { return do_cmp( Object{t}, Object{u}, Py_EQ ); }
        TEMPLATE_TU friend bool operator != (  const T& t, const U& u ) { return do_cmp( Object{t}, Object{u}, Py_NE ); }
        TEMPLATE_TU friend bool operator >  (  const T& t, const U& u ) { return do_cmp( Object{t}, Object{u}, Py_GT ); }
        TEMPLATE_TU friend bool operator <  (  const T& t, const U& u ) { return do_cmp( Object{t}, Object{u}, Py_LT ); }
        TEMPLATE_TU friend bool operator >= (  const T& t, const U& u ) { return do_cmp( Object{t}, Object{u}, Py_GE ); }
        TEMPLATE_TU friend bool operator <= (  const T& t, const U& u ) { return do_cmp( Object{t}, Object{u}, Py_LE ); }

        Object do_ip( OpFunc& op_func, const Object& u ) {
            Object ret = op_func(p, u.p);
            throw_if_pyerr(TRACE);

            if( ret.p != p )
                *this = ret;

            return ret;
        }

        template<typename U> Object operator += ( const U& u ) { return do_ip( PyNumber_InPlaceAdd          , u ); }
        template<typename U> Object operator -= ( const U& u ) { return do_ip( PyNumber_InPlaceSubtract     , u ); }
        template<typename U> Object operator *= ( const U& u ) { return do_ip( PyNumber_InPlaceMultiply     , u ); }
        template<typename U> Object operator /= ( const U& u ) { return do_ip( PyNumber_InPlaceTrueDivide   , u ); }
        template<typename U> Object operator %= ( const U& u ) { return do_ip( PyNumber_InPlaceRemainder    , u ); }


        // !!! add more from https://docs.python.org/3/c-api/number.html
        #undef TEMPLATE_TU

        friend std::ostream& operator << ( std::ostream& outstream, const Object &ob )
        {
            return outstream << ob.dump_ascii();
        }

        /*
          Links:
            http://stackoverflow.com/questions/27168446/tidying-up-c-operator-overloads
            http://stackoverflow.com/questions/27228659/overloading-arithmetic-operations-for-c11-python-wrapper                <-- !!! ToDo: Add Answer
            http://stackoverflow.com/questions/27266393/design-pattern-for-structuring-operator-overloads-for-python-wrapper    <-- !!! ToDo: Add Answer
            http://stackoverflow.com/questions/27267210/implementing-template-classderivingfrombase-not
         */


        // - - - - - - -
#pragma mark METHODS

        // M E T H O D S

        Object keys()       const { return PyMapping_Keys  (p); }
        Object values()     const { return PyMapping_Values(p); }
        Object items()      const { return PyMapping_Items (p); } // list of (key, value) pairs

        //Py_ssize_t length()  const { return PyObject_Length(p); }
        // Queries
        Object      type()                      const { return PyObject_Type(p); }        // type object
        Object      str()                       const { return PyObject_Str (p); }
        Object      repr()                      const { return PyObject_Repr(p); }                  // string
        Object      dir()                       const { return PyObject_Dir (p); }                  // list
        std::string as_string()                 const { return static_cast<std::string>(str());  }

        Py_ssize_t reference_count()            const { return p ? p->ob_refcnt : 0; }


        bool   hasAttr( const std::string& s )  const { return         PyObject_HasAttrString(p,const_cast<char*>(s.c_str())) ? true : false; }
        Object getAttr( const std::string& s )  const { return charge( PyObject_GetAttrString(p,const_cast<char*>(s.c_str())) ); }

        Object getItem( const Object& key )     const { return PyObject_GetItem(p,*key); }

        long hashValue()                        const { return PyObject_Hash(p); }

        // convert to bool
        bool as_bool()                          const { return PyObject_IsTrue(p) != 0; }


        Object callMemberFunction( const std::string& f )                                       const { return (getAttr(f)) ()        ; }
        Object callMemberFunction( const std::string& f, const Object& args )                   const { return (getAttr(f)) (args)    ; }
        Object callMemberFunction( const std::string& f, const Object& args, const Object& kw ) const { return (getAttr(f)) (args,kw) ; }

        // int print( FILE *fp, int flags=Py_Print_RAW )
        //{
        //    return PyObject_Print( p, fp, flags );
        //}

        // identity tests
        bool is( PyObject* pother )     const { return p == pother;  }
        bool is( const Object& other )  const { return p == other.p; }
        bool isNull()                   const { return p == nullptr; }
        bool isNone()                   const { return p == Py_None; }
        bool isBoolean()                const { return PyBool_Check       (p); }
        bool isBytes()                  const { return PyBytes_Check      (p); }
        bool isString()                 const { return PyUnicode_Check    (p); }
        bool isTuple()                  const { return PyTuple_Check      (p); }
        bool isList()                   const { return PyList_Check       (p); }
        bool isDict()                   const { return PyDict_Check       (p); }
        bool isTrue()                   const { return PyObject_IsTrue    (p) != 0; }
        bool isCallable()               const { return PyCallable_Check   (p) != 0; }
        bool isNumeric()                const { return PyNumber_Check     (p) != 0; }
        bool isSequence()               const { return PySequence_Check   (p) != 0; }
        bool isMapping()                const { return PyMapping_Check    (p) != 0; } // WARNING: http://bugs.python.org/issue5945

        bool isType( const Object& t )  const { return type().p == t.p; }

        // Commands
        void setAttr( const std::string& s, const Object& value ) {
            ENSURE_OK( PyObject_SetAttrString( p, const_cast<char*>(s.c_str()), *value ) );
        }

        void delAttr( const std::string& s ) {
            ENSURE_OK( PyObject_DelAttrString( p, const_cast<char*>(s.c_str())         ) );
        }
        
        // PyObject_SetItem is too weird to be using from C++
        // so it is intentionally omitted.

        void delItem( const Object& key ) {
            //if( PyObject_DelItem( p, *key ) == -1 )
            // failed to link on Windows?
            THROW( "delItem failed" );
        }

        // Numeric interface
        Object operator+( ) const { return PyNumber_Positive(p); } // !!! check isNumeric ?
        Object operator-( ) const { return PyNumber_Negative(p); }
        Object abs      ( ) const { return PyNumber_Absolute(p); }



#pragma mark  C O N T A I N E R
    /*
      The problem of trying to have a generic Object class is that we can't really support Map and Vector
      functionality together. Because each container has its own different iterators.
        http://stackoverflow.com/questions/27726181/hybrid-listdict-container-in-c11
      I've opted for just supporting a Vector type container
      This allows fast-enumerating through myMapObj.keys()
      It does mean that we can't throw an Object containing a PyDict_Type through any of
      stdlib's Map processing algorithms. If anyone wants that, they would probably have to design
      a Dict : Object, and then maybe work out whether this machinery conflicts with the new machinery
      they would have to write etc.
     */

    private:
        // Was deriving from: public std::iterator< std::random_access_iterator_tag, Object >
        // Now we do that manually
        template< bool isConst >
        class iterator_base
        {
        private:
            using pointer           = Object*;
            using iterator_category = std::random_access_iterator_tag;

            using value_type      = Object;
            using reference       = value_type&;
            using difference_type = ptrdiff_t;

            friend class Object; // allow Object to see our data

            using I = iterator_base<isConst>;

            template <typename T>
            using MaybeConst = typename std::conditional< isConst, const T, T >::type;

            const Object&       m_ob;
            Py_ssize_t          m_index;

        public:
            iterator_base( const Object& ob, Py_ssize_t index ) : m_index{index}, m_ob{ ob }  { }

            MaybeConst<I&> operator++() { m_index++; return *this; }

            MaybeConst<I> operator++( int ) { I iter{ m_ob, m_index }; m_index++; return iter; }

            friend bool operator == ( const I& lhs, const I& rhs )  { return *lhs.m_ob == *rhs.m_ob  &&  lhs.m_index == rhs.m_index; };
            friend bool operator != ( const I& lhs, const I& rhs )  { return !(lhs == rhs); };

            MaybeConst<Object> operator*()  const {
                return m_ob[m_index];
            }

        };

    public:
        using value_type      = Object;
        using reference       = value_type&;
        using const_reference = const reference;
        using difference_type = ptrdiff_t;
        using size_type       = Py_ssize_t;
        using iterator        = iterator_base<false>;
        using const_iterator  = iterator_base<true>;

        Py_ssize_t              length()        const { return PyObject_Length(p); }  // was PySequence_Length
        Py_ssize_t              size()          const { return PyObject_Length(p); }  // was PySequence_Length
        Py_ssize_t              max_size()      const { return std::numeric_limits<Py_ssize_t>::max(); }
        bool                    empty()         const { return length()==0; }

        void        swap( Object& o )                 { Object tmp = o;  o = *this;  set_ptr(tmp.p); }
        friend void swap( Object& a, Object& b )      { a.swap(b); }

        iterator                begin()         const { return iterator{ *this, 0        }; }
        iterator                end()           const { return iterator{ *this, length() }; }

        //const const_iterator    cbegin()        const { return const_iterator{ *this, 0        }; }
        //const const_iterator    cend()          const { return const_iterator{ *this, length() }; }

    }; // End of class Object

    static inline Object to_tuple(PyObject* pyob) {
        return Object{ pyob ? charge(pyob) : PyTuple_New(0) };
    }
    static inline Object to_dict(PyObject* pyob) {
        return Object{ pyob ? charge(pyob) : PyDict_New() };
    }

#pragma mark = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =

    inline Object None()  { return Object{ charge( Py_None  ) }; }
    inline Object True()  { return Object{ charge( Py_True  ) }; }
    inline Object False() { return Object{ charge( Py_False ) }; }


} // namespace
