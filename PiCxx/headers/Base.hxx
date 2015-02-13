#pragma once

// if 'PICXX_DEBUG=0' not supplied to compiler, assume we are in debug mode
#ifndef PICXX_DEBUG
#   define PICXX_DEBUG (1)
#endif
#include "Base/Debug.hxx"
#include "Base/Config.hxx"
#include "Base/Exception.hxx"

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
