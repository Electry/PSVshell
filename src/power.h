#ifndef _POWER_H_
#define _POWER_H_

typedef enum {
    PSVS_POWER_PLAN_SAVER,
    PSVS_POWER_PLAN_BALANCED,
    PSVS_POWER_PLAN_PERFORMANCE,
    PSVS_POWER_PLAN_MAX
} psvs_power_plan_t;

bool psvs_power_cpu_raise_freq(int power_plan, int freq, int peak_usage, int avg_usage);
bool psvs_power_cpu_lower_freq(int power_plan, int freq, int peak_usage);

#endif