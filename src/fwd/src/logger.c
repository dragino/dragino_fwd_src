#include <stdarg.h> 

#include "logger.h"
#include "gwcfg.h"

DECLARE_GW;

void lgw_log(int FLAG, const char *format, ...) {  
    if (FLAG & GW.log.debug_mask) {                        
        char buffer[PRINT_SIZE];
        va_list args;  
        va_start(args, format); 
        int numChars = vsnprintf(buffer, PRINT_SIZE, format, args);  
  
        if (numChars >= (int)PRINT_SIZE) {  
            buffer[PRINT_SIZE - 1] = '\0'; 
            fprintf(stderr, "[WARNING~][LOGGER] Output truncated due to buffer size limit.\n");  
        }  
        va_end(args); 
        printf("%s", buffer);
    }
}  

