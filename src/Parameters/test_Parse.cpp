// See LICENSE_CELLO file for license and copyright information

/// @file     test_Parse.cpp
/// @author   James Bordner (jobordner@ucsd.edu)
/// @date     2009-10-06
/// @brief    Test program for reading in parameters then displaying them

#include "main.hpp" 
#include "test.hpp"

#include "parameters.hpp"

PARALLEL_MAIN_BEGIN
{
  PARALLEL_INIT;
  for (int i=1; i<PARALLEL_ARGC; i++) {
    const char * filename = PARALLEL_ARGV[i];
    FILE * fp = fopen (filename,"r");
    cello_parameters_read(filename,fp);
  }
    
  cello_parameters_print();
  PARALLEL_EXIT;
}
PARALLEL_MAIN_END
