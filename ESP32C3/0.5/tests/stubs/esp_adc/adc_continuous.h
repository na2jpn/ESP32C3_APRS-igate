#pragma once
#include <Arduino.h>
#include <hal/adc_types.h>
using esp_err_t=int;
constexpr int ESP_OK=0, ESP_ERR_TIMEOUT=1, ESP_ERR_INVALID_STATE=2;
using adc_continuous_handle_t=void*;
struct adc_continuous_evt_data_t{};
struct adc_continuous_evt_cbs_t{bool(*on_pool_ovf)(adc_continuous_handle_t,const adc_continuous_evt_data_t*,void*);};
struct adc_continuous_handle_cfg_t{int max_store_buf_size;int conv_frame_size;};
struct adc_digi_pattern_config_t{int atten,channel,unit,bit_width;};
struct adc_continuous_config_t{uint32_t sample_freq_hz;int conv_mode,format,pattern_num;adc_digi_pattern_config_t*adc_pattern;};
inline int failStage=0, stops=0, deinits=0, reads=0;
inline bool errors=false;
struct EndRead{};
inline const char* esp_err_to_name(int){return "MOCK";}
inline int adc_continuous_new_handle(const adc_continuous_handle_cfg_t*c,void**h){assert(c->max_store_buf_size==8192);*h=failStage==1?nullptr:(void*)1;return failStage==1?2:0;}
inline int adc_continuous_config(void*,adc_continuous_config_t*c){assert(c->sample_freq_hz==26400);return failStage==2?2:0;}
inline int adc_continuous_register_event_callbacks(void*,adc_continuous_evt_cbs_t*,void*){return failStage==3?2:0;}
inline int adc_continuous_start(void*){return failStage==4?2:0;}
inline int adc_continuous_stop(void*){++stops;return 0;}
inline int adc_continuous_deinit(void*){++deinits;return 0;}
inline int adc_continuous_read(void*,uint8_t*b,uint32_t size,uint32_t*len,int){
 if(++reads>9)throw EndRead{};
 if(errors){*len=0;return reads%2?ESP_ERR_TIMEOUT:ESP_ERR_INVALID_STATE;}
 *len=size;
 for(unsigned i=0;i<size;i+=4){adc_digi_output_data_t v{};v.type2.channel=3;v.type2.data=2048;__builtin_memcpy(b+i,&v,4);}
 return ESP_OK;
}
