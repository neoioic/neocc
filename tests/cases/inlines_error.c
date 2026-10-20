static inline int broken(void) { return missing_inline; }
int invoke(void) { return broken(); }
