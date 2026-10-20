int broken(void)
{
	__asm__("" : : "r"(({
		__asm__("" : : "r"(missing_temporary));
		7;
	})));
	return 0;
}
