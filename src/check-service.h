// check-service.h
#ifndef CHECK_SERVICE_H
#define CHECK_SERVICE_H

typedef enum {
    SERVICE_NOT_LAUNCHED,
    SERVICE_LAUNCHED
} ServiceState;

ServiceState checkservice(void);

#endif
