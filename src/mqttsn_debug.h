#ifndef MQTTSN_DEBUG_H_
#define MQTTSN_DEBUG_H_

/********** Debug control *************/

/* Set the debug level
   0: Disabled
   1: Errors only
   2: Everything
 */
#define MQTTSN_DEBUG_LEVEL             2

#if (MQTTSN_DEBUG_LEVEL == 0)
    
    #define MQTTSN_INFO_PRINT(...)
    #define MQTTSN_INFO_PRINTLN(...)
    #define MQTTSN_ERROR_PRINT(...)
    #define MQTTSN_ERROR_PRINTLN(...)
    
#else
    
    #include <stdio.h>
    #define MQTTSN_ERROR_PRINT(...)             printf(__VA_ARGS__)
    #define MQTTSN_ERROR_PRINTLN(fmt, ...)      printf(fmt "\r\n", ##__VA_ARGS__)
    
    #if (MQTTSN_DEBUG_LEVEL == 1)        
        #define MQTTSN_INFO_PRINT(...)
        #define MQTTSN_INFO_PRINTLN(...)
    #else
        #define MQTTSN_INFO_PRINT(...)          printf(__VA_ARGS__)
        #define MQTTSN_INFO_PRINTLN(fmt, ...)   printf(fmt "\r\n", ##__VA_ARGS__)
    #endif

#endif

#endif
