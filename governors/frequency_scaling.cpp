#include <math.h>
#include "frequency_scaling.h"

int freq_upscaling(float controller_out, float desired_value, int MaxFrequencyCounter, int FrequencyCounter)
{
    if (controller_out < 0) {
        int idx = 0;
        idx = FrequencyCounter + round(-controller_out / desired_value) * 2 + 1;

        if (idx < MaxFrequencyCounter)
        {
            return idx;
        }
        else
        {
            return MaxFrequencyCounter;
        }
    }
    else
    {
        return FrequencyCounter;
    }
}