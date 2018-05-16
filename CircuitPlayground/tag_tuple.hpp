#pragma once

/**
 * Helper library for tag-based dispatching.
 * Something like visiting a variant, but without the values.
 */

#include <tuple> // for std::tuple
#include <utility> // for std::forward
#include <type_traits> // for std::integral_constant

namespace extensions {

    template <typename T>
    struct tag{
        using type = T;
    };

    template <typename... T>
    struct tag_tuple{
    private:
        template <size_t I, typename Callback>
        inline static void invoke(Callback&& callback) {
            if constexpr (I == sizeof...(T)) {
                return;
            }
            else {
                callback(tag<std::tuple_element_t<I, std::tuple<T...>>>{}, std::integral_constant<size_t, I>{});
                invoke<I + 1>(std::forward<Callback>(callback));
            }
        }
    public:
        template <typename Callback>
        inline static void for_each(Callback&& callback) {
            invoke<0>(std::forward<Callback>(callback));
        }
    };

}
