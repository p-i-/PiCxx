/*
  Object class
      In Python everything is a a PyObject.
      πCxx has an Object class that wraps a PyObject.
      It allows us to initialise and manipulate the underlying PyObject using a syntax
      that is tolerably close to Python's own awesomeness.

      The best way to understand what's going on is to look at the syntax, and 
      single-step into the line of interest.

  CHARGING
      Object takes care of encapsulating reference counting
      The main thing you have to remember is that if you initialise it with a PyObject,
      you have to ensure it is CHARGED, which you can do using the charge() function:
      
          PyObject* pyob = ...;
          Object foo = charge(pyob);

      Charge is the same as INCREF
      It's my own terminology
      Python talks about ownership.
      So say Python passes ownership to me, then it is INCREF-ing the object and it is 
      my duty to DECREF it. By my terminology, it is passing me a CHARGED pyob.
      Similarly if I need to pass ownership to Python, I must INCREF the pyob
      and it is Python's duty to DECREF it. By my terminology I am CHARGING it.

      The consumer of πCxx only need to worry about this in the aforementioned case.
 */

#include "Objects.hxx"

using namespace Py;


void test_ob() {
    Py_Initialize();

    if(1) {
        // creates a PyLong_Type
        XCOUT( Object{2} );

        XCOUT( Object{2}.dump_ascii() );


        {
            COUT( "-> illustrating caching of low value integers:" );
            Object x = 1; // Python caches PyLong_Type (on my implementation it's -5 through 256 but that's not guaranteed)
            COUT_DATA(x);  // 1's refcount will be > 100 as the runtime will be making use of it all over the place
            x += 1;
            COUT_DATA(x);
        }

        {
            COUT( "\n-> illustrating refcount when assigning:" );
            Object x = 1000*1000; // creates a PyLong_Type with value 1M -- now this WILL be a new pyobject
            COUT_DATA(x);

            Object y = x; // y now refers to the same object which now has refcount of 2
            COUT_DATA(y);

            y += 1; // integers are immutable, so Python creates a new object to hold this new value even though we did +=
            COUT_DATA(x);
            COUT_DATA(y);
        };

        XCOUT( Object{ std::string("foo") } );
        XCOUT( Object{ "bar" } ); // const char* relays to same c'tor, so still creates a PyUnicode_Type
        // ^ I had wanted const char* to create a PyBytes_Type and std::string to create a PyUnicode_Type
        // can't remember why that didn't work out, there was an issue with doing that
        // it's probably because we generally want to make a string, and Python won't convert
        // PyBytes_Type -> PyUnicode_Type

        XCOUT(( Object{ PyBytes_Type, "bar" } ));
        // ^ that's how you would go about creating a PyBytes_Type, in this case with the three elements
        // Note:
        //      MACRO(  X{a,b}  )  <-- 2 args: 'X(a' and 'b)'
        //      MACRO( (X{a,b}) )  <-- 1 argument, okay
        // () instead of {} would also solve the problem.

        XCOUT( std::string(Object{42}) ); // or (std::string)Object{42}, or static_cast...

        // construct PyUnicode_Type containing "42", convert to PyLong_Type, convert to int
        XCOUT( static_cast<int>( Object{"42"}.convert_to(PyLong_Type) )  ); // convert_to would throw if "xyz" i.e. non-numeric

        // however we can simplify this to:
        XCOUT( static_cast<int>( Object{"42"} ) );
        // .. as  static_cast<int> calls convert_to(PyLong_Type) and then converts that to a C++ int type
        // we could have used any other integral type (like unsigned long long)

        try{
            // Let's try something that fails, and check that an exception is correctly thrown
            // Should get "TypeError: unsupported operand type(s) for +: 'float' and 'str'"
            XCOUT( Object{3.14} + Object{"1"} );
            COUT( "ERROR! 3.14 + '1' should be raising an exception and it isn't!" );
        }
        catch( const Exception& t ) {
            std::cout << "Correctly caught Python error, as Python can't handle  3.14 + '1' \n"; 
        }
        catch (...) { 
            std::cout << "Caught WTF\n"; // Object's operator+ overload should have thrown an Exception object
        }
        PyErr_Clear();

        // test operator overloads
        XCOUT(       Object(PyComplex_Type,"3+4j")  ); // Object{ PyFoo_Type, bar } will first do Object{bar} and then convert_to(PyFoo_Type)
        XCOUT( 2   * Object(PyComplex_Type,"3+4j")  ); // Note () not {} as C++'s ancient/cronky/shit macro system can't handle nested {} with ","
        XCOUT( 2.0 * Object( std::complex<float> (1,2) )  );
        XCOUT(       Object( std::complex<double>(1,2) ) * Object(PyComplex_Type,"3+4j")  );

        XCOUT( std::complex<float> ( Object{"1+2j"} ) );
        XCOUT( std::complex<double>( Object{"1+2j"} ) ); // again, need () not {}

        XCOUT( Object( 'T', 1, 2.01, "three")    ); // tuple
        XCOUT( Object( 'L', 1, 2.01, "three")[1] ); // list, second item

        Object dict( 'D',  "k1", 1.1,  "k2", 666 ); // dict
        XCOUT( dict["k2"].str() ); // dict lookup, .str() illustrates that dict["k2"] is an object

        XCOUT( Object( 'D',  "k1", 1.1,  "k2", 666 )["k2"] ); // could have just done it in one line...

        XCOUT( Object('B', "abcde")[3] == int('d')  ?  "True" : "False" ); // bytes, 4th item

        /*
        Note:
          std::string s( Object("foo") ); // ok
          std::string s{ Object("foo") }; // ERROR!
         
         ^ This produces compiler error "No matching constructor for initialization of 'std::string'..."
           Because it is attempting to invoke std::string's initialiser-list constructor
           (list with one element)
           Starting to really hate initialiser lists, they really muck up using {} initialiser syntax
        */

        // test appending one list to another
        Object list( 'L', 1,2,3 );
        XCOUT( {
                list += Object( 'L', 4,5 );
                list;
            } );

        // test modifying list and fast enumeration
        XCOUT( {
                list[1] = 42;
                for(auto&& i : list)
                    std::cout << i << ", ";
                "\n" "woot";
            } );
            /*
                ^ list has a .begin() that returns an rvalue? you want auto&& i.
                auto will make a copy (which accepts rvalue), const auto& will bind to rvalue
                as will auto&&
                auto&& leaves i mutable
                auto& can't bind to an rvalue
             */
    }

    Py_Finalize();
}
