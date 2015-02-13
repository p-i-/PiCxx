#import test_funcmapper
#
#o = test_funcmapper.old_style_class()
#
#n = test_funcmapper.new_style_class()
#
#class Derived(test_funcmapper.new_style_class):
#    pass
#
#d = Derived()


print( '\n--- importing... ---' )
import test_funcmapper

print( '\n--- module func ---' )
test_funcmapper.func()
test_funcmapper.func( 4, 5 )
test_funcmapper.func( 4, 5, name=6, value=7 )


print( '\n--- old-style class function ---' )
o = test_funcmapper.old_style_class()
#print( dir( old_style_class ) )
print( '---' )
o.func_noargs()
o.func_varargs()
o.func_varargs( 4 )
o.func_keyword()
o.func_keyword( name=6, value=7 )
o.func_keyword( 4, 5 )
o.func_keyword( 4, 5, name=6, value=7 )


print( '\n--- new-style class function ---' )
n = test_funcmapper.new_style_class()
#print( dir(new_style_class) )
#_new_style_class.newAttribute = 5
#print(" _new_style_class.newAttribute = " + _new_style_class.newAttribute )
n.func_noargs()
n.func_varargs()
n.func_varargs( 4 )
n.func_keyword()
n.func_keyword( name=6, value=7 )
n.func_keyword( 4, 5 )
n.func_keyword( 4, 5, name=6, value=7 )

print( '\nTesting error-catching...' )

try:
    n.func_exception()
    print( '\nError: did not raised RuntimeError' )
    sys.exit( 1 )

except RuntimeError as e:
    print( 'SUCCESS! Raised %r' % (str(e),) )


print( '\n--- Derived func ---' )
class Derived(test_funcmapper.new_style_class):
    pass
    def __init__( self ):
        test_funcmapper.new_style_class.__init__( self )

    def derived_func( self ):
        print( '\nderived_func' )
#       print( vars(super()) )
        super().func_noargs()

    def func_noargs( self ):
        print( '\nderived func_noargs' )

d = Derived()
print( dir(d) )
d.derived_func()
d.func_noargs()
d.func_varargs()
d.func_varargs( 4 )
d.func_keyword()
d.func_keyword( name=6, value=7 )
d.func_keyword( 4, 5 )
d.func_keyword( 4, 5, name=6, value=7 )

print( d.value )
d.value = "a string"
print( d.value )
d.new_var = 42
d.new_var2 = 32768


n = None

