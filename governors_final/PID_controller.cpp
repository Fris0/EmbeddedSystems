#include "PID_controller.h"

float error_prior = 0;
float integral_prior = 0;

/* Calculate Controller Ouput and return the solution.
 *
 * desired: Desired value in int.
 * actual: Measured value in int.
 * iteration_time: Latency for proccesing a frame in float.
 * Kp, Ki, Kd: The gain values set in the Governor.
 * 
 * Output: A float which indicates the change required by the
 *         actuator.
*/
float controller_output(float desired, float actual, float iteration_time,
                        float K_P, float K_I, float K_D)
{
    float error = desired - actual;
    float integral = integral_prior + error * (iteration_time / 1000);
    float derivative = (error - error_prior) / (iteration_time / 1000);
    float output = K_P * error + K_I * integral + K_D * derivative;

    error_prior = error;
    integral_prior = integral;

    return output;
}