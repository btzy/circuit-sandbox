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

        template <size_t I, typename Callback>
        inline static void get_by_index(const size_t index, Callback&& callback) {
            if constexpr (I == sizeof...(T)) {
                return;
            }
            else {
                if (I == index) {
                    callback(tag<std::tuple_element_t<I, std::tuple<T...>>>{});
                }
                else {
                    get_by_index<I + 1>(index, std::forward<Callback>(callback));
                }
            }
        }

    public:

        inline constexpr static size_t size = sizeof...(T);


        /**
         * Invokes callback(tag<T>, integral_constant<size_t, I>) for each T, in order.
         */
        template <typename Callback>
        inline static void for_each(Callback&& callback) {
            invoke<0>(std::forward<Callback>(callback));
        }


        /**
        * Invokes callback(tag<T>) for the T at the given index.
        * If index is out of bounds, then callback will not be invoked.
        */
        template <typename Callback>
        inline static void get(const size_t index, Callback&& callback) {
            get_by_index<0>(index, std::forward<Callback>(callback));
        }

    };

}
