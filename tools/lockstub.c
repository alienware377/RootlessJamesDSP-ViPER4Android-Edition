/* The effect harnesses link without the controller, so the lock is a no-op:
   they are single-threaded, and what is under test is the DSP. */
struct dspsys;
void jdsp_lock(struct dspsys *j) { (void)j; }
void jdsp_unlock(struct dspsys *j) { (void)j; }
