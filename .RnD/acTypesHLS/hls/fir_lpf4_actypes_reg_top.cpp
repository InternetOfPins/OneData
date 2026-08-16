// Same 4-tap Hamming-LPF FIR, coefficients and ac_fixed types as
// examples/hls_fir/hls/fir_lpf4_actypes_top.cpp, but the delay register
// is split out into a reusable oneData::Data<Sample> component instead of
// a raw member -- proves the component composes cleanly (via HAPI's
// mono_block: a Chain<TapLogic, Data<Sample>> used as one element of the
// outer Chain) and costs nothing extra under Bambu vs. the raw-member
// baseline. Previously used a HAPI-hosted Reg<T> shell (retired -- OneData
// already proves Data<T> synthesizes cleanly under Bambu in its own
// .RnD/hls/FINDINGS.md, so there was no reason to keep a separate,
// HAPI-core-hosted minimal primitive).
#include <hapi/hapi.h>
#include <oneData/oneData.h>
#include <ac_fixed.h>
#include <cstdint>
using namespace hapi;
using oneData::Data;

#if AC_VERSION < 4
#error "resolved to bambu's bundled ac_types fork, not real upstream github.com/hlslibs/ac_types"
#endif

using Sample = ac_fixed<16, 16, true>;
using Accum  = ac_fixed<32, 32, true>;

struct Item {
  static Accum mac(Sample /*delayed*/, Accum acc) { return acc; }
};

template<int16_t Coeff>
struct TapLogic {
  template<typename I>
  struct Part : I {
    using Base = I;
    using Base::Base;

    Accum mac(Sample x, Accum acc) {
      Accum sum   = acc + Accum(Coeff) * Accum(Base::get());
      Sample prev = Base::get();
      Base::set(x);
      return I::mac(prev, sum);
    }
  };
};

template<int16_t Coeff>
using Tap = Chain<TapLogic<Coeff>, Data<Sample>>;

using FirLpf4AcReg = Chain<Tap<10>, Tap<118>, Tap<118>, Tap<10>>;
using Top          = APIOf<Item, FirLpf4AcReg>;

Top firLpf4AcReg;

int32_t firLpf4ActypesRegTop(int16_t x) {
  return firLpf4AcReg.mac(Sample(x), Accum(0)).to_int();
}
