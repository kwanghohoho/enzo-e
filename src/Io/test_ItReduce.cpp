// See LICENSE_CELLO file for license and copyright information

/// @file     test_ItReduce.cpp
/// @author   James Bordner (jobordner@ucsd.edu)
/// @date     2011-09-26
/// @brief    Test program for the ItReduce family of classes

#include "main.hpp"
#include "test.hpp"

#include "io.hpp"

PARALLEL_MAIN_BEGIN
{

  PARALLEL_INIT;

  unit_init(0,1);

  unit_class("ItReduceAvg");

  ItReduce * it_reduce = ItReduce::create(reduce_avg);

  unit_func ("create");

  unit_assert (it_reduce != NULL);

  it_reduce->next(1.0);
  it_reduce->next(2.0);
  it_reduce->next(3.0);
  it_reduce->next(4.0);
  it_reduce->next(5.0);

  unit_func ("value");
  unit_assert (it_reduce->value() == 3.0);

  unit_func ("first");
  it_reduce->first();

  it_reduce->next(5.0);
  it_reduce->next(3.0);
  unit_assert (it_reduce->value() == 4.0);

  unit_finalize();

  PARALLEL_EXIT;
}

PARALLEL_MAIN_END
