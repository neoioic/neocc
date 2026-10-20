#include <stdio.h>
#include <string.h>
#include <wchar.h>

struct item { int number; const char *text; };
struct flex { int count; unsigned char bytes[]; };

static char joined[] = "ab" "c\0" "de";
static wchar_t wide[] = L"A" L"Z";
static int designated[8] = { [5] = 17, [1] = 3, [6] = 19 };
static struct item items[] = {
	{ .text = "fi" "rst", .number = 11 },
	{ .number = 23, .text = "second" }
};
static struct flex flexible = { 3, { 4, 5, 6 } };
static struct flex flexible_again = { 1, { 9 } };
static struct item *compound = &(struct item){ 31, "static" };
static int *nested = (int[]){ 7, sizeof((char[]){ "nested" }), 13 };
static int section_value __attribute__((section(".da" "ta"))) = 29;
static int function(void) { return 41; }
int alias_function(void) __attribute__((alias("func" "tion")));
_Static_assert(sizeof(joined) == 7, "joined " "size");

int main(void)
{
	char local[] = "lo" "cal";
	wchar_t local_wide[] = L"x" L"y";
	struct item automatic[] = { { 2, local }, { 5, "tail" } };
	struct item *literal = &(struct item){ automatic[0].number + 1, local };
	int matrix[][3] = { { 1, 2 }, { [2] = 8 }, { 4, 5, 6 } };
	const char *expression[] = { "abc" + 1, (char[]){ "x" "y" } };
	int ok = sizeof(joined) == 7 && !memcmp(joined, "abc\0de", 7) &&
		wide[0] == 'A' && wide[1] == 'Z' && !wide[2] &&
		designated[5] == 17 && designated[1] == 3 && !designated[7] &&
		items[0].number == 11 && !strcmp(items[0].text, "first") &&
		items[1].number == 23 && flexible.bytes[2] == 6 &&
		flexible_again.bytes[0] == 9 && compound->number == 31 &&
		nested[0] == 7 && nested[1] == 7 && nested[2] == 13 &&
		section_value == 29 && alias_function() == 41 &&
		!strcmp(local, "local") && local_wide[1] == 'y' &&
		literal->number == 3 && literal->text == local &&
		matrix[0][2] == 0 && matrix[1][2] == 8 && matrix[2][1] == 5 &&
		!strcmp(expression[0], "bc") && !strcmp(expression[1], "xy");

	__asm__("" "");
	printf("initializers: %s\n", ok ? "ok" : "failed");
	return !ok;
}
