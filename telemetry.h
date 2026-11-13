#ifndef ALLEY_TELEMETRY_H
#define ALLEY_TELEMETRY_H
#include <stdbool.h>

// true if the telemetry submission succeeded, false if it failed or was not attempted.
bool Telemetry_SubmitRun(int score, float seconds, int coins, const char* cause);
// number of runs successfully uploaded to the telemetry server.
long Telemetry_GetUploadedCount(void);
long Telemetry_GetFailedCount(void);
#endif