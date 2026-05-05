#ifndef OMNISOC_PACK_BYTES_H
#define OMNISOC_PACK_BYTES_H

#include <stdint.h>
#include <string.h>

// OmniSoc UART v3 payload helpers. Pack/unpack typed fields into the
// opaque byte payload of an OmniSoc frame. Little-endian on the wire.
//
// Each pack_* writes its value to `p` and returns the advanced pointer
// for chaining:
//
//     uint8_t buf[12]; uint8_t* p = buf;
//     p = pack_u32(p, ts);
//     p = pack_float(p, angle);
//     p = pack_u8(p, flags);
//     sendMessage(header, buf, p - buf);
//
// Each unpack_* mirrors the shape with a const pointer:
//
//     const uint8_t* p = rxBuf;
//     uint32_t ts;  p = unpack_u32(p, &ts);
//     float angle;  p = unpack_float(p, &angle);
//     uint8_t flags;p = unpack_u8(p, &flags);

inline uint8_t* pack_u8(uint8_t* p, uint8_t v) {
    *p = v;
    return p + 1;
}
inline uint8_t* pack_u16(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
    return p + 2;
}
inline uint8_t* pack_u32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
    p[2] = (uint8_t)((v >> 16) & 0xFF);
    p[3] = (uint8_t)((v >> 24) & 0xFF);
    return p + 4;
}
inline uint8_t* pack_u64(uint8_t* p, uint64_t v) {
    for (int i = 0; i < 8; ++i) p[i] = (uint8_t)((v >> (8 * i)) & 0xFF);
    return p + 8;
}
inline uint8_t* pack_i8 (uint8_t* p, int8_t  v) { return pack_u8 (p, (uint8_t) v); }
inline uint8_t* pack_i16(uint8_t* p, int16_t v) { return pack_u16(p, (uint16_t)v); }
inline uint8_t* pack_i32(uint8_t* p, int32_t v) { return pack_u32(p, (uint32_t)v); }
inline uint8_t* pack_i64(uint8_t* p, int64_t v) { return pack_u64(p, (uint64_t)v); }

inline uint8_t* pack_float(uint8_t* p, float v) {
    memcpy(p, &v, 4);
    return p + 4;
}
inline uint8_t* pack_double(uint8_t* p, double v) {
    memcpy(p, &v, 8);
    return p + 8;
}

inline const uint8_t* unpack_u8(const uint8_t* p, uint8_t* out) {
    *out = *p;
    return p + 1;
}
inline const uint8_t* unpack_u16(const uint8_t* p, uint16_t* out) {
    *out = (uint16_t)p[0] | ((uint16_t)p[1] << 8);
    return p + 2;
}
inline const uint8_t* unpack_u32(const uint8_t* p, uint32_t* out) {
    *out =  (uint32_t)p[0]
         | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
    return p + 4;
}
inline const uint8_t* unpack_u64(const uint8_t* p, uint64_t* out) {
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i) v |= ((uint64_t)p[i]) << (8 * i);
    *out = v;
    return p + 8;
}
inline const uint8_t* unpack_i8 (const uint8_t* p, int8_t*  out) { uint8_t  t; auto r = unpack_u8 (p, &t); *out = (int8_t) t; return r; }
inline const uint8_t* unpack_i16(const uint8_t* p, int16_t* out) { uint16_t t; auto r = unpack_u16(p, &t); *out = (int16_t)t; return r; }
inline const uint8_t* unpack_i32(const uint8_t* p, int32_t* out) { uint32_t t; auto r = unpack_u32(p, &t); *out = (int32_t)t; return r; }
inline const uint8_t* unpack_i64(const uint8_t* p, int64_t* out) { uint64_t t; auto r = unpack_u64(p, &t); *out = (int64_t)t; return r; }

inline const uint8_t* unpack_float(const uint8_t* p, float* out) {
    memcpy(out, p, 4);
    return p + 4;
}
inline const uint8_t* unpack_double(const uint8_t* p, double* out) {
    memcpy(out, p, 8);
    return p + 8;
}

#endif // OMNISOC_PACK_BYTES_H
