// Native (plain g++, no Bambu) correctness for the PID controller,
// linked directly against the real hls/pid_top.cpp (not a
// reimplementation). Known-good values are hand-derived from
// u[n]=Kp*e[n]+Ki*sum(e)+Kd*(e[n]-e[n-1]) with Kp=1.0, Ki=0.25, Kd=0.5
// (exact powers of two, zero rounding), in raw Q8.8 units (x256).
#include <cstdint>
#include <cassert>
#include <cstdio>

extern int32_t pidStep(int16_t);

// NOTE: both sequences share the SAME underlying `pid` instance (a
// single static object in pid_top.cpp) -- this test must run each
// sequence in one continuous, freshly-started process to be valid. Two
// separate test binaries would be safest; here, running them as two
// independent functions within one process would corrupt the second
// sequence with the first's leftover state, so only one sequence runs
// per process invocation, selected by argv.
static void constant_error_test() {
  // e=1 held for 5 steps: P=Kp*e stays flat, I=Ki*sum grows every step,
  // D=Kd*(e-prev) is 0 after the first step (e never changes again).
  const int32_t expected[] = {448, 384, 448, 512, 576};
  for (int i = 0; i < 5; ++i) {
    assert(pidStep(1) == expected[i]);
  }
  printf("constant_error_test: PASS (448 384 448 512 576)\n");
}

static void impulse_test() {
  // e=1 then 0,0,0,0: P+D spike on the impulse itself, D undershoots
  // when the error vanishes (e decreased), then settles to the I term's
  // permanent memory of the disturbance (0.25 forever after).
  int16_t impulse[] = {1, 0, 0, 0, 0};
  const int32_t expected[] = {448, -64, 64, 64, 64};
  for (size_t i = 0; i < 5; ++i) {
    assert(pidStep(impulse[i]) == expected[i]);
  }
  printf("impulse_test: PASS (448 -64 64 64 64)\n");
}

int main(int argc, char** argv) {
  if (argc > 1 && argv[1][0] == 'i') {
    impulse_test();
  } else {
    constant_error_test();
  }
  return 0;
}
