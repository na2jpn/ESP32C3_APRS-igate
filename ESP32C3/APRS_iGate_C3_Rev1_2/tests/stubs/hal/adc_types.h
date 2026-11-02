#pragma once
using adc_atten_t=int;
constexpr int ADC_CHANNEL_3=3, ADC_UNIT_1=0, ADC_CONV_SINGLE_UNIT_1=0, ADC_DIGI_OUTPUT_FORMAT_TYPE2=0;
union adc_digi_output_data_t{struct {uint32_t data:12;uint32_t channel:4;uint32_t unit:1;uint32_t unused:15;}type2;uint32_t value;};
