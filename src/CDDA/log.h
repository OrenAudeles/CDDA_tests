#include <stdio.h>
#include <vector>

struct log_message{
	char str[80];
};

typedef std::vector<log_message> Log;
static Log log;

inline void push_log(const char* fmt, ...){
	log_message item = {0};

	va_list arg;
	va_start(arg, fmt);
	vsnprintf(item.str, 80, fmt, arg);
	va_end(arg);

	log.push_back(item);
}

inline void dump_log(void){
	for (log_message& item : log){
		printf("LOG >> %s\n", item.str);
	}
}