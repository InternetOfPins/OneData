// A genuinely FUNCTIONAL component, not another compatibility/resource
// check: a real PID controller, u[n] = Kp*e[n] + Ki*sum(e) +
// Kd*(e[n]-e[n-1]), built entirely from patterns already proven in this
// investigation -- the integral term reuses the accumulator shape
// (ac_accumulator_top.cpp), the derivative term reuses the FIR tap's
// delay-register shape (fir_lpf4_actypes_reg_top.cpp), and both coexist
// in one flat mono_block chain exactly like biquad_top.cpp's feedforward/
// feedback split: two independently-named methods (integrate()/
// differentiate()), each touching only its own oneData::Data<T> state,
// ordinary C++ inheritance routing each call past whichever component
// doesn't define it.
//
// Kp=1.0(256), Ki=0.25(64), Kd=0.5(128) -- exact powers of two, same
// reasoning as every other coefficient choice in this investigation:
// zero rounding, so the response is exactly hand-computable. Raw Q8.8
// bit-pattern coefficients via ac_fixed::set_slc, not the double
// constructor -- same constant-folding trap found in the biquad round
// (a `static const` double-initialized literal synthesizes a real
// stateful lazy-init guard under Bambu instead of folding away).
#include <hapi/hapi.h>
#include <oneData/oneData.h>
#include <ac_fixed.h>
#include <ac_int.h>
#include <cstdint>
using namespace hapi;
using oneData::Data;

#if AC_VERSION < 4
#error "resolved to bambu's bundled ac_types fork, not real upstream github.com/hlslibs/ac_types"
#endif

using Sample = ac_fixed<16, 8, true>;   // Q8.8
using Accum  = ac_fixed<32, 16, true>;  // Q16.16

template<int16_t RawBits>
static inline Sample rawCoeff() {
  Sample c;
  c.set_slc(0, ac_int<16, true>(RawBits));
  return c;
}

struct Item {
  static Accum integrate(Sample /*e*/) { return Accum(0); }
  static Accum differentiate(Sample /*e*/) { return Accum(0); }
};

// Integral term: accumulates error every call (same shape as
// ac_accumulator_top.cpp's AccumulateOp), returns the Ki-scaled sum.
template<int16_t KiRawBits>
struct IntegLogic {
  template<typename I>
  struct Part : I {
    using Base = I;
    using Base::Base;
    Accum integrate(Sample e) {
      Base::set(Accum(Base::get()) + Accum(e));
      return Accum(rawCoeff<KiRawBits>()) * Base::get();
    }
  };
};
template<int16_t KiRawBits>
using Integrator = Chain<IntegLogic<KiRawBits>, Data<Accum>>;

// Derivative term: same read-old/update-new shape as a FIR tap's delay,
// returns the Kd-scaled delta between this call's error and the last.
template<int16_t KdRawBits>
struct DerivLogic {
  template<typename I>
  struct Part : I {
    using Base = I;
    using Base::Base;
    Accum differentiate(Sample e) {
      Sample prev = Base::get();
      Base::set(e);
      return Accum(rawCoeff<KdRawBits>()) * Accum(e - prev);
    }
  };
};
template<int16_t KdRawBits>
using Differentiator = Chain<DerivLogic<KdRawBits>, Data<Sample>>;

// Kp=1.0(256), Ki=0.25(64), Kd=0.5(128).
using Pid = Chain<Integrator<64>, Differentiator<128>>;
using Top = APIOf<Item, Pid>;

Top pid;

// Input is an integer-valued error sample (er=1 -> e=1.0), output is raw
// Q8.8 bits (e.g. 0.5 -> 128) -- same asymmetric convention as every
// other target in this investigation: simple integer test inputs in,
// fractional results made visible as scaled integers out.
int32_t pidStep(int16_t er) {
  Sample e = Sample(er);
  Accum p = Accum(rawCoeff<256>()) * Accum(e);
  Accum i = pid.integrate(e);
  Accum d = pid.differentiate(e);
  Sample u = Accum(p + i + d);  // narrow Q16.16 -> Q8.8, quantized correctly
  return ac_int<16, true>(u.slc<16>(0)).to_int();
}
