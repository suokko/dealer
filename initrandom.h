
#if __cplusplus
extern "C" {
#endif

struct initrng;

/**
 * Initialize indeterministic random number stream
 */
struct initrng* irng_open();
/**
 * Return next random number from the stream
 */
int irng_next(struct initrng *r, int min, int max);
/**
 * Free the random number the stream
 */
void irng_close(struct initrng *r);

#if __cplusplus
} /* extern "C" */
#endif

