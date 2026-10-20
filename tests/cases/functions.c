#include <stdio.h>
#include <stdarg.h>
#include <string.h>

struct triple {
	long long x, y, z;
};

static struct triple make(long long value)
{
	struct triple result = { value, value + 1, value + 2 };

	return result;
}

static long long combine(struct triple a, struct triple b, struct triple c,
	struct triple d, struct triple e, struct triple f, struct triple g,
	struct triple h, struct triple i, struct triple j)
{
	return a.x + b.x + c.x + d.x + e.x + f.x + g.x + h.x + i.x +
		j.x + a.y + j.z;
}

static struct triple recursive(int n)
{
	struct triple previous;

	if (!n)
		return make(0);
	previous = recursive(n - 1);
	return make(previous.x + 1);
}

static inline struct triple inline_make(long long value)
{
	return make(value + 20);
}

static double sum(int count, ...)
{
	va_list args;
	double total = 0;

	va_start(args, count);
	while (count--)
		total += va_arg(args, double);
	va_end(args);
	return total;
}

static int old_style(n, value)
int n;
double value;
{
	return n + (int)value;
}

static int twice(int n)
{
	return n * 2;
}

static int thrice(int n)
{
	return n * 3;
}

static int (*pick(int index))(int)
{
	if (index)
		return thrice;
	return twice;
}

static int vla_arg(int n, int array[n])
{
	int copy[n];

	copy[n - 1] = array[n - 1];
	return copy[n - 1] + n;
}

static const char *function_name(void)
{
	return __func__;
}

int main(void)
{
	struct triple (*creator)(long long) = make;
	long long many = combine(creator(1), creator(2), creator(3), creator(4),
		creator(5), creator(6), creator(7), creator(8), creator(9), creator(10));
	struct triple nested = recursive(7);
	struct triple inlined = inline_make(3);
	double varied = sum(3, 1.5, 2.0, 3.0);
	int old = old_style(3, 4.5);
	int callback = pick(1)(4);
	int array[] = { 1, 2, 5 };
	int vla = vla_arg(3, array);
	const char *name = function_name();

	printf("struct=%lld,%lld,%lld var=%.1f old=%d callback=%d vla=%d name=%s\n",
		many, nested.x, inlined.x, varied, old, callback, vla, name);
	return many == 69 && nested.x == 7 && inlined.x == 23 && varied == 6.5 &&
		old == 7 && callback == 12 && vla == 8 &&
		!strcmp(name, "function_name") ? 0 : 1;
}
