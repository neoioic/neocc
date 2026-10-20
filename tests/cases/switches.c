#include <stdio.h>

static int dense(int n)
{
	switch (n) {
	case -4: case -3: case -2: return 10;
	case -1: return 11;
	case 0: return 12;
	case 1: return 13;
	case 2: return 14;
	case 3: return 15;
	case 4: return 16;
	case 5: return 17;
	case 6: return 18;
	case 7: return 19;
	case 8: return 20;
	case 9: return 21;
	case 10: return 22;
	default: return 3;
	}
}

static int ranges(unsigned long long n)
{
	switch (n) {
	case 0: return 1;
	case 2 ... 5: return 2;
	case 0x7fffffffffffffffULL: return 3;
	case 0x8000000000000000ULL: return 4;
	case 0xfffffffffffffffeULL ... 0xffffffffffffffffULL: return 5;
	default: return 6;
	}
}

static int nested(int n)
{
	int sum = 0;

	for (int i = 0; i < 4; i++) {
		switch (n) {
		case 1:
			switch (i) {
			case 0: sum += 2; break;
			case 1: continue;
			default: sum += 3;
			}
			break;
		default: sum++;
		}
		sum += 10;
	}
	return sum;
}

static int suppressed(int n)
{
	if (0) {
		switch (n) { case 1: return 99; default: return 98; }
	}
	switch (n) { }
	switch (n) { default: return 7; }
}

int main(void)
{
	int sum = 0;

	for (int n = -5; n <= 11; n++)
		sum += dense(n);
	int high = ranges(0xffffffffffffffffULL);
	int middle = ranges(0x8000000000000000ULL);
	int range = ranges(4);
	int nest = nested(1);
	int dead = suppressed(5);

	printf("switch=%d,%d,%d,%d,%d,%d\n", sum, high, middle, range, nest, dead);
	return sum == 234 && high == 5 && middle == 4 && range == 2 &&
		nest == 38 && dead == 7 ? 0 : 1;
}
