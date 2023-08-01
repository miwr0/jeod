/**
 * @addtogroup Models
 * @{
 * @addtogroup Utils
 * @{
 * @addtogroup Orientation
 * @{
 *
 * @file models/utils/orientation/src/euler_angles.cc
 * Define Orientation methods related to computing Euler angles.
 */

/*******************************************************************************

Purpose:
  ()

 

*******************************************************************************/

// System includes
#include <cmath>

// JEOD includes
#include "utils/math/include/matrix3x3.hh"
#include "utils/message/include/message_handler.hh"

// Model includes
#include "../include/orientation.hh"
#include "../include/orientation_messages.hh"


//! Namespace jeod
namespace jeod {

/**
 * The threshold for determining whether a gimbal lock condition exists.
 * Gimbal lock occurs when sin(theta) (aerodynamics Euler sequences) or
 * cos(theta) (astronomical sequences) is very close to -1 or +1.
 * This static variable quantifies the meaning of 'very close'.
 */
double Orientation::gimbal_lock_threshold = 1e-13;


/**
 * Contains data needed to construct a transformation matrix given a sequence
 * of Euler angles and to extract a sequence of Euler angles from a matrix.
 * See Orientation::compute_euler_angles_from_matrix for details.
 */
struct EulerInfo {

   /**
    * The axes about which the rotations are performed in the order in which
    * the rotations are performed, with X=0, Y=1, Z=2. For example, an XYZ or
    * roll pitch yaw sequence is {0,1,2} while a ZXZ sequence is {2,0,2}.
    */
   unsigned int indices[3]; //!< trick_units(--)

   /**
    * The initial element of the sequence for aerodynamics sequences,
    * but the index of the omitted axis for astronomical sequences.
    * For example, the omitted axis in a ZXZ sequence is Y=1.
    */
   unsigned int alternate_x; //!< trick_units(--)

   /**
    * The final element of the sequence for aerodynamics sequences,
    * but the index of the omitted axis for astronomical sequences.
    */
   unsigned int alternate_z; //!< trick_units(--)

   /**
    * Indicates whether the 3-axis rotation sequence generated by replacing the
    * final element of the sequence with the one axis not specified by the
    * first two elements of the sequence is an even (true) permutation or odd
    * permutation (false) of XYZ.
    * The alternative 3-axis sequence is identical to the original sequence in
    * the case of aerodynamics sequences. The astronomical ZXZ sequence becomes
    * ZXY via this replacement rule. Since ZXY is an even permutation of XYZ,
    * the is_even_permutation member for a ZXZ sequence is true.
    */
   bool is_even_permutation; //!< trick_units(--)

   /**
    * True if the sequence is an aerodynamics sequence such as XYZ;
    * false for an astronomical sequence such as ZXZ.
    */
   bool is_aerodynamics_sequence; //!< trick_units(--)
};


/**
 * Contains twelve EulerInfo objects, one per each of the JEOD Euler sequences.
 * The elements are arranged per the values of the Orientation::EulerSequence
 * enumeration items.
 */
static const EulerInfo Euler_info[12] = {
   //  seq       altx  altz  right    aero
   { {0, 1, 2},   0,    2,    true,   true }, // EulerXYZ
   { {0, 2, 1},   0,    1,   false,   true }, // EulerXZY
   { {1, 2, 0},   1,    0,    true,   true }, // EulerYZX
   { {1, 0, 2},   1,    2,   false,   true }, // EulerYXZ
   { {2, 0, 1},   2,    1,    true,   true }, // EulerZXY
   { {2, 1, 0},   2,    0,   false,   true }, // EulerZYX
   { {0, 1, 0},   2,    2,    true,  false }, // EulerXYX
   { {0, 2, 0},   1,    1,   false,  false }, // EulerXZX
   { {1, 2, 1},   0,    0,    true,  false }, // EulerYZY
   { {1, 0, 1},   2,    2,   false,  false }, // EulerYXY
   { {2, 0, 2},   1,    1,    true,  false }, // EulerZXZ
   { {2, 1, 2},   0,    0,   false,  false }  // EulerZYZ
};



/**
 * Compute the left transformation quaternion from the Euler sequence.
 * The quaternion is formed by generating a sequence of three simple
 * quaternions corresponding to the three rotations. The composite
 * quaternion is the reverse-order product of these three simple quaternions.
 * \param[in] euler_sequence Euler sequence
 * \param[in] euler_angles Euler angles\n Units: r
 * \param[out] quat Resultant quaternion
 */
void
Orientation::compute_quaternion_from_euler_angles (
   EulerSequence euler_sequence,
   const double euler_angles[3],
   Quaternion & quat)
{
   // Validate the value of the euler_sequence member.
   // Note that an invalid value means the object is left unchanged.
   if ((euler_sequence < EulerXYZ) || (euler_sequence > EulerZYZ)) {
      MessageHandler::error (
         __FILE__, __LINE__, OrientationMessages::invalid_enum,
         "The euler_sequence data member has not been set or is invalid; "
         "value=%d",
         static_cast<int> (euler_sequence));
      return;
   }

   const unsigned int * axes = Euler_info[euler_sequence].indices;
   Quaternion q[3], q21;

   for (int ii = 0; ii < 3; ii++) {
      double htheta = 0.5*euler_angles[ii];
      double cosht = std::cos (htheta);
      double sinht = std::sin (htheta);
      q[ii].scalar = cosht;
      q[ii].vector[axes[ii]] = -sinht;
   }

   q[2].multiply (q[1], q21);
   q21.multiply (q[0], quat);
   quat.normalize ();

   return;
}


/**
 * Compute the transformation matrix from the Euler sequence.
 * The matrix is formed by generating a sequence of three simple transformation
 * matrices corresponding to the three rotations. The composite transformation
 * matrix is the reverse-order product of these three simple matrices.
 * \param[in] euler_sequence Euler sequence
 * \param[in] euler_angles Euler angles\n Units: r
 * \param[out] trans Resultant transformation matrix
 */
void
Orientation::compute_matrix_from_euler_angles (
   EulerSequence euler_sequence,
   const double euler_angles[3],
   double trans[3][3])
{
   // Validate the value of the euler_sequence member.
   // Note that an invalid value means the object is left unchanged.
   if ((euler_sequence < EulerXYZ) || (euler_sequence > EulerZYZ)) {
      MessageHandler::error (
         __FILE__, __LINE__, OrientationMessages::invalid_enum,
         "The euler_sequence data member has not been set or is invalid; "
         "value=%d",
         static_cast<int> (euler_sequence));
      return;
   }
   const unsigned int * axes = Euler_info[euler_sequence].indices;
   double m[3][3][3], m21[3][3];

   for (int ii = 0; ii < 3; ii++) {
      Matrix3x3::initialize (m[ii]);
      double sin_theta = std::sin (euler_angles[ii]);
      double cos_theta = std::cos (euler_angles[ii]);
      switch (axes[ii]) {
      case 0:
         m[ii][0][0] = 1.0;
         m[ii][1][1] =  cos_theta;
         m[ii][1][2] =  sin_theta;
         m[ii][2][1] = -sin_theta;
         m[ii][2][2] =  cos_theta;
         break;
      case 1:
         m[ii][1][1] = 1.0;
         m[ii][0][0] =  cos_theta;
         m[ii][0][2] = -sin_theta;
         m[ii][2][0] =  sin_theta;
         m[ii][2][2] =  cos_theta;
         break;
      case 2:
         m[ii][2][2] = 1.0;
         m[ii][0][0] =  cos_theta;
         m[ii][0][1] =  sin_theta;
         m[ii][1][0] = -sin_theta;
         m[ii][1][1] =  cos_theta;
         break;
      default:
         break;
      }
   }

   Matrix3x3::product (m[2], m[1], m21);
   Matrix3x3::product (m21, m[0], trans);

   return;
}


/**
 * Extract an Euler sequence from the transformation matrix.
 * A transformation matrix constructed from an XYZ Euler sequence
 * is of the form @f[
 *    \left[\array{ccc}
 *       \cos\psi\cos\theta & \cdots & \cdots \\
 *      -\sin\psi\cos\theta & \cdots & \cdots \\
 *       \sin\theta  & -\cos\theta\sin\phi & \cos\theta\cos\phi
 *    \endarray\right]
 * @f]
 * Note that the [2][0] element of the matrix depends on theta only.
 * The other two elements of the leftmost column are simple terms that depend on
 * theta and psi only, and the other two elements of the bottommost row are
 * simple terms that depend on theta and phi only.
 * Those five elements are the key to extracting an XYZ Euler sequence from a
 * transformation matrix.
 * The same principle applies to all twelve of the Euler sequences:
 * Five key elements contain all of the information needed to extract the
 * desired sequence. The location and form of those key elements of course
 * depends on the sequence.
 *
 * A problem arises in the above when cos(theta) is zero, or nearly so. This
 * siutation is called 'gimbal lock'. Those four elements used to determine phi
 * and psi are zero or nearly so. Fortunately That ugly stuff isn't so ugly in
 * the case of gimbal lock. Once again looking at the matrix generated from an
 * XYZ Euler sequence, when theta=pi/2 the matrix becomes @f[
 *    \left[\array{ccc}
 *        0 & \sin(\phi+\psi) & -cos(\phi+\psi) \\
 *        0 &  \cos(\phi+\psi) & \sin(\phi+\psi) \\
 *        1 & 0 & 0
 *    \endarray\right]
 * @f]
 * In this case there no way to determine both phi and psi; all that can be
 * determined is their sum. One way to overcome this problem is to arbitrarily
 * set one of those angles to an arbitrary value such as zero. That is the
 * approach used in this method. This arbitrary setting enables an XYZ Euler
 * sequence to be extracted from the matrix even in the case of gimbal lock.
 * The same principle once again applies to all twelve sequences.
 *
 * In summary, for a transformation matrix corresponding to an XYZ sequence,
 *  - The [2][0] element of the matrix specifies theta.
 *  - The [1][0] and [0][0] elements of the matrix specify psi.
 *  - The [2][1] and [2][2] elements of the matrix specify phi.
 *    These psi and phi values are valid only when gimbal lock is not present.
 *  - The [1][2] and [1][1] elements of the matrix specify phi in the
 *    case of gimbal lock.
 *
 * Extending this analysis to the remaining eleven sequences provides the
 * essential information needed to extract the desired Euler angles from a
 * transformation matrix. This information is captured in the EulerInfo
 * array Euler_info defined at the head of this file. With a
 * reference <tt>info</tt> to the appropriate element of this array,
 *  - The [info.indices[2]][info.indices[0]] element of the matrix
 *    specifies the angle theta.
 *  - The [info.indices[1]][info.indices[0]] and
 *        [info.alternate_x][info.indices[0]] elements of the matrix
 *    specify the angle psi when gimbal lock is not present.
 *  - The [info.indices[2]][info.indices[1]] and
 *        [info.indices[2]][info.alternate_z] elements of the matrix
 *    specify the angle phi when gimbal lock is not present.
 *  - The [info.indices[1]][info.alternate_z] and
 *        [info.indices[1]][info.indices[1]] elements of the matrix
 *    specify angle phi when gimbal lock is present.
 *
 * \par Assumptions and Limitations
 *  - To within numerical accuracy, the transformation matrix in the
 *     Orientation object @e is a proper transformation matrix:
 *      - The magnitude of each row and column vector is nearly one.
 *      - The inner product of any two different rows / two different columns of
 *        the matrix nearly zero.
 *      - The determinant of the matrix is nearly one.
 *      - An element whose value is outside the range [-1,1] is only slightly
 *        outside that range and the deviation is numerical.
 * \param[in] trans Transformation matrix
 * \param[in] euler_sequence Euler sequence
 * \param[out] euler_angles Resultant Euler angles\n Units: r
 */
void
Orientation::compute_euler_angles_from_matrix (
   const double trans[3][3],
   EulerSequence euler_sequence,
   double euler_angles[3])
{
   // Validate the value of the euler_sequence member.
   // Note that an invalid value means the object is left unchanged.
   if ((euler_sequence < EulerXYZ) || (euler_sequence > EulerZYZ)) {
      MessageHandler::error (
         __FILE__, __LINE__, OrientationMessages::invalid_enum,
         "The euler_sequence data member has not been set or is invalid; "
         "value=%d",
         static_cast<int> (euler_sequence));
      return;
   }
   const EulerInfo & info = Euler_info[euler_sequence];  // See above

   double phi;            // First Euler angle
   double theta;          // Second Euler angle
   double psi;            // Third Euler angle

   double cos_phi;        // cos(phi) * extra stuff * sign
   double sin_phi;        // sin(phi) * extra stuff * sign
   double cos_psi;        // cos(psi) * extra stuff * sign
   double sin_psi;        // sin(psi) * extra stuff * sign
   double theta_val;      // sin(theta), -sin(theta) or cos(theta)
   double alt_theta_val1; // Conceptually, 1-theta_val^2
   double alt_theta_val2; // Conceptually, 1-theta_val^2
   double alt_theta_val;  // Conceptually, 1-theta_val^2


   // Extract the key elements from the matrix assuming that this is not
   // a gimbal lock situation.
   // The trans[info.indices[2]][info.indices[0]] element is
   //  *  sin(theta) for even permutation aerodynamics sequences,
   //  * -sin(theta) for odd permutation aerodynamics sequences, or
   //  *  cos(theta) for all astronomical sequences.
   theta_val = trans[info.indices[2]][info.indices[0]];

   // Get terms containing the sines and cosines of the first and third Euler
   // angle times sin(theta) or cos(theta) (and sometimes negated).
   sin_phi = trans[info.indices[2]][info.indices[1]];
   cos_phi = trans[info.indices[2]][info.alternate_z];
   sin_psi = trans[info.indices[1]][info.indices[0]];
   cos_psi = trans[info.alternate_x][info.indices[0]];

   // Compute alternative theta values based on the above four terms.
   alt_theta_val1 = std::sqrt (sin_phi*sin_phi + cos_phi*cos_phi);
   alt_theta_val2 = std::sqrt (sin_psi*sin_psi + cos_psi*cos_psi);
   alt_theta_val  = 0.5 * (alt_theta_val1 + alt_theta_val2);

   // theta_val is -sin(theta) for odd permutation aerodynamics sequences.
   // Negate to get rid of the minus sign.
   if (info.is_aerodynamics_sequence && (! info.is_even_permutation)) {
      theta_val = -theta_val;
   }


   // Compute theta.
   if (alt_theta_val < std::fabs (theta_val)) {
      double alt_theta = std::asin (alt_theta_val);

      if (info.is_aerodynamics_sequence) {
         if (theta_val < 0.0) {
            theta = -0.5*M_PI + alt_theta;
         }
         else {
            theta =  0.5*M_PI - alt_theta;
         }
      }
      else {
         if (theta_val < 0.0) {
            theta = M_PI - alt_theta;
         }
         else {
            theta =  alt_theta;
         }
      }
   }
   else {
      if (info.is_aerodynamics_sequence) {
         theta = std::asin (theta_val);
      }
      else {
         theta = std::acos (theta_val);
      }
   }


   // Not in a gimbal lock situation:
   // Compute sin_phi, cos_phi, sin_psi, and cos_psi.
   //  - These are not the sine and cosine of the Euler angles phi and psi.
   //    Rather, they are sin(phi) etc. scaled by a common positive number.
   //  - Key elements of the matrix are of the form
   //      sign*cos(theta)*sin(phi) etc.
   //  - The trick then is to find these key elements and then ensure that
   //    sign*cos(theta) is positive for each of the four values.
   //  - The specifics of the sign correction are depend on whether the
   //    sequence is an aerodynamical or astronomical sequence.
   if (alt_theta_val > gimbal_lock_threshold) {

      // Correct signs for aerodynamic sequences.
      if (info.is_aerodynamics_sequence) {

         // The sine values have the wrong sign for right-handed aero sequences.
         if (info.is_even_permutation) {
            sin_phi = -sin_phi;
            sin_psi = -sin_psi;
         }
      }

      // Correct signs for astronomical sequences.
      else {

         // A cosine term has the wrong sign in the case of astro sequences.
         // The term with the wrong sign is cos_phi for even permutations but
         // cos_psi for odd permutations.
         if (info.is_even_permutation) {
            cos_phi = -cos_phi;
         }
         else {
            cos_psi = -cos_psi;
         }
      }

      // Compute phi and psi.
      phi = std::atan2 (sin_phi, cos_phi);
      psi = std::atan2 (sin_psi, cos_psi);
   }


   // In a gimbal lock situation:
   // All that can be determine is the difference between / sum of phi and psi.
   // Arbitrarily setting psi to zero resolves this issue.
   else {
      // Compute sin_phi and cos_phi with the same constraint on the
      // common scale factor as outlined above.
      sin_phi = trans[info.indices[1]][info.alternate_z];
      cos_phi = trans[info.indices[1]][info.indices[1]];

      // The sine value has the wrong sign for odd sequences.
      if (! info.is_even_permutation) {
         sin_phi = -sin_phi;
      }

      // Compute phi and psi.
      phi = std::atan2 (sin_phi, cos_phi);
      psi = 0.0;
   }

   // Save the computed angles in the object.
   euler_angles[0] = phi;
   euler_angles[1] = theta;
   euler_angles[2] = psi;

   return;
}

} // End JEOD namespace

/**
 * @}
 * @}
 * @}
 */
