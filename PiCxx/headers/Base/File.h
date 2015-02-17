#pragma once

namespace Py
{
    inline void run_file( const char* filestring )
    {
        FILE* file = fopen(filestring,"r");
        
        COUT( "\n = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = \n" );
        COUT( "Executing File:" << filestring );

        COUT( PyRun_SimpleFile(file, filestring) );
        
        fclose( file );
    }
}
