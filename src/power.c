#include <stdbool.h>

#include "power.h"

bool psvs_power_cpu_raise_freq(int power_plan, int freq, int peak_usage, int avg_usage) {
    switch (power_plan)
    {
        case PSVS_POWER_PLAN_SAVER:
            if(freq <= 111 && peak_usage >= 55)
                return true;
            if(freq <= 222 && peak_usage >= 70)
                return true;
            if(freq == 333 && (peak_usage >= 85 || avg_usage >= 55))
                return true;
            if(freq == 444 && (peak_usage >=90 || avg_usage >= 62))
                return true;
            break;

        case PSVS_POWER_PLAN_BALANCED:
            if(freq <= 111 && peak_usage >= 50)
                return true;
            if(freq <= 222 && peak_usage >= 60)
                return true;
            if(freq == 333 && (peak_usage >= 80 || avg_usage >= 50))
                return true;
            if(freq == 444 && (peak_usage >=85 || avg_usage >= 60))
                return true;
            break;

        case PSVS_POWER_PLAN_PERFORMANCE:
            if(freq <= 111 && peak_usage >= 50)
                return true;
            if(freq <= 222 && peak_usage >= 55)
                return true;
            if(freq == 333 && (peak_usage >= 75 || avg_usage >= 50))
                return true;
            if(freq == 444 && (peak_usage >=80 || avg_usage >= 60))
                return true;
            break;
        
        default:
            break;
    }
    return false;
}

bool psvs_power_cpu_lower_freq(int power_plan, int freq, int peak_usage) {
    switch (power_plan)
    {
        case PSVS_POWER_PLAN_SAVER:
            if (freq == 500 && peak_usage < 85)
                return true;
            if (freq < 500 && peak_usage < 80)
                return true;
            if (freq < 333 && peak_usage < 75)
                return true;
            if (freq < 222 && peak_usage < 60)
                return true;
            if (freq < 111 && peak_usage < 50)
                return true;
            break;

        case PSVS_POWER_PLAN_BALANCED:
            if (freq == 500 && peak_usage < 75)
                return true;
            if (freq < 500 && peak_usage < 70)
                return true;
            if (freq < 333 && peak_usage < 60)
                return true;
            if (freq < 222 && peak_usage < 50)
                return true;
            if (freq < 111 && peak_usage < 45)
                return true;
            break;

        case PSVS_POWER_PLAN_PERFORMANCE:
            if (freq == 500 && peak_usage < 70)
                return true;
            if (freq < 500 && peak_usage < 65)
                return true;
            if (freq < 333 && peak_usage < 55)
                return true;
            if (freq < 222 && peak_usage < 50)
                return true;
            if (freq < 111 && peak_usage < 45)
                return true;
            break;
        
        default:
            break;
    }
    return false;
}