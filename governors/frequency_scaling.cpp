#include <math.h>
#include "frequency_scaling.h"

/* Calculate the new idx value for the frequencies based on
 * the Controller Output value.
 *
 * controller_out: Float number that indicates the aggregated control error.
 * desired_value: Float number that indicates the desired aggregated value.
 * MaxFrequencyCounter: Integer that defines the Limit of the indices in the freq set.
 * FrequencyCounter: Current indice in the freq set.
 * 
 * Output: Integer that is used to index the new frequency.
*/
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