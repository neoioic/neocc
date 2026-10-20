static inline int duplicate(void) { return 1; }
static inline int duplicate(void) { return 2; }
int invoke(void) { return duplicate(); }
