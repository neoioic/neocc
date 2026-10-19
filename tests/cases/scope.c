#include <stdio.h>

struct pair {
	int a, b;
};

enum choice {
	first = 1
};

static int global_value = 5;
static struct pair global_pair = { 11, 12 };

static struct pair make_pair(int value)
{
	struct pair pair = { value, value + 1 };
	return pair;
}

static int scoped(int value)
{
	int total = 0;

	{
		struct pair local = make_pair(value);

		total += local.a + local.b;
	}
	{
		enum choice { first = 9, second };

		total += second;
	}
	total += first;
	{
		extern int global_value;

		total += global_value;
	}
	{
		int i;

		for (i = 0; i < 3; i++) {
			int global_value = i;

			total += global_value;
		}
	}
	{
		extern struct pair global_pair;

		total += global_pair.b;
	}
	goto skip;
	total = -1;
skip:
	total += ({ int temporary = value * 2; temporary; });
	return total;
}

int main(void)
{
	struct pair pair = make_pair(2);

	printf("%d %d %d\n", pair.a, pair.b, scoped(3));
	return pair.a == 2 && pair.b == 3 && scoped(3) == 44 ? 0 : 1;
}
