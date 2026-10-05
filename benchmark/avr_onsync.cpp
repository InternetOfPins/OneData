// What change reporting costs on an ATmega328P (avr-g++ -Os, simavr at 16 MHz): the same eight values set one after the other
// (1 1 2 3 3 3 4 5), each variant reporting a change its own way. Prints "SIZEOF <bytes>", "CYC <cycles>", "FIRED <reports>" over the
// UART, then OK or WRONG, then END.
//   -DVARIANT=0  no data: the harness alone (the loop, the UART)
//   -DVARIANT=1  Data<uint8_t>: set(), then the value is read back each time (nothing is tracked)
//   -DVARIANT=2  Watch<Data<uint8_t>>, the loop tests changed() and calls sync() itself
//   -DVARIANT=3  OnSync<note> over Watch<Data<uint8_t>>: sync() reports
//   -DVARIANT=4  OnChange<note> over Data<uint8_t>: every set() reports
#include <oneData/oneData.h>
#include <avr/io.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>

using namespace oneData;

volatile uint8_t sink, fired;
static void note(uint8_t v) { sink = v; fired = uint8_t(fired + 1); }

#if VARIANT == 1
  using D = DataDef<Data<uint8_t>>;
#elif VARIANT == 2
  using D = DataDef<Watch<Data<uint8_t>>>;
#elif VARIANT == 3
  using D = DataDef<OnSync<note>, Watch<Data<uint8_t>>>;
#elif VARIANT == 4
  using D = DataDef<OnChange<note>, Data<uint8_t>>;
#endif

static void uputc(char c) { while (!(UCSR0A & (1 << UDRE0))) {} UDR0 = c; }
static void puts_(const char* s) { while (*s) uputc(*s++); }
static void putu(uint16_t v) { char b[6]; uint8_t k = 0; do { b[k++] = char('0' + v % 10); v /= 10; } while (v); while (k) uputc(b[--k]); }

volatile uint8_t seq[8] = {1, 1, 2, 3, 3, 3, 4, 5};

int main() {
  UBRR0 = 8; UCSR0B = (1 << TXEN0) | (1 << RXEN0); UCSR0C = 3 << UCSZ00;
  TCCR1A = 0; TCCR1B = 1 << CS10;                              // Timer1 at the CPU clock
#if VARIANT == 0
  TCNT1 = 0; for (uint8_t i = 0; i < 8; ++i) sink = seq[i]; uint16_t c = TCNT1;
  puts_("SIZEOF 0\n"); puts_("CYC "); putu(c); uputc('\n'); puts_("FIRED 0\nOK\n");
#else
  static D d;
  TCNT1 = 0;
  for (uint8_t i = 0; i < 8; ++i) {
    d.set(seq[i]);
  #if VARIANT == 1
    sink = d.get();
  #elif VARIANT == 2
    if (d.changed()) { note(d.get()); d.sync(); }
  #elif VARIANT == 3
    d.sync();
  #endif
  }
  uint16_t c = TCNT1;
  puts_("SIZEOF "); putu(sizeof(D)); uputc('\n'); puts_("CYC "); putu(c); uputc('\n'); puts_("FIRED "); putu(fired); uputc('\n');
  // Watch reports the five changes (1 2 3 4 5); OnChange every one of the eight sets; Data none, its last read is 5
  const uint8_t want = VARIANT == 1 ? 0 : VARIANT == 4 ? 8 : 5;
  puts_(fired == want && sink == 5 ? "OK\n" : "WRONG\n");
#endif
  puts_("END\n");
  cli(); sleep_cpu();
}
