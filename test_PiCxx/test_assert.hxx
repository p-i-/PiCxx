class TestError
{
public:
    TestError( const std::string &description )
        : m_description( description )
    { }

    ~TestError()
    { }

    std::string m_description;
};

// B gets cast to V
template <typename B, typename V>
static void test_assert( std::string description, B benchmark, V value )
{
    std::ostringstream full_description;

    full_description << description
        //<< " : { " << "benchmark"      << ", " << "value"          << " }"
        << "   { " << typeid(B).name() << ", " << typeid(V).name() << " }"
        << " = { " << benchmark        << ", " << value            << " }";

    // WTF https://gist.github.com/p-i-/ce6f074e90b65dfc91d0
    // http://ideone.com/gbyU6Y

    if( benchmark==value && value==benchmark )
        std::cout << "    PASSED: " << full_description.str() << std::endl;
    else
        throw TestError( full_description.str() );
}
