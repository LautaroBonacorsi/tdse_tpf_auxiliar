#ifndef TELEMETRY_H
#define TELEMETRY_H

void telemetry_init(void);
void telemetry_update(void);
void telemetry_send(const char *msg);

#endif /* TELEMETRY_H */
