#include <libxsmm.h>
#include <libxsmm_timer.h>

#if defined(LIBXSMM_OFFLOAD_TARGET)
# pragma offload_attribute(push,target(LIBXSMM_OFFLOAD_TARGET))
#endif
#include <stdlib.h>
#include <stdio.h>
#if defined(_OPENMP)
# include <omp.h>
#endif
#if defined(LIBXSMM_OFFLOAD_TARGET)
# pragma offload_attribute(pop)
#endif


/**
 * This (micro-)benchmark optionally takes a number of dispatches to be performed.
 * The program measures the duration needed to figure out whether a requested matrix
 * multiplication is available or not. The measured duration excludes the time taken
 * to actually generate the code during the first dispatch.
 */
int main(int argc, char* argv[])
{
  const int size = LIBXSMM_DEFAULT(10000000, 1 < argc ? atoi(argv[1]) : 0);
  const int nthreads = LIBXSMM_DEFAULT(1, 2 < argc ? atoi(argv[2]) : 0);
  unsigned long long start;
  double dcall, ddisp;
  int i;

#if defined(_OPENMP)
  if (1 < nthreads) omp_set_num_threads(nthreads);
#endif

  fprintf(stdout, "Dispatching %i calls %s internal synchronization using %i thread%s...\n", size,
#if 0 != LIBXSMM_SYNC
    "with",
#else
    "without",
#endif
    1 >= nthreads ? 1 : nthreads,
    1 >= nthreads ? "" : "s");

#if 0 != LIBXSMM_JIT
  if (LIBXSMM_X86_AVX > libxsmm_get_target_archid()) {
    fprintf(stderr, "\tWarning: JIT support is not available at runtime!\n");
  }
#else
  fprintf(stderr, "\tWarning: JIT support has been disabled at build time!\n");
#endif

#if defined(LIBXSMM_OFFLOAD_TARGET)
# pragma offload target(LIBXSMM_OFFLOAD_TARGET)
#endif
  {
    /* first invocation may initialize some internals (libxsmm_init),
     * or actually generate code (code gen. time is out of scope)
     */
    libxsmm_dmmdispatch(LIBXSMM_AVG_M, LIBXSMM_AVG_N, LIBXSMM_AVG_K,
      NULL/*lda*/, NULL/*ldb*/, NULL/*ldc*/, NULL/*alpha*/, NULL/*beta*/,
      NULL/*flags*/, NULL/*prefetch*/);

    /* run non-inline function to measure call overhead of an "empty" function */
    start = libxsmm_timer_tick();
#if defined(_OPENMP)
#   pragma omp parallel for default(none) private(i)
#endif
    for (i = 0; i < size; ++i) {
      libxsmm_init(); /* subsequent calls are not doing any work */
    }
    dcall = libxsmm_timer_duration(start, libxsmm_timer_tick());

    start = libxsmm_timer_tick();
#if defined(_OPENMP)
#   pragma omp parallel for default(none) private(i)
#endif
    for (i = 0; i < size; ++i) {
      libxsmm_dmmdispatch(LIBXSMM_AVG_M, LIBXSMM_AVG_N, LIBXSMM_AVG_K,
        NULL/*lda*/, NULL/*ldb*/, NULL/*ldc*/, NULL/*alpha*/, NULL/*beta*/,
        NULL/*flags*/, NULL/*prefetch*/);
    }
    ddisp = libxsmm_timer_duration(start, libxsmm_timer_tick());
  }

  if (0 < dcall && 0 < ddisp) {
    fprintf(stdout, "\tdispatch calls/s: %.1f MHz\n", 1E-6 * size / ddisp);
    fprintf(stdout, "\tempty calls/s: %.1f MHz\n", 1E-6 * size / dcall);
    fprintf(stdout, "\toverhead: %.1fx\n", ddisp / dcall);
  }
  fprintf(stdout, "Finished\n");

  return EXIT_SUCCESS;
}

