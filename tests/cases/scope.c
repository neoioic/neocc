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

static int cleanup_trace;

static void release(int *value)
{
	cleanup_trace = cleanup_trace * 10 + *value;
}

static int cleanup_jump(int jump)
{
	int outer __attribute__((cleanup(release))) = 1;

	{
		int inner __attribute__((cleanup(release))) = 2;

		if (jump)
			goto out;
	}
out:
	return cleanup_trace;
}

static int cleanup_backwards(void)
{
	int count = 0;

again:
	{
		int value __attribute__((cleanup(release))) = 3;

		if (!count++)
			goto again;
	}
	return cleanup_trace;
}

static int cleanup_loop(void)
{
	int total = 0;

	for (int i = 0; i < 4; i++) {
		int n = i + 2;
		int array[n];
		int value __attribute__((cleanup(release))) = i + 1;

		array[n - 1] = i;
		switch (i) {
		case 1:
			break;
		default:
			total += array[n - 1];
		}
		if (i == 0)
			continue;
		if (i == 2)
			break;
	}
	return total;
}

static int cleanup_expression(void)
{
	int result = ({
		int value __attribute__((cleanup(release))) = 4;

		value + 5;
	});

	return result;
}

int main(void)
{
	struct pair pair = make_pair(2);

	printf("%d %d %d\n", pair.a, pair.b, scoped(3));
	int jumped = cleanup_jump(1);
	int jump_trace = cleanup_trace;
	cleanup_trace = 0;
	int backwards = cleanup_backwards();
	cleanup_trace = 0;
	int loop = cleanup_loop();
	int loop_trace = cleanup_trace;
	cleanup_trace = 0;
	int expression = cleanup_expression();
	printf("cleanup=%d,%d backwards=%d loop=%d,%d expr=%d,%d\n",
		jumped, jump_trace, backwards, loop, loop_trace,
		expression, cleanup_trace);
	return pair.a == 2 && pair.b == 3 && scoped(3) == 44 &&
		jumped == 2 && jump_trace == 21 && backwards == 33 &&
		loop == 2 && loop_trace == 123 && expression == 9 &&
		cleanup_trace == 4 ? 0 : 1;
}
