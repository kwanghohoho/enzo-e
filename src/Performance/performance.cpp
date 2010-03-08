/** 
 *********************************************************************
 *
 * @file      
 * @brief     
 * @author    
 * @date      
 * @ingroup
 * @note      
 *
 *--------------------------------------------------------------------
 *
 * DESCRIPTION:
 *
 *    
 *
 * CLASSES:
 *
 *    
 *
 * FUCTIONS:
 *
 *    
 *
 * USAGE:
 *
 *    
 *
 * REVISION HISTORY:
 *
 *    
 *
 * COPYRIGHT: See the LICENSE_CELLO file in the project directory
 *
 *--------------------------------------------------------------------
 *
 * $Id$
 *
 *********************************************************************
 */

/** 
 *********************************************************************
 *
 * @file      performance.cpp
 * @brief     
 * @author    James Bordner (jobordner@ucsd.edu)
 * @ingroup   Performance
 * @date      
 * @note      
 *
 * DESCRIPTION 
 * 
 *    
 *
 * PACKAGES
 *
 *    
 * 
 * INCLUDES
 *  
 *    
 *
 * PUBLIC FUNCTIONS
 *  
 *    
 *
 * PRIVATE FUCTIONS
 *  
 *    
 *
 * $Id$
 *
 *********************************************************************
 */

#include "cello.h"

#include "error.hpp" 
#include "memory.hpp"
#include "performance.hpp"

Performance::Performance 
(
 unsigned num_attributes,
 unsigned num_counters,
 unsigned num_groups,
 unsigned num_regions)
  : counters_(),
    num_attributes_(num_attributes),
    attribute_names_(NULL),
    monotonic_attributes_(NULL),
    monotonic_attribute_values_(NULL),
    num_counters_(num_counters),
    counter_names_(NULL),
    num_groups_(num_groups),
    current_group_(0),
    group_names_(NULL),
    num_regions_(num_regions),
    region_names_(NULL),
    current_region_(0)
/**
 *********************************************************************
 *
 * @param         
 * @return        
 *
 * Create a Performance object with space reserved for the specified
 * number of attributes and the specified number of counters
 *
 *********************************************************************
 */
{
  Memory * memory = Memory::get_instance();

  memory->new_group (component_performance, "Performance");
  memory->begin_group(component_performance);

  attribute_names_              = new std::string [ num_attributes_ + 1];
  counter_names_                = new std::string [ num_counters_ + 1];
  group_names_                  = new std::string [ num_groups_ + 1];
  region_names_                 = new std::string [ num_regions_ + 1];
  monotonic_attributes_         = new bool [num_attributes + 1];
  for (unsigned i=0; i<=num_attributes; i++) {
    monotonic_attributes_[i]       = false;
  }
  monotonic_attribute_values_   = new int [num_attributes + 1];
  for (unsigned i=0; i<=num_attributes; i++) {
    monotonic_attribute_values_[i] = 0;
  }

  // Create initial Counters object

  counters_.push_back(new Counters(num_attributes,num_counters));

  memory->end_group(component_performance);
}

Performance::~Performance()
/**
 *********************************************************************
 *
 * @param         
 * @return        
 *
 * Destroy a Performance object
 *
 *********************************************************************
 */
{
  Memory * memory = Memory::get_instance();

  memory->begin_group(component_performance);

  delete [] attribute_names_;
  delete [] counter_names_;
  delete [] group_names_;
  delete [] region_names_;
  delete [] monotonic_attributes_;
  delete [] monotonic_attribute_values_;

  for (unsigned i=0; i<counters_.size(); i++) {
    delete counters_.at(i);
  }

  memory->end_group(component_performance);

}

void Performance::new_attribute(unsigned    id_attribute, 
				std::string attribute_name,
				bool        is_monotonic)
/**
 *********************************************************************
 *
 * @param         
 * @return        
 *
 * Create a new attribute
 *
 *********************************************************************
 */
{
  new_item_ (item_attribute, 
	     "attribute", 
	     id_attribute, 
	     attribute_name,
	     attribute_names_, 
	     num_attributes_);

  monotonic_attributes_[id_attribute] = true;

}

int Performance::get_attribute(unsigned id_attribute)
/**
 *********************************************************************
 *
 * @param         
 * @return        
 *
 * Return the value of an attribute
 *
 *********************************************************************
 */
{
  return 0;
}

void Performance::set_attribute(unsigned id_attribute)
/**
 *********************************************************************
 *
 * @param         
 * @return        
 *
 * Assign a value to an attribute
 *
 *********************************************************************
 */
{
}

void Performance::new_group(unsigned    id_group, 
			    std::string group_name)
/**
 *********************************************************************
 *
 * @param         
 * @return        
 *
 * Create a new group
 *
 *********************************************************************
 */
{
  new_item_ (item_group, 
	     "group", 
	     id_group, 
	     group_name,
	     group_names_, 
	     num_groups_);
}

int Performance::get_group(unsigned id_group)
/**
 *********************************************************************
 *
 * @param         
 * @return        
 *
 * Return the value of the currently active group
 *
 *********************************************************************
 */
{
  return 0;
}

void Performance::set_group(unsigned id_group)
/**
 *********************************************************************
 *
 * @param         
 * @return        
 *
 * Assign a value to a group
 *
 *********************************************************************
 */
{
}

void Performance::begin_group(unsigned group_id)
/**
 *********************************************************************
 *
 * @param         
 * @return        
 *
 * Define the start of a group
 *
 *********************************************************************
 */
{
  
  if ( current_group_ ){
    char message [ ERROR_MESSAGE_LENGTH ];
    sprintf (message, "Performance group started when one already active");
    WARNING_MESSAGE("Performance::begin_group",message);
  }

  current_group_ = group_id;

}

void Performance::end_group(unsigned id_group)
/**
 *********************************************************************
 *
 * @param         
 * @return        
 *
 * Define the end of a group
 *
 *********************************************************************
 */
{
  if (id_group != current_group_) {
    char message [ ERROR_MESSAGE_LENGTH ];
    sprintf (message, "Mismatch between begin_group(%s) and end_group(%s)",
	     group_names_[current_group_].c_str(),
	     group_names_[id_group].c_str());
    WARNING_MESSAGE("Performance::end_group",message);
  }

  current_group_ = 0;

}

void Performance::new_region(unsigned    id_region, 
			     std::string region_name)
/**
 *********************************************************************
 *
 * @param         
 * @return        
 *
 * Create a new region
 *
 *********************************************************************
 */
{
  new_item_ (item_region, 
	     "region", 
	     id_region, 
	     region_name,
	     region_names_, 
	     num_regions_);
}

int Performance::get_region(unsigned id_region)
/**
 *********************************************************************
 *
 * @param         
 * @return        
 *
 * Return the value of the currently active region
 *
 *********************************************************************
 */
{
  return 0;
}

void Performance::set_region(unsigned id_region)
/**
 *********************************************************************
 *
 * @param         
 * @return        
 *
 * Assign a value to a region
 *
 *********************************************************************
 */
{
}

void Performance::start_region(unsigned region_id)
/**
 *********************************************************************
 *
 * @param         
 * @return        
 *
 * Define the start of a region
 *
 *********************************************************************
 */
{
}

void Performance::stop_region(unsigned region_id)
/**
 *********************************************************************
 *
 * @param         
 * @return        
 *
 * Define the end of a region
 *
 *********************************************************************
 */
{
}


void Performance::new_counter(unsigned    id_counter,
			      std::string counter_name)
/**
 *********************************************************************
 *
 * @param         
 * @return        
 *
 * Create a new user counter
 *
 *********************************************************************
 */
{
  new_item_ (item_counter, 
	     "counter", 
	     id_counter, 
	     counter_name,
	     counter_names_, 
	     num_counters_);
}

type_counter Performance::get_counter(unsigned id_counter)
/**
 *********************************************************************
 *
 * @param         
 * @return        
 *
 * Return the value of a counter
 *
 *********************************************************************
 */
{
  return 0;
}

void Performance::set_counter(unsigned          id_counter,
			      type_counter value)
/**
 *********************************************************************
 *
 * @param         
 * @return        
 *
 * Assign a value to a user counter
 *
 *********************************************************************
 */
{
}

void Performance::increment_counter(unsigned          id_counter,
				    type_counter value)
/**
 *********************************************************************
 *
 * @param         
 * @return        
 *
 * Increment a user counter
 *
 *********************************************************************
 */
{
}

void Performance::flush()
/**
 *********************************************************************
 *
 * @param         
 * @return        
 *
 * Flush data to disk
 *
 *********************************************************************
 */
{
}


void Performance::new_item_ 
(
 enum_item_type  item_type,
 std::string item_type_name,
 unsigned id_item, 
 std::string item_name,
 std::string * item_names,
 unsigned num_items_
)
/**
 *********************************************************************
 *
 * @param         
 * @return        
 *
 * Create a new attribute
 *
 *********************************************************************
 */
{
  if ( ! (1 <= id_item && id_item <= num_items_)) {

    char message [ ERROR_MESSAGE_LENGTH ];
    sprintf (message, "id_item = %d out of range [0,%d] for item %s",
	     id_item, num_items_, item_name.c_str());
    WARNING_MESSAGE("Performance::start_item",message);

  } else {

    item_names[id_item] = item_name;

  }
}

