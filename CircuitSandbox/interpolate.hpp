#pragma once

/**
 * Interpolation library.
 */

namespace ext {
    /**
     * Interpolate between two values.
     * Returns the value that corresponds to source_value.
     */
    template <typename WidenedType, typename Source, typename Target>
    inline Target interpolate(Source source_begin, Source source_end, Target target_begin, Target target_end, Source source_value) {
        assert(source_begin != source_end);
        return ((static_cast<WidenedType>(target_begin) * source_end - static_cast<WidenedType>(target_end) * source_begin) + static_cast<WidenedType>(target_end - target_begin) * source_value) / (source_end - source_begin);
    }

    template <typename Source, typename Target>
    inline Target interpolate(Source source_begin, Source source_end, Target target_begin, Target target_end, Source source_value) {
        return interpolate<Target>(source_begin, source_end, target_begin, target_end, source_value);
    }

    template <typename Source, typename Target>
    inline Target interpolate_safe(Source source_begin, Source source_end, Target target_begin, Target target_end, Source source_value) {
        return interpolate(static_cast<Source>(0), source_end - source_begin, target_begin, target_end, source_value - source_begin);
    }

    template <typename Source, typename Target>
    inline Target interpolate_time(Source source_begin, Source source_end, Target target_begin, Target target_end, Source source_value) {
        return interpolate_safe(source_begin.time_since_epoch().count(), source_end.time_since_epoch().count(), target_begin, target_end, source_value.time_since_epoch().count());
    }
}
