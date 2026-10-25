// newlib's int and unsigned conversions, which glibc lacks. The long versions
// come from the core's stdlib_noniso.c.
#ifdef __cplusplus
extern "C" {
#endif
char* ltoa(long value, char* result, int base);
char* ultoa(unsigned long value, char* result, int base);

char* itoa(int v, char* s, int base) { return ltoa(v, s, base); }
char* utoa(unsigned v, char* s, int base) { return ultoa(v, s, base); }
#ifdef __cplusplus
}
#endif
