// Minimal DIS (IEEE 1278.1) Entity State PDU: encode/decode + dead reckoning.
// Wire format is big endian. Total size with no articulation params: 144 bytes.
#pragma once
#include <cstdint>
#include <cstring>
#include <array>
#include <cmath>

namespace dis {

constexpr size_t kEspduSize = 144;
constexpr uint8_t kProtocolVersion = 6;  // IEEE 1278.1-1995
constexpr uint8_t kPduTypeEntityState = 1;
constexpr uint8_t kFamilyEntityInfo = 1;
constexpr uint8_t kDeadReckoningFPW = 2;  // constant velocity, world coords

struct Vec3d { double x{}, y{}, z{}; };
struct Vec3f { float x{}, y{}, z{}; };

struct EntityType {
    uint8_t kind{1}, domain{2};  // 1 = platform, 2 = air
    uint16_t country{225};       // 225 = USA
    uint8_t category{1}, subcategory{0}, specific{0}, extra{0};
};

struct EntityStatePdu {
    uint8_t exerciseId{1};
    uint32_t timestamp{};
    uint16_t site{1}, application{1}, entity{1};
    uint8_t forceId{1};  // 1 = friendly
    EntityType type{};
    Vec3f velocity{};    // m/s, ECEF
    Vec3d location{};    // m, ECEF (geocentric)
    Vec3f orientation{}; // psi, theta, phi (radians)
    uint32_t appearance{};
    uint8_t drAlgorithm{kDeadReckoningFPW};
    Vec3f drAccel{};
    Vec3f drAngVel{};
    char marking[12]{"ENTITY1"};
    uint32_t capabilities{};
};

// ---- big endian helpers ----
class Writer {
public:
    explicit Writer(uint8_t* p) : p_(p) {}
    void u8(uint8_t v) { *p_++ = v; }
    void u16(uint16_t v) { u8(v >> 8); u8(v & 0xFF); }
    void u32(uint32_t v) { u16(v >> 16); u16(v & 0xFFFF); }
    void u64(uint64_t v) { u32(uint32_t(v >> 32)); u32(uint32_t(v)); }
    void f32(float v) { uint32_t b; std::memcpy(&b, &v, 4); u32(b); }
    void f64(double v) { uint64_t b; std::memcpy(&b, &v, 8); u64(b); }
    void zeros(size_t n) { for (size_t i = 0; i < n; ++i) u8(0); }
    void bytes(const void* s, size_t n) { std::memcpy(p_, s, n); p_ += n; }
private:
    uint8_t* p_;
};

class Reader {
public:
    explicit Reader(const uint8_t* p) : p_(p) {}
    uint8_t u8() { return *p_++; }
    uint16_t u16() { uint16_t h = u8(); return uint16_t(h << 8 | u8()); }
    uint32_t u32() { uint32_t h = u16(); return h << 16 | u16(); }
    uint64_t u64() { uint64_t h = u32(); return h << 32 | u32(); }
    float f32() { uint32_t b = u32(); float v; std::memcpy(&v, &b, 4); return v; }
    double f64() { uint64_t b = u64(); double v; std::memcpy(&v, &b, 8); return v; }
    void skip(size_t n) { p_ += n; }
    void bytes(void* d, size_t n) { std::memcpy(d, p_, n); p_ += n; }
private:
    const uint8_t* p_;
};

inline std::array<uint8_t, kEspduSize> encode(const EntityStatePdu& e) {
    std::array<uint8_t, kEspduSize> buf{};
    Writer w(buf.data());
    // PDU header (12 bytes)
    w.u8(kProtocolVersion); w.u8(e.exerciseId); w.u8(kPduTypeEntityState);
    w.u8(kFamilyEntityInfo); w.u32(e.timestamp); w.u16(kEspduSize); w.u16(0);
    // Body
    w.u16(e.site); w.u16(e.application); w.u16(e.entity);
    w.u8(e.forceId); w.u8(0);  // no articulation parameters
    auto type = [&](const EntityType& t) {
        w.u8(t.kind); w.u8(t.domain); w.u16(t.country);
        w.u8(t.category); w.u8(t.subcategory); w.u8(t.specific); w.u8(t.extra);
    };
    type(e.type); type(EntityType{0, 0, 0, 0, 0, 0, 0});  // alternate type
    w.f32(e.velocity.x); w.f32(e.velocity.y); w.f32(e.velocity.z);
    w.f64(e.location.x); w.f64(e.location.y); w.f64(e.location.z);
    w.f32(e.orientation.x); w.f32(e.orientation.y); w.f32(e.orientation.z);
    w.u32(e.appearance);
    w.u8(e.drAlgorithm); w.zeros(15);
    w.f32(e.drAccel.x); w.f32(e.drAccel.y); w.f32(e.drAccel.z);
    w.f32(e.drAngVel.x); w.f32(e.drAngVel.y); w.f32(e.drAngVel.z);
    w.u8(1);  // ASCII character set
    w.bytes(e.marking, 11);
    w.u32(e.capabilities);
    return buf;
}

// Returns false if the packet is not a well formed Entity State PDU.
inline bool decode(const uint8_t* data, size_t len, EntityStatePdu& e) {
    if (len < kEspduSize) return false;
    Reader r(data);
    uint8_t version = r.u8(); e.exerciseId = r.u8();
    uint8_t pduType = r.u8(); uint8_t family = r.u8();
    e.timestamp = r.u32(); uint16_t length = r.u16(); r.skip(2);
    if (version != kProtocolVersion || pduType != kPduTypeEntityState ||
        family != kFamilyEntityInfo || length != kEspduSize) return false;
    e.site = r.u16(); e.application = r.u16(); e.entity = r.u16();
    e.forceId = r.u8();
    if (r.u8() != 0) return false;  // articulation params unsupported
    auto type = [&](EntityType& t) {
        t.kind = r.u8(); t.domain = r.u8(); t.country = r.u16();
        t.category = r.u8(); t.subcategory = r.u8(); t.specific = r.u8(); t.extra = r.u8();
    };
    EntityType alt; type(e.type); type(alt);
    e.velocity = {r.f32(), r.f32(), r.f32()};
    e.location = {r.f64(), r.f64(), r.f64()};
    e.orientation = {r.f32(), r.f32(), r.f32()};
    e.appearance = r.u32();
    e.drAlgorithm = r.u8(); r.skip(15);
    e.drAccel = {r.f32(), r.f32(), r.f32()};
    e.drAngVel = {r.f32(), r.f32(), r.f32()};
    r.skip(1); r.bytes(e.marking, 11); e.marking[11] = '\0';
    e.capabilities = r.u32();
    return true;
}

// Dead reckoning (FPW): position at dt seconds after the last PDU.
inline Vec3d extrapolate(const EntityStatePdu& e, double dt) {
    return {e.location.x + e.velocity.x * dt,
            e.location.y + e.velocity.y * dt,
            e.location.z + e.velocity.z * dt};
}

inline double distance(const Vec3d& a, const Vec3d& b) {
    return std::sqrt((a.x-b.x)*(a.x-b.x) + (a.y-b.y)*(a.y-b.y) + (a.z-b.z)*(a.z-b.z));
}

}  // namespace dis
