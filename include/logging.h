#ifndef __LOGGING_H_

#define LOG_NONE  0
#define LOG_BASIC 1 // Log timings for full payload response
#define LOG_FULL  2 // Log individual payloads in workers (costly)

#ifndef LOG_LEVEL
#define LOG_LEVEL LOG_BASIC
#endif

#endif // __LOGGING_H