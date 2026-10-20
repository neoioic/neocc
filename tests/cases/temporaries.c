#include <stdio.h>

struct inner { int x; };
struct outer { struct inner a; struct inner b[2]; };
typedef struct outer aggregate;
enum values { FIRST = 7, SECOND = 13 };

static int nested(aggregate *p)
{
	int value = FIRST;

	__asm__("" : "+r"(value) : "r"(({
		int inner = SECOND;
		__asm__("" : "+r"(inner));
		inner;
	})));
	return value + p->a.x + p->b[1].x;
}

int main(void)
{
	aggregate value = {{2}, {{3}, {5}}};

	printf("%d\n", nested(&value));
	return 0;
}
