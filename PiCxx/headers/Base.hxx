#pragma once

// if 'PICXX_DEBUG=0' not supplied to compiler, assume we are in debug mode
#ifndef PICXX_DEBUG
#   define PICXX_DEBUG (1)
#endif
#include "Base/Config.h"
#include "Base/Debug.h"
#include "Base/Exception.hxx"
#include "Base/File.h"

