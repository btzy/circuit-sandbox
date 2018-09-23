/*
 * Circuit Sandbox
 * Copyright 2018 National University of Singapore <enterprise@nus.edu.sg>
 *
 * This file is part of Circuit Sandbox.
 * Circuit Sandbox is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 3.
 * Circuit Sandbox is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License along with Circuit Sandbox.  If not, see <https://www.gnu.org/licenses/>.
 */

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
