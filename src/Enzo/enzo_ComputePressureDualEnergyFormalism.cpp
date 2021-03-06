// See LICENSE_ENZO file for license and copyright information

/// @file      enzo_ComputePressureDualEnergyFormalism.cpp
/// @author    Greg Bryan
/// @date      November, 1994
/// @brief     (COMPUTE THE PRESSURE FIELD AT THE GIVEN TIME) - DUAL ENERGY
///
/// Compute the pressure at the requested time.  The pressure here is
/// just the ideal-gas equation-of-state (dual energy version).

#include "cello.hpp"

#include "enzo.hpp"
 
//----------------------------------------------------------------------
 
int EnzoBlock::ComputePressureDualEnergyFormalism
(enzo_float time, 
 enzo_float *pressure,
 bool comoving_coordinates)
{

  /* declarations */
 
  int i, size = 1;
 
  /* Compute the size of the grid. */
 
  const int in = cello::index_static();

  for (int dim = 0; dim < GridRank[in]; dim++)
    size *= GridDimension[dim];
 
  Field field = data()->field();

  enzo_float * density         = (enzo_float *) field.values("density");
  enzo_float * internal_energy = (enzo_float *) field.values("internal_energy");

  /* Loop over the grid, compute the thermal energy, then the pressure,
     the timestep and finally the implied timestep. */
 
  /* special loop for no interpolate. */
 
  if (time == this->time()) {
 
    for (i = 0; i < size; i++) {
      pressure[i] = (Gamma[in] - 1.0) * density[i] *
	internal_energy[i];
      pressure[i] = std::max(pressure[i],pressure_floor[in]);
    }
 
  } else {
 
    /* general case: */

    double time_prev = field.history_time(1);
    double time_now = this->time();
    double coef = (time - time_prev) / (time_now - time_prev);
    
    enzo_float * density_old         =
      (enzo_float *) field.values("density",1);
    enzo_float * internal_energy_old =
      (enzo_float *) field.values("internal_energy",1);

    for (i = 0; i < size; i++) {
 
      enzo_float ge = coef*internal_energy[i]
	+       (1.0-coef)*internal_energy_old[i];
      enzo_float de = coef*density[i]
	+    	(1.0-coef)*density_old[i];
 
      pressure[i] = (Gamma[in] - 1.0)*de*ge;
      pressure[i] = std::max(pressure[i],pressure_floor[in]);
 
    }
  }
 
  /* Correct for Gamma from H2. */
 
  if (MultiSpecies[in] > 1) {
 
    enzo_float TemperatureUnits = 1, number_density, nH2, GammaH2Inverse,
      GammaInverse = 1.0/(Gamma[in]-1.0), x, Gamma1, temp;
 
    enzo_float * species_De    = (enzo_float *) field.values("species_De");
    enzo_float * species_HI    = (enzo_float *) field.values("species_HI");
    enzo_float * species_HII   = (enzo_float *) field.values("species_HII");
    enzo_float * species_HeI   = (enzo_float *) field.values("species_HeI");
    enzo_float * species_HeII  = (enzo_float *) field.values("species_HeII");
    enzo_float * species_HeIII = (enzo_float *) field.values("species_HeIII");
    // enzo_float * species_HM    = (enzo_float *) field.values("species_HM");
    enzo_float * species_H2I   = (enzo_float *) field.values("species_H2I");
    enzo_float * species_H2II  = (enzo_float *) field.values("species_H2II");
    // enzo_float * species_DI    = (enzo_float *) field.values("species_DI");
    // enzo_float * species_DII   = (enzo_float *) field.values("species_DII");
    // enzo_float * species_HDI   = (enzo_float *) field.values("species_HDI");
 
    /* Find the temperature units if we are using comoving coordinates. */
 
    EnzoUnits * units = enzo::units();

    units->set_current_time (time);
    
    TemperatureUnits = units->temperature();
 
    for (i = 0; i < size; i++) {
 
      number_density =
	  0.25 * (species_HeI[i]  + 
		  species_HeII[i] +
		  species_HeIII[i] )
	+        (species_HI[i]   + 
		  species_HII[i]  +
		  species_De[i]);
 
      nH2 = 0.5*(species_H2I[i]  + species_H2II[i]);
 
      /* First, approximate temperature. */
 
      if (number_density == 0)
	number_density = number_density_floor[in];
      temp = MAX(TemperatureUnits*pressure[i]/(number_density + nH2), 
		 (enzo_float)(1.0));
 
      /* Only do full computation if there is a reasonable amount of H2.
	 The second term in GammaH2Inverse accounts for the vibrational
	 degrees of freedom. */
 
      GammaH2Inverse = 0.5*5.0;
      if (nH2/number_density > 1e-3) {
	x = temp/6100.0;
	if (x < 10.0)
	  GammaH2Inverse = 0.5*(5 + 2.0 * x*x * exp(x)/pow(exp(x)-1.0,2));
      }
 
      Gamma1 = 1.0 + (nH2 + number_density) /
	             (nH2*GammaH2Inverse + number_density * GammaInverse);
	
      /* Correct pressure with improved Gamma. */
 
      pressure[i] *= (Gamma1 - 1.0)/(Gamma[in] - 1.0);
 
    } // end: loop over i
 
  } // end: if (MultiSpecies > 1)
 
  return ENZO_SUCCESS;
}
