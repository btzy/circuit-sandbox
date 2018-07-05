#pragma once

#include <type_traits>
#include <stdexcept>
#include <SDL.h>
#include <boost/endian/conversion.hpp>

// note: could be constexpr with C++20 bit_cast

// note: ARGB8888 means that the color is encoded as 0xAARRGGBB, i.e. alpha in the most significant 8 bits.

template <uint32_t PixelFormat>
inline uint32_t fast_MapRGBA(const SDL_Color& color);

template <>
inline uint32_t fast_MapRGBA<SDL_PIXELFORMAT_RGBA8888>(const SDL_Color& color) {
    uint32_t ans = reinterpret_cast<const uint32_t&>(color);
    boost::endian::big_to_native_inplace(ans);
    return ans;
}

template <>
inline uint32_t fast_MapRGBA<SDL_PIXELFORMAT_RGBX8888>(const SDL_Color& color) {
    uint32_t ans = reinterpret_cast<const uint32_t&>(color);
    boost::endian::big_to_native_inplace(ans);
    return ans;
}

template <>
inline uint32_t fast_MapRGBA<SDL_PIXELFORMAT_ABGR8888>(const SDL_Color& color) {
    uint32_t ans = reinterpret_cast<const uint32_t&>(color);
    boost::endian::little_to_native_inplace(ans);
    return ans;
}

template <>
inline uint32_t fast_MapRGBA<SDL_PIXELFORMAT_ARGB8888>(const SDL_Color& color) {
    uint32_t ans = reinterpret_cast<const uint32_t&>(color);
    boost::endian::big_to_native_inplace(ans);
    return (ans >> 8) | (ans << 24);
}

template <>
inline uint32_t fast_MapRGBA<SDL_PIXELFORMAT_BGRA8888>(const SDL_Color& color) {
    uint32_t ans = reinterpret_cast<const uint32_t&>(color);
    boost::endian::little_to_native_inplace(ans);
    return (ans << 8) | (ans >> 24);
}

template <>
inline uint32_t fast_MapRGBA<SDL_PIXELFORMAT_BGRX8888>(const SDL_Color& color) {
    uint32_t ans = reinterpret_cast<const uint32_t&>(color);
    boost::endian::little_to_native_inplace(ans);
    return (ans << 8);
}

template <uint32_t PixelFormat>
inline uint32_t fast_MapRGB(const SDL_Color& color);

template <>
inline uint32_t fast_MapRGB<SDL_PIXELFORMAT_RGBA8888>(const SDL_Color& color) {
    uint32_t ans = reinterpret_cast<const uint32_t&>(color);
    boost::endian::big_to_native_inplace(ans);
    return ans;
}

template <>
inline uint32_t fast_MapRGB<SDL_PIXELFORMAT_RGBX8888>(const SDL_Color& color) {
    uint32_t ans = reinterpret_cast<const uint32_t&>(color);
    boost::endian::big_to_native_inplace(ans);
    return ans;
}

template <>
inline uint32_t fast_MapRGB<SDL_PIXELFORMAT_ABGR8888>(const SDL_Color& color) {
    uint32_t ans = reinterpret_cast<const uint32_t&>(color);
    boost::endian::little_to_native_inplace(ans);
    return ans;
}

template <>
inline uint32_t fast_MapRGB<SDL_PIXELFORMAT_ARGB8888>(const SDL_Color& color) {
    uint32_t ans = reinterpret_cast<const uint32_t&>(color);
    boost::endian::big_to_native_inplace(ans);
    return (ans >> 8);
}

template <>
inline uint32_t fast_MapRGB<SDL_PIXELFORMAT_BGRA8888>(const SDL_Color& color) {
    uint32_t ans = reinterpret_cast<const uint32_t&>(color);
    boost::endian::little_to_native_inplace(ans);
    return (ans << 8);
}

template <>
inline uint32_t fast_MapRGB<SDL_PIXELFORMAT_BGRX8888>(const SDL_Color& color) {
    uint32_t ans = reinterpret_cast<const uint32_t&>(color);
    boost::endian::little_to_native_inplace(ans);
    return (ans << 8);
}

template <typename Callback>
inline auto invoke_RGB_format(const uint32_t& pixelFormat, Callback&& callback) {
    switch (pixelFormat) {
    case SDL_PIXELFORMAT_RGBA8888:
        return std::forward<Callback>(callback)(std::integral_constant<uint32_t, SDL_PIXELFORMAT_RGBA8888>{});
    case SDL_PIXELFORMAT_RGBX8888:
        return std::forward<Callback>(callback)(std::integral_constant<uint32_t, SDL_PIXELFORMAT_RGBX8888>{});
    case SDL_PIXELFORMAT_ABGR8888:
        return std::forward<Callback>(callback)(std::integral_constant<uint32_t, SDL_PIXELFORMAT_ABGR8888>{});
    case SDL_PIXELFORMAT_ARGB8888:
        return std::forward<Callback>(callback)(std::integral_constant<uint32_t, SDL_PIXELFORMAT_ARGB8888>{});
    case SDL_PIXELFORMAT_BGRA8888:
        return std::forward<Callback>(callback)(std::integral_constant<uint32_t, SDL_PIXELFORMAT_BGRA8888>{});
    case SDL_PIXELFORMAT_BGRX8888:
        return std::forward<Callback>(callback)(std::integral_constant<uint32_t, SDL_PIXELFORMAT_BGRX8888>{});
    default:
        throw std::logic_error("Unrecognized pixel format");
    }
}

inline SDL_Texture* create_fast_texture(SDL_Renderer* renderer, int access, const ext::point& size, uint32_t& format) {
    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_UNKNOWN, access, size.x, size.y);
    if (texture != nullptr) {
        uint32_t textureFormat;
        SDL_QueryTexture(texture, &textureFormat, nullptr, nullptr, nullptr);
        switch (textureFormat) {
        case SDL_PIXELFORMAT_RGBA8888: [[fallthrough]];
        case SDL_PIXELFORMAT_RGBX8888: [[fallthrough]];
        case SDL_PIXELFORMAT_ABGR8888: [[fallthrough]];
        case SDL_PIXELFORMAT_ARGB8888: [[fallthrough]];
        case SDL_PIXELFORMAT_BGRA8888: [[fallthrough]];
        case SDL_PIXELFORMAT_BGRX8888:
            format = textureFormat;
            return texture;
        default:
            SDL_DestroyTexture(texture);
            break;
        }
    }
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, access, size.x, size.y); // default texture (most widely supported)
    if (texture != nullptr) {
        format = SDL_PIXELFORMAT_ARGB8888;
    }
    return texture;
}
