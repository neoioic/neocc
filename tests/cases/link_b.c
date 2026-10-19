#include <stdio.h>
extern int (*function_pointer)(int);
extern const char *message;
extern int shared[4];
int calculate(int value)
{
	return shared[value & 3] * 13;
}
int main(void)
{
	printf("%s=%d\n", message, function_pointer(2));
	return function_pointer(3) == 143 ? 0 : 1;
}
