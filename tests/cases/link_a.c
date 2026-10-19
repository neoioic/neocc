extern int calculate(int);
int (*function_pointer)(int) = calculate;
const char *message = "relocation";
int shared[4] = { 3, 5, 7, 11 };
