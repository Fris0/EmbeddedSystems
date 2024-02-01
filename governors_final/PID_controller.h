#ifndef PIDCONTROLLER_H
#define PIDCONTROLLER_H

extern float error_prior;
extern float integral_prior;

float controller_output(float desired, float actual, float iteration_time,
                        float K_P, float K_I, float K_D);

#endif