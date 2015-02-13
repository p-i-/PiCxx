
void test_ob();
void test_funcmapper();
void test_prompt( int argc, const char* argv[] );

int main(int argc, const char * argv[])
{
    // test PyCXX's wrappers for stock Python objects
    if ((1))
        test_ob();

    // test extension module, old-style extension class, new-style extension class
    //    For each object, test calling through from Python to consumer-made C++ methods,
    if((1))
        test_funcmapper();

    // launch a Python prompt
    if((0))
        test_prompt( argc, argv );

    return 0;
}


