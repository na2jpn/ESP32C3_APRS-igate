#pragma once
struct RfChannel { const char *frequency; const char *label; };
static constexpr RfChannel RF_CHANNELS[] = {
  {"144.6400", "144.640 MHz"},
  {"144.6600", "144.660 MHz (JA)"},
  {"144.3900", "144.390 MHz (US)"},
  {"144.8000", "144.800 MHz (EU)"},
  {"145.1750", "145.175 MHz (AU)"},
  {"144.5750", "144.575 MHz (NZ)"},
  {"144.9300", "144.930 MHz (AR)"},
  {"145.5700", "145.570 MHz (BR)"}
};
