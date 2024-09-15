#ifndef __DEBUG_H__
#define __DEBUG_H__

#define DEBUG_INFO(args...)                                                        \
    do                                                                             \
    {                                                                              \
        char b__[1024];                                                            \
        sprintf(b__, args);                                                        \
        fprintf(stderr, "[%s,%s,%d] : %s", __FILE__, __FUNCTION__, __LINE__, b__); \
    } while (0)
#endif