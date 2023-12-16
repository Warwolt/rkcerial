
typedef enum {
    RK_LOG_LEVEL_INFO,
    RK_LOG_LEVEL_WARNING,
    RK_LOG_LEVEL_ERROR,
} rkcerial_log_level_t;

#define LOG_INFO(...) rkcerial_log_info(__FILE__, __LINE__, __VA_ARGS__)

void rkcerial_init_logging(void);
void rkcerial_printf(const char* fmt, ...);
void rkcerial_log_info(const char* file, int line, const char* fmt, ...);
