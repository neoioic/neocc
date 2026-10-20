#include <stdio.h>

static int values[] = {4, 7, 9};
static struct { int x; } state = {5};
static int touched;
enum { COMPUTED = 7 * 3 };
_Static_assert(sizeof((int[]){1, 2, 3}) == 3 * sizeof(int), "constant size");

static int touch(void)
{
	touched++;
	return 100;
}

static int compute(int n) __attribute__((section(".text.extra")));
static int compute(int n)
{
	int result = COMPUTED;
	__typeof__(touch()) inferred = values[1];

	(void)sizeof(touch());
	(void)sizeof((struct { int x; }){touch()});
	if (0) {
		extern int never_called(void);
		static int discarded = 55;
		result = never_called() + discarded + touch();
	}
	if (n)
		goto chosen;
	result += 2;
	goto end;
	result += 1000;
chosen:
	result += 5;
end:
	__asm__("" : "+r"(result));
	return result + inferred + state.x;
}

int main(void)
{
	int a = compute(0), b = compute(1);

	printf("%d %d %d\n", a, b, touched);
	return 0;
}
