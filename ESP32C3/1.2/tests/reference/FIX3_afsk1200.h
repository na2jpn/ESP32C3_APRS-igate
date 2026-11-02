#pragma once

#include <stdint.h>
#include <stddef.h>
#include <math.h>
#include "config.h"

class Afsk1200Decoder {
public:
  using FrameCallback = void (*)(const uint8_t *frame, size_t len);

  explicit Afsk1200Decoder(FrameCallback cb) : _callback(cb) {}

  void begin() {
    for (int i = 0; i < LUT_LEN; ++i) {
      const float t = static_cast<float>(i) / static_cast<float>(AFSK_SAMPLE_RATE);
      const float pm = 2.0f * PI_F * 1200.0f * t;
      const float ps = 2.0f * PI_F * 2200.0f * t;
      _mCos[i] = cosf(pm); _mSin[i] = sinf(pm);
      _sCos[i] = cosf(ps); _sSin[i] = sinf(ps);
    }
    resetAll();
  }

  void resetAll() {
    _dc = 0.0f;
    _dcInitialized = false;
    _rms = 0.0f;
    _mi = _mq = _si = _sq = 0.0f;
    _lut = 0;
    _tone = false;
    _toneValid = false;
    _lastMidToneValid = false;
    _bitPhase = 0.0f;
    _silenceSamples = 0;
    _rawCount = 0;
    _flagShift = 0;
    _flagBits = 0;
    _inFrame = false;
  }

  void processRaw(uint16_t raw) {
    // Remove the 1.65 V ADC bias with a slow DC tracker. The first sample
    // initializes the center, so this also tolerates 11-bit vs 12-bit DMA formats.
    if (!_dcInitialized) { _dc = static_cast<float>(raw); _dcInitialized = true; }
    _dc += 0.0010f * (static_cast<float>(raw) - _dc);
    const float x = (static_cast<float>(raw) - _dc) / 2048.0f;
    _rms = 0.997f * _rms + 0.003f * x * x;

    // Complex mixers + one-pole low-pass. This yields mark/space energy.
    constexpr float alpha = 0.18f;
    const float mc = x * _mCos[_lut];
    const float ms = x * _mSin[_lut];
    const float sc = x * _sCos[_lut];
    const float ss = x * _sSin[_lut];
    _mi += alpha * (mc - _mi);
    _mq += alpha * (ms - _mq);
    _si += alpha * (sc - _si);
    _sq += alpha * (ss - _sq);

    if (++_lut >= LUT_LEN) _lut = 0;

    const float em = _mi * _mi + _mq * _mq;
    const float es = _si * _si + _sq * _sq;
    const float total = em + es + 1.0e-12f;
    const float discr = (em - es) / total;

    // A weak/noisy input is allowed to exist, but we don't clock HDLC from it.
    const bool signalPresent = (_rms > 1.0e-5f) && (total > 2.0e-6f);
    if (!signalPresent) {
      if (++_silenceSamples > AFSK_SAMPLE_RATE / 5) { // ~200 ms
        _toneValid = false;
        _lastMidToneValid = false;
        _inFrame = false;
        _rawCount = 0;
        _flagBits = 0;
      }
      return;
    }
    _silenceSamples = 0;

    bool candidate = _tone;
    bool candidateKnown = false;
    if (discr > 0.10f) { candidate = true; candidateKnown = true; }
    else if (discr < -0.10f) { candidate = false; candidateKnown = true; }

    if (!_toneValid && candidateKnown) {
      _tone = candidate;
      _toneValid = true;
    } else if (_toneValid && candidateKnown && candidate != _tone) {
      _tone = candidate;
      // Tone transitions correspond to NRZI zeroes and occur at bit boundaries.
      float err = _bitPhase;
      if (err > 0.5f) err -= 1.0f;
      _bitPhase -= 0.55f * err;
      if (_bitPhase < 0.0f) _bitPhase += 1.0f;
      if (_bitPhase >= 1.0f) _bitPhase -= 1.0f;
    }

    if (!_toneValid) return;

    const float oldPhase = _bitPhase;
    _bitPhase += static_cast<float>(AFSK_BAUD) / static_cast<float>(AFSK_SAMPLE_RATE);
    if (_bitPhase >= 1.0f) _bitPhase -= 1.0f;

    // Sample at the middle of each bit cell, away from tone transitions.
    if (oldPhase < 0.5f && _bitPhase >= 0.5f) {
      if (_lastMidToneValid) {
        const bool dataBit = (_tone == _lastMidTone); // NRZI: same=1, transition=0
        feedHdlcBit(dataBit);
      }
      _lastMidTone = _tone;
      _lastMidToneValid = true;
    }
  }

  float rms() const { return _rms; }
  uint32_t framesGood() const { return _framesGood; }
  uint32_t crcBad() const { return _crcBad; }
  uint32_t frameErrors() const { return _frameErrors; }

private:
  static constexpr float PI_F = 3.14159265358979323846f;
  // 26400 Hz gives exact periods: 1200 Hz=22 samples, 2200 Hz=12 samples.
  // LCM(22,12)=132 samples, so both lookup tables repeat perfectly here.
  static constexpr int LUT_LEN = 132;
  static constexpr size_t RAW_MAX_BITS = 4096;
  static constexpr size_t FRAME_MAX_BYTES = 512;

  FrameCallback _callback = nullptr;
  float _mCos[LUT_LEN]{};
  float _mSin[LUT_LEN]{};
  float _sCos[LUT_LEN]{};
  float _sSin[LUT_LEN]{};
  int _lut = 0;

  float _dc = 0.0f;
  bool _dcInitialized = false;
  float _rms = 0.0f;
  float _mi = 0, _mq = 0, _si = 0, _sq = 0;
  bool _tone = false;
  bool _toneValid = false;
  bool _lastMidTone = false;
  bool _lastMidToneValid = false;
  float _bitPhase = 0.0f;
  uint32_t _silenceSamples = 0;

  uint8_t _rawBits[RAW_MAX_BITS]{};
  size_t _rawCount = 0;
  uint8_t _flagShift = 0;
  uint8_t _flagBits = 0;
  bool _inFrame = false;

  uint32_t _framesGood = 0;
  uint32_t _crcBad = 0;
  uint32_t _frameErrors = 0;

  void feedHdlcBit(bool bit) {
    _flagShift = static_cast<uint8_t>((_flagShift << 1) | (bit ? 1 : 0));
    if (_flagBits < 8) ++_flagBits;

    if (_inFrame) {
      if (_rawCount >= RAW_MAX_BITS) {
        _inFrame = false;
        _rawCount = 0;
        ++_frameErrors;
      } else {
        _rawBits[_rawCount++] = bit ? 1 : 0;
      }
    }

    if (_flagBits >= 8 && _flagShift == 0x7E) {
      if (_inFrame && _rawCount >= 8) {
        decodeRawFrame(_rawBits, _rawCount - 8); // remove the closing flag
      }
      // A flag is also a valid opening flag for the next frame.
      _inFrame = true;
      _rawCount = 0;
    }
  }

  void decodeRawFrame(const uint8_t *bits, size_t count) {
    if (count < 16) return;

    uint8_t bytes[FRAME_MAX_BYTES]{};
    size_t byteCount = 0;
    uint8_t cur = 0;
    uint8_t bitPos = 0;
    uint8_t ones = 0;

    for (size_t i = 0; i < count; ++i) {
      const uint8_t b = bits[i] ? 1 : 0;

      if (b) {
        cur |= static_cast<uint8_t>(1u << bitPos);
        ++ones;
      } else {
        ones = 0;
      }

      ++bitPos;
      if (bitPos == 8) {
        if (byteCount >= FRAME_MAX_BYTES) { ++_frameErrors; return; }
        bytes[byteCount++] = cur;
        cur = 0;
        bitPos = 0;
      }

      // After five consecutive 1 bits, HDLC inserts a 0 bit. Skip that 0.
      if (ones == 5) {
        if (i + 1 >= count) { ++_frameErrors; return; }
        const uint8_t stuffed = bits[++i] ? 1 : 0;
        if (stuffed != 0) { ++_frameErrors; return; }
        ones = 0;
      }
    }

    if (bitPos != 0 || byteCount < 3) {
      ++_frameErrors;
      return;
    }

    const uint16_t expected = static_cast<uint16_t>(bytes[byteCount - 2]) |
                              (static_cast<uint16_t>(bytes[byteCount - 1]) << 8);
    const uint16_t actual = crc16X25(bytes, byteCount - 2);
    if (expected != actual) {
      ++_crcBad;
      return;
    }

    ++_framesGood;
    if (_callback) _callback(bytes, byteCount - 2); // callback excludes FCS
  }

  static uint16_t crc16X25(const uint8_t *data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; ++i) {
      crc ^= data[i];
      for (int b = 0; b < 8; ++b) {
        crc = (crc & 1) ? static_cast<uint16_t>((crc >> 1) ^ 0x8408) : static_cast<uint16_t>(crc >> 1);
      }
    }
    return static_cast<uint16_t>(crc ^ 0xFFFF);
  }
};
