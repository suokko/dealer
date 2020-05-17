
#if __cplusplus
extern "C" {
#endif

struct shuffle;
struct globals;

/**
 * Create shuffle state object based on global state
 */
struct shuffle* DEFUN(shuffle_factory)(struct globals* gp);

/**
 * Generate next hand
 *
 * @return 0 when hand was generated successfully
 */
int DEFUN(shuffle_next_hand)(struct shuffle* sh, union board* d, struct globals* gp);

/**
 * Free shuffle state
 */
void DEFUN(shuffle_close)(struct shuffle* sh);

/**
 * Provide random number from shuffle context
 */
unsigned DEFUN(random32)(struct shuffle* sh, unsigned max);

#if __cplusplus
} /* extern "C" */
#endif
