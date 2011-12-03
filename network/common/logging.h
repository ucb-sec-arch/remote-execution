#ifndef LOGGING_H
#define LOGGING_H

#include <sys/time.h>

static inline uint64_t get_system_time()
{
    struct timeval tv; 
    uint64_t secs, usecs;
    gettimeofday(&tv, NULL);
    secs = tv.tv_sec;
    usecs = tv.tv_usec;

    return (secs * 1000000 + usecs);
}

static inline void log_stuff(char* msg)
{
    printf("%s at %llu\n", msg, get_system_time());
}

#endif /* LOGGING_H */
