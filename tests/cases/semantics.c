#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdalign.h>

struct pair { long long a; double b; };
struct bits { unsigned a : 3; signed b : 6; unsigned c : 7; };

static struct pair transform(struct pair value, long long add)
{
	value.a += add;
	value.b *= 2;
	return value;
}

static double sum(int count, ...)
{
	va_list args;
	double value = 0;
	int i;

	va_start(args, count);
	for (i = 0; i < count; i++)
		value += va_arg(args, double);
	va_end(args);
	return value;
}

int main(void)
{
	struct pair value = transform((struct pair){ 0x123456789LL, 1.25 }, 11);
	struct bits bits = { 7, -17, 101 };
	alignas(16) unsigned char memory[32];
	int array[5] = { [2] = 73, [4] = -9 };
	int x = 3, total = 0, i;
	int (*indirect)(const char *, const char *) = strcmp;

	_Static_assert(sizeof(uint64_t) == 8, "64-bit integer");
	for (i = 0; i < 37; i++) {
		int n = i % 7 + 1;
		int local[n];
		local[n - 1] = i;
		total += local[n - 1];
	}
	printf("pair=%lld,%.2f varargs=%.2f\n", value.a, value.b,
		sum(5, 1.0, 2.25, 3.5, 4.75, 5.0));
	printf("bits=%u,%d,%u array=%d,%d scope=%d\n", bits.a, bits.b,
		bits.c, array[2], array[4], total);
	{
		int x = 19;
		printf("inner=%d ", x);
	}
	printf("outer=%d aligned=%d indirect=%d\n", x,
		(int)((uintptr_t)memory % 16), indirect("same", "same"));
	return value.a == 0x123456794LL && total == 666 ? 0 : 1;
}
