#pragma once

/**
 * Helper library for tag-based dispatching.
 * Something like visiting a variant, but without the values.
 */

#include <tuple> // for std::tuple
#include <utility> // for std::forward
#include <type_traits> // for std::integral_constant

namespace ext {

    template <typename T>
    struct tag{
        using type = T;
    };

    template <typename... T>
    struct tag_tuple{

    private:

        // Note: Verbose multiple functions for invoke is due Visual Studio choking on nested if-constexpr.
        // See https://stackoverflow.com/q/50857307/1021959
        template <size_t I, typename Callback>
        inline static bool invoke(Callback&& callback) {
            if constexpr (I == sizeof...(T)) {
                return false;
            }
            else {
                return _invoke_impl<I>(std::forward<Callback>(callback));
            }
        }

        template <size_t I, typename Callback>
        inline static bool _invoke_impl(Callback&& callback) {
            if constexpr (std::is_convertible_v<std::invoke_result_t<Callback, tag<std::tuple_element_t<I, std::tuple<T...>>>, std::integral_constant<size_t, I>>, bool>) {
                if (callback(tag<std::tuple_element_t<I, std::tuple<T...>>>{}, std::integral_constant<size_t, I>{})) {
                    return true;
                }
            }
            else {
                callback(tag<std::tuple_element_t<I, std::tuple<T...>>>{}, std::integral_constant<size_t, I>{});
            }
            return invoke<I + 1>(std::forward<Callback>(callback));
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

        template <size_t I, typename Callback, typename DefaultReturnValue>
        inline static auto get_by_index(const size_t index, Callback&& callback, DefaultReturnValue&& defaultRet) {
            if constexpr (I == sizeof...(T)) {
                return std::forward<DefaultReturnValue>(defaultRet);
            }
            else {
                if (I == index) {
                    return callback(tag<std::tuple_element_t<I, std::tuple<T...>>>{});
                }
                else {
                    return get_by_index<I + 1>(index, std::forward<Callback>(callback), std::forward<DefaultReturnValue>(defaultRet));
                }
            }
        }

    public:

        inline constexpr static size_t size = sizeof...(T);


        /**
         * Invokes callback(tag<T>, integral_constant<size_t, I>) for each T, in order.
         * If any callback returns true, immediately break and return true.
         * Otherwise it returns false.
         * Also accepts callback that do not return any value.
         */
        template <typename Callback>
        inline static bool for_each(Callback&& callback) {
            return invoke<0>(std::forward<Callback>(callback));
        }


        /**
         * Invokes callback(tag<T>) for the T at the given index.
         * If index is out of bounds, then callback will not be invoked.
         * The three-parameter version allows returning a value from the callback.  If index is out of bounds, then defaultRet will be returned.
         */
        template <typename Callback>
        inline static void get(const size_t index, Callback&& callback) {
            get_by_index<0>(index, std::forward<Callback>(callback));
        }
        template <typename Callback, typename DefaultReturnValue>
        inline static auto get(const size_t index, Callback&& callback, DefaultReturnValue&& defaultRet) {
            return get_by_index<0>(index, std::forward<Callback>(callback), std::forward<DefaultReturnValue>(defaultRet));
        }

        /**
         * Generic template instantiation helper
         */
        template <template <typename...> typename TemplateType>
        using instantiate = TemplateType<T...>;

    };

}
