# πCxx
Version 0.0  
by π (pi@pipad.org)  
20 Jan 2015


## What is πcxx?

πcxx is a bridge between C++ and Python

Currently it only supports C++ >= 11, and Python >= 3. It wouldn't take to much work to provide Python 2.x support, I just haven't done it as I'm targeting 3.x in my own work.

Currently I'm only providing an XCode (OSX) commandline project to demonstrate it. However it should be straightforward to compile the demo on any platform with a C++ >= 11 compiler, and Python >= 3.


### Extending vs Embedding
There are two scenarios where you might want to use it:

   - ***Extending*** Python  
     Your app is in Python and you wish to make (using C++) library objects for it to use. (*e.g. a module containing functions & classes for performing fast math*)

     ```python
     # Your Python app
     import mymodule  # written in C++ using πcxx
     
     # variable
     print( mymodule.some_variable )
     
     # optional args/keywords
     ret = mycxxmodule.some_function("foo", "bar" = 42)
     
     cxxclass = mymodule.some_class;
     myinst = cxxclass();
     
     myinst.some_attribute = 42
     
     def mycallback():
         print "callback"
     
     def mypyclass:
         def do_print():
             print("mypyclass member func")
     
     # C++ object would even be able to call these asynchronously
     myinst.some_method(mypyclass, mycallback)
     ```
     
     ```c++
     // Your C++ library -- compile this as a library and place 
     //  it somewhere in Python's search path
     
     // Upon 'import x' Python will search for some PyInit_x function
     extern "C" PyObject* PyInit_mymodule() {
         return *foomodule::reset(); // boilerplate
     }
     
     #include "ExtModule.hxx"
     
     class foomodule : public ExtModule<foomodule> {
         :
         // would need to provide some_variable, some_function,
         //  some_class (with some_attribute, some_method)
         //
         // For now I won't go into what the boilerplate for providing 
         //  what these would look like
     }
     
     ```
   
   - ***Embedding*** Python  
     You want your C++ app to run a Python VM,  
     *e.g. you want to script your gamelogic in Python*

     ```c++ game
     // C++ Game
     void gamelogic_loader() {
         // Crank up Python runtime
         // we'll need a matching Py_Finalize() before we quit
         Py_Initialize();
         
         Py::run_file( "./py/gamelogic.py" );
         
         // or ...
         Object logic = PyImport_AddModule(""./py/gamelogic.py"");
         
         // COUT is a debug helper macro
         COUT( logic.some_attribute );
         
		  logic.some_func(); // could supply args&kwds
		  Object inst = logic.some_class();
		  inst.some_method();
     }
         
     ```
     ```python
     # gamelogic.py
     
     some_attribute = [1,2,3]  # a list, maybe
     
     def some_func():
         print( "some_func()" )
     
     def some_class:
         def some_method()
     ```
     
Extending and embedding actually boil down to the same thing. In both cases there is a Python runtime, and in both cases πcxx is interacting with it via its C-API (which includes placing trampolines in the slots of its function-pointer tables).

*(A* ***trampoline*** *here is a C function that bounces to a C++ object instance method. Trampolining is necessary because Python's slots can only accept C function pointers).*

The only difference is that if we're *embedding*, then ***we*** need to crank up the runtime (and make sure that in-so-doing, it loads any modules we've written). 

On the other hand, if we're *extending*, we will be compiling to make a library, and we need to ensure Python will be able to look in this library and see (and load) the modules we've written.

Apart from that, everything is interchangeable.

### Object class: Mirroring Python's PyObject in C++
In Python everything is a PyObject.  
πcxx has a corresponding Object class that wraps a PyObject.  
Everything you can do with a PyObject in Python, you can do in C++ using Object.

Python            | C++
---               | ---
`x = [1,2,3]`     | `Object x{ 'L', 1,2,3 }`
`x += ["four"]`   | `x += Object{'L', "four"}`

Have a look at `test_objects.cxx` to see how incredibly versatile this Object class is. 

Doing say `Object x = 3.14, y = 42, z = "foo";` will create a `PyFloat_Type`, `PyLong_Type`, `PyUnicode_Type` respectively.

#### The *CHARGE* rule 
You can initialise an Object with a PyObject. If so, you need to make sure this PyObject is ***charged***.  That's my own terminology, the conventional terminology is: "a new reference". i.e. the PyObject has been INCREF-d without any future corresponding DECREF. 

This is because Object's destructor will discharge (i.e. DECREF) it.

πcxx actually supplies a `PyObject* charge(PyObject* pyob)` function that charges (INCREF-s) and returns pyob.  So do:

    Object x{ charge(pyob) }; // or...
    Object x = charge(pyob);  // (it's all the same to C++11)

This is superpowerful, because it means this is the only thing you need to remember regarding reference counting. Be always feeding a *charged* PyObject into an Object, and it will always work.

List items, Dictionary keys and values -- feed Object-s into these. If you feed something that isn't an Object, like `mydict[foo]=...` then it will attempt to initialise an Object with `foo`.  So if `foo` is of type `PyObject`, make certain that it is *CHARGED*, as per the rule!

#### Recursive/Auto Proxy
One very nice πcxx feature is that `somedict["some_key"]` resolves into an Object, so you can do `a[b]=c` or even `a[b].c=d`.  (`c()` also ok).  The key need not be a string, it can be any type including Object. This is one significant improvement upon PyCxx, which resolves `a[b]` into a Proxy object which then tries to emulate an Object. So `a[b].c` means that the Proxy class needs to provide a `c` variable or method that forwards to and a corresponding `Object.c`.  

I've never come across this design pattern before. The original Proxy pattern that PyCXX uses is documented by Scott Meyers. However, I am feeding it back into itself like a Klein bottle, which requires some cunning with `mutable`. So Recursive/Auto Proxy seems like a reasonable term.

### Other similar libraries
I'm aware of two other open-source libraries that do the same thing: PyCxx and Boost::Python.

πcxx is a complete rewrite of [PyCxx](http://sourceforge.net/projects/cxx/). 
Motivation is given [below](#Motivation).  You could also maybe think of it as Boost::Python without requiring Boost, although I haven't explored Boost::Python, so I can't say how similar they are.

= = = = = = = = = = = = = = = = = = = = = = = = = = = =

## Build stuff:

Dropping the code into an empty Xcode project, I needed to do the following:
    
   XCode project build settings:
       Other Linker Flags: -lpython3.4.1_OSX
       Library Search Paths: ./Libs
    
   Also look in test_funcmapper()


## QuickStart:

This code contains an XCode project that should work on OSX.
 The code contains its own libpython Library and Python headers,
 so it shouldn't need any faffling around with Linker flags.
 It works on my Yosemite MacBook Air.

To get it working under Linux/Windows, you'll need to supply your own LibPython3.x
 You may get away with using my supplied Python headers.
 You would probably have to look through the filesystem below to figure out
 how to get it spinning.  Please TALK TO ME, so that I can get the next version
 working out of the box across major platforms.


## Contact:

Please feel welcome to get in touch.

pi@pipad.org
IRC: Server:FreeNode, Channel:#pi

= = = = = = = = = = = = = = = = = = = = = = = = = = = =

## License:

   - Non-commercial  
     You are free to do whatever you like.

   - Commercial  
     You have to donate something you consider appropriate.  Cash.  Kittens.  A punch in the face.  Up to you -- surprise me!  At least donate an email saying hello.  (paypal would be a nice surprise: sunfish7@gmail.com)

π  
11 Feb 2015


## <a name="Motivation"></a> Motivation for rewriting PyCXX:

I spent some time researching the various possibilities for embedding Python in C++
(because I want to use Python in JUCE which is a C++ framework).

   - SWIG looked messy -- it is an interface generator.
   - Boost.Python looked right, but requires boost.
   - PyCXX looked perfect.

However, I couldn't see how to use it.  So I tried to understand the code, which did my head in.

That is to say it requires an unnecessarily high mental capacity to understand it. Someone with a particularly high mental capacity I'm sure can figure it out and work with it.  But for me to figure it out I had to rewrite it in a way that makes sense to me.

At time of writing it's taken me close to 4 months, and I've had a tremendous amount of expert-level help from IRC and Stack Overflow (although I did have to learn C++ and Python at the same time).

PyCxx...

   - appears to be ~20 years old.
   - restricts itself to C++9x compliancy and appears to support obscure/obsolete compilers
   - has huge amounts of duplication
      - the whole library itself is split into two wings (for Python 2.x and 3.x)
      - 50 or so separate trampoline functions (one for each of PyTypeObject's function-pointer slots)
      - 3 different mechanisms for trampolining method calls (old-style class, new style class, module)
      - pretty much every combination of operator overrides provided manually for PyObject wrapper class
   - ...

It looks as though someone patched it up for Python3 and then (someone else?) bolted the new patched version together with the original version, using:

```
// foo.hxx
#ifdef Py2
#   include "py2/foo.hxx"
#else
#   include "py3/foo.hxx"
#endif
```

πcxx

   - uses C++11 constructs (pretty much all of them in fact)
   - C++11 compliant
   - thoroughly commented
   - zero duplication
   - manages a seamless wrapping of PyObject
   - maybe 10x fewer LOC (once you've taken the comments out)

However, it isn't tried and tested, it won't run on pre-11 compilers, and it may have a larger executable size.  Also it currently supports Python3.

= = = = = = = = = = = = = = = = = = = = = = = = = = = =

## Internals:

Note that the documentation currently doesn't have a clear separation between "how to use it" (which should be about a page) and "how it works internally" (which could easily be a whole book).

You should be able to see how to use it by looking through the test files. I've tried to minimally-demonstrate every aspect.

If you would like help, get in touch! Once I understand where people get stuck, I will be able to provide better documentation.

The remainder of this document is a mix of internal / external.  For the deep internals, documentation is provided in the code.

= = = = = = = = = = = = = = = = = = = = = = = = = = = =

## File structure:

    pi@piBookAir.local ~ /Users/pi/Dev/JUCE
    ls -1 -R πcxx/

    πcxx.xcodeproj
    Libs
        libpython3.4.1_OSX.dylib
        python3.4.1_OSX
            Python-ast.h
            Python.h
            abstract.h
            accu.h
            asdl.h
            :
            unicodeobject.h
            warnings.h
            weakrefobject.h

    PiCXX
        doc.txt
        notes

        Src
            Exception.cxx

        headers
            Base.hxx
            Base
                Config.hxx
                Debug.hxx
                Exception.hxx

            Objects.hxx
            ExtObj.hxx
            ExtObj
                Bridge.hxx
                ExtObjBase.hxx
                FuncMapper.hxx
                TypeObject.hxx
                ExtObject.hxx
                OldStyle.hxx
                NewStyle.hxx

            ExtModule.hxx

    test_PiCXX
        main.cpp
        test_assert.hxx
        test_funcmapper.cxx
        test_funcmapper.py
        test_prompt.cpp

= = = = = = = = = = = = = = = = = = = = = = = = = = = =

## Notes on file structure:

- - - - - - - - - - - - - - 

    πcxx.xcodeproj
    Libs
        libpython3.4.1_OSX.dylib
        python3.4.1_OSX
            Python-ast.h
            Python.h
            abstract.h
            accu.h
            asdl.h
            :
            unicodeobject.h
            warnings.h
            weakrefobject.h

Need to link to a libpython3.x

I haven't designed πcxx to support Python 2.x (although it shouldn't be much extra work, the basic structure wouldn't change).  

I installed Python 3.4.1 using Homebrew and located the .dylib and copied it into my source tree, renaming it to `libpython3.4.1_OSX.dylib`

Also I located the folder containing the Python headers, and again copied it to my source tree.

I'm copying things into my source tree so that: 

   - no relative vs absolute path issues  
   - no long filenames  
   - source tree should be complete on a different OSX machine

- - -

    PiCXX
        doc.txt
        notes
        Src
            Exception.cxx

`Exception` uses `Object` stuff, and `Object` uses `Exception` stuff, so one of them has to bite the bullet and separate its definitions into a `.cxx`.
I chose `Exception` so as to keep `Object` tidy (it's a much bigger file so it's more important to keep it tidy, `Exception` is auxiliary / support)

Other than that, the entire library is headers only. The good thing about that is it makes the code easier to read as you don't have to jump around so much.

- - -

       headers
           Base.hxx
           Base
               Config.hxx
               Debug.hxx
               Exception.hxx

Base.hxx includes all the headers in /Base in the order in which they are listed.
(Used to be several until I did some pruning)

- - -

           Objects.hxx

`Objects.hxx` includes `Base.hxx`

If you just want to make use of stock Python objects, e.g. use a Python Dictionary object, just include Objects.hxx
(but I can't imagine any practical use case like this).

The `test_objects.cxx` demo does exactly this. Maybe have a look at it before progressing...

- - -

           ExtObj.hxx
           ExtObj
               Bridge.hxx
               ExtObjBase.hxx
               FuncMapper.hxx
               TypeObject.hxx
               ExtObject.hxx
               OldStyle.hxx
               NewStyle.hxx

If you need extension objects, just include ExtObj.hxx.  That will include Objects.hxx and everything in /ExtObj in the correct order (as shown).

Be aware that Python has old style and new style classes.  New style came in with 2.2?  Everything is new style for 3.x  

Why am I still supporting old-style then?  Well, maybe one day someone (maybe me) will extend this to support Python 2.x.  It doesn't seem to be dying...

So both OldStyle and NewStyle derive from ExtObject, which itself derives from ExtObjBase.  I've added that extra layer just to explicitly separate out the parts of the base that require CRTP

If you look at ExtObjBase, you will see it has a ton of virtual methods -- each one corresponds to a slot on the function-pointer table of a PyTypeObject (look in TypeObject.hxx)

For a custom extension object, every time Python runtime makes a new instance of it, in C++ land we must make an instance of a corresponding C++ class (deriving from OldStyle or NewStyle)

When the Python runtime invokes some slot from this PyObject's PyTypeObject, (i.e. the slot contains a pointer to a function, so say Python runtime calls this function) this must result in a corresponding function getting invoked on this C++ object

Bridge is the structure that binds the PyObject to the C++ object.  You will notice that Bridge is only used new style classes.  Old-style classes have PyObject as base-class, so the same memory location doubles as the PyObject and the bass class of the C++ object.

 New style classes can't use this trick as the size of the PyObject isn't guaranteed, because for new style classes, we allow within Python derivation. And the derived class may have a bigger footprint.

***TODO**: currently the new-style class still has a PyObject base which is unused. **Get rid of this!***

- - -

           ExtModule.hxx

If you need an extension module, include only ExtModule.hxx, which will include ExtObj.hxx.  And I can't think of any situation where you wouldn't do this.  You stick your extension objects in an extension module. From Python you import the module and can then access the contained objects.

So what does FuncMapper do?  I'm documenting it here because it applies to both extension modules and extension classes.  It's possible to write functions for an extension module or an extension object.

So the user, in Python, does something like `myModule.myFunc()` of `myObj.myFunc(arg,kw)` (etc), and Python will perform a lookup on `myFunc` and invoke its associated function pointer. πcxx needs to supply a function that will trampoline to a corresponding method in the module/object's C++ class.

FuncMapper handles this trampolining.  
The mechanism is slightly different for {old-style classes & modules} and {new-style classes}, but there is enough in common to warrant a single mechanism.

- - -

    test_PiCXX
        helper
            test_assert.hxx
        main.cpp
        test_objects.cxx
        test_funcmapper.cxx
        test_funcmapper.py
        test_prompt.cpp

Test suite!  At the moment this is not very comprehensive, but it should give some idea how to use πcxx.  It's probably best to start here when browsing through the source code.

test_assert.hxx is a helper (currently unused)

main.cpp allows you to toggle which of the tests you wish to run

test_objects.cxx -- it's important to understand this one, because everything else makes use of this Object class.

test_funcmapper.* tests extension module and classes (old&new style).
Initialisation, trampolining of function calls, destruction.

test_prompt.cpp creates a interactive Python terminal prompt in XCode's console output Sometimes it's helpful to spawn this prompt in the middle of some other test.  This way we can inspect the state of the Python Runtime.


## Walkthroughs:

Search the project for: `MARKER_STARTUP_`

And you will get a walk-through for what happens when you create an extension module containing an extension object.

Here is a quick summary that should make sense once you have followed the walk-through.

Start-up sequence:

   1. ...
    
      1. Basically you register a function that will get run when Python encounters `import foo`

      2. This function calls `reset()` on the module, which initialises the module

   2.  The consumer needs to supply the module with a `register_methods_and_classes()` method in which they must register all functions and extension classes.

   3.  For each extension class that is registered:

      1.  A `one_time_setup()` method is called on it (3.1a old-style, or 3.1b new-style), in which the consumer must register all functions for that class.

      2.  A `PyTypeObject` is constructed during this `one_time_setup()`

      3.  This `PyTypeObject` contains several tables of slots.  We are expected to fill a slot with a function-pointer if we wish that slot to be enabled.  So for example, if Python wants to access a `foo` attribute, it will invoke the function pointer at the `tp_getattr` slot passing a `PyUnicode_Type` (with data  `foo`) as a parameter. πcxx provides its own functions for some slots and also allows the consumer to provide their own slot functions.

This won't yet makes sense, until you understand how Python uses `PyTypeObject`, which is a separate walk-through.

- - -

I think the concept of this kind of walk-through is a good one, because the way we read is linear, whereas execution hops all over the place.  So we should have a style of documentation that matches this.

Other possible walk-throughs would be:

   - Everything that happens when Python executes `myObj.myMethod()` or `myObj.someAttribute` i.e. trampolining
   
   - Auto proxy / recursive proxy / self proxy.  
     It's a design pattern I've made myself, so I'm not sure I'm the best name yet. Basically when you do `foo[bar]`, `[]` is overloaded, so we can mimic Python's dictionary (or list) access syntax.  
     It's harder to allow `foo[bar] = quux`, however a Proxy can solve this
     It is trickier still to allow `foo[bar].quux = ...` because in this case the proxy object needs to be the original `Object` class, and you end up sawing the branch you are sitting on.  I finally solved this using a trick involving `mutable`.

If anyone is interested, get in touch and I will explain.  However, I'm not going to get myself in a tangle explaining all of this to my imaginary friend.


## Thanks
Special thanks to the wonderful people on Freenode, especially Yhg1s on #python and hs_ & cbreak on ##c++. Also to the Stack Overflow community, notably Piotr S.


## Final notes:
In a few places I've demonstrated some C++ constructs using geordi
Google "Geordi Eelis".  You can use Geordi for testing out C++ constructs on the FreeNode IRC server, #geordi

I recommend looking in `test_prompt.cpp` first, then looking in main, then looking at `test_funcmapper.*`.  Setting breakpoints and single-stepping through the code should reveal how the library works.

However, it's very difficult for me (now that I understand the problem) to provide good documentation.  Because reading and writing documentation is a linear process, And something like this is a connected web of moving parts.  So it's a challenge to present everything in some sensible order that builds up from the ground without any holes.  I'm hoping that in the future, other people seeking to understand the library will shine some light through these holes so that they can be patched up.

A lot of the documentation needs reworking, putting in the right places.
There is a lot of duplication, sometimes I say this same thing in five different places.  I've tried to err on the side of over-documentation.  This is good for learning.

Another thing I've done is print console output in many places.  So by looking at this output together with the code it's possible to see what the code is doing.  Again, much tidying and improving could be done.

I have currently linked in every Stack Overflow question I asked, even though many of these links can probably be taken out at some stage soon.