/**
 * @addtogroup Models
 * @{
 * @addtogroup Interactions
 * @{
 * @addtogroup RadiationPressure
 * @{
 *
 * @file models/interactions/radiation_pressure/src/radiation_params.cc
 * A virtual base class for radiation facet parameters, used to create interaction
 * facets for radiation in the InteractionSurfaceFactorys
 */

/************************** TRICK HEADER***************************************
PURPOSE:
    ()

REFERENCE:
    (((None)))

ASSUMPTIONS AND LIMITATIONS:
      ((None))

Library dependencies:
    ((radiation_params.cc)
     (interactions/thermal_rider/src/thermal_params.cc)
     (utils/surface_model/src/facet_params.cc))


*******************************************************************************/

/*  JEOD includes */

/* Model structure includes */
#include "../include/radiation_params.hh"

//! Namespace jeod
namespace jeod {

/**
 * Constructor for RadiationParams
 */
RadiationParams::RadiationParams (
   void)
{
   albedo  = 0.0;
   diffuse = 0.0;
}

/**
 * Destructor for RadiationParams
 */
RadiationParams::~RadiationParams (
   void)
{

   // empty for now

}

} // End JEOD namespace

/**
 * @}
 * @}
 * @}
 */
