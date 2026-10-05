#pragma once
// Energieverbrauch: Wh pro Zeitraum (Schlüssel JJJJMMTT, JJJJMM oder JJJJ)
#include <stdint.h>
struct EBin { uint32_t key; float wh; };
struct EnergyLog { double total; EBin days[31]; EBin months[24]; EBin years[10]; };
