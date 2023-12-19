
typedef enum {
	RK_LOG_LEVEL_INFO,
	RK_LOG_LEVEL_WARNING,
	RK_LOG_LEVEL_ERROR,
} rk_log_level_t;

#ifdef NDEBUG
#define LOG_INFO(...)
#define LOG_WARNING(...)
#define LOG_ERROR(...)
#else
#define LOG_INFO(...) rk_log(RK_LOG_LEVEL_INFO, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_WARNING(...) rk_log(RK_LOG_LEVEL_WARNING, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERROR(...) rk_log(RK_LOG_LEVEL_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#endif

typedef unsigned long (*time_now_ms_t)(void);

void rk_init_logging(time_now_ms_t callback);
void rk_log(rk_log_level_t level, const char* file, int line, const char* fmt, ...);
