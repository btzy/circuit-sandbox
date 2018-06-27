#pragma once

/* All functions in unicode.h will not throw as long as callbacks don't throw */

#include <utility>
#include <string>
#include <type_traits>

namespace ext {

	enum : char {
		TAB = 0x09,
		LF = 0x0A,
		CR = 0x0D
	};

	// from here: http://stackoverflow.com/questions/1031645/how-to-detect-utf-8-in-plain-c

	// callback(const char* begin, size_t length) -> bool (return false to break)
	// terminate(const char* current) -> bool (return true to terminate)
	// returns true if this is a valid UTF-8 std::string
	template <typename TTerminateChecker, typename TCallback>
	inline bool basic_utf8_foreach_breakable(const char* s_bytes, TTerminateChecker terminate, TCallback callback) {
		
		while (!terminate(s_bytes))
		{
			const unsigned char* const bytes = reinterpret_cast<const unsigned char*>(s_bytes);
			if (
				bytes[0] == TAB || // TAB
				bytes[0] == LF || // LF
				bytes[0] == CR    // CR
				) {
				if (!callback(s_bytes, 1))return false;
				s_bytes += 1;
				continue;
			}
			if (// ASCII
				// use bytes[0] <= 0x7F to allow ASCII control characters
				(0x20 <= bytes[0] && bytes[0] <= 0x7E)
				) {
				if (!callback(s_bytes, 1))return false;
				s_bytes += 1;
				continue;
			}

			if ((// non-overlong 2-byte
				(0xC2 <= bytes[0] && bytes[0] <= 0xDF) &&
				(0x80 <= bytes[1] && bytes[1] <= 0xBF)
				)
				) {
				if (!callback(s_bytes, 2))return false;
				s_bytes += 2;
				continue;
			}

			if ((// excluding overlongs
				bytes[0] == 0xE0 &&
				(0xA0 <= bytes[1] && bytes[1] <= 0xBF) &&
				(0x80 <= bytes[2] && bytes[2] <= 0xBF)
				) ||
				(// straight 3-byte
				((0xE1 <= bytes[0] && bytes[0] <= 0xEC) ||
					bytes[0] == 0xEE ||
					bytes[0] == 0xEF) &&
					(0x80 <= bytes[1] && bytes[1] <= 0xBF) &&
					(0x80 <= bytes[2] && bytes[2] <= 0xBF)
					) ||
					(// excluding surrogates
						bytes[0] == 0xED &&
						(0x80 <= bytes[1] && bytes[1] <= 0x9F) &&
						(0x80 <= bytes[2] && bytes[2] <= 0xBF)
						)
				) {
				if (!callback(s_bytes, 3))return false;
				s_bytes += 3;
				continue;
			}

			if ((// planes 1-3
				bytes[0] == 0xF0 &&
				(0x90 <= bytes[1] && bytes[1] <= 0xBF) &&
				(0x80 <= bytes[2] && bytes[2] <= 0xBF) &&
				(0x80 <= bytes[3] && bytes[3] <= 0xBF)
				) ||
				(// planes 4-15
				(0xF1 <= bytes[0] && bytes[0] <= 0xF3) &&
					(0x80 <= bytes[1] && bytes[1] <= 0xBF) &&
					(0x80 <= bytes[2] && bytes[2] <= 0xBF) &&
					(0x80 <= bytes[3] && bytes[3] <= 0xBF)
					) ||
					(// plane 16
						bytes[0] == 0xF4 &&
						(0x80 <= bytes[1] && bytes[1] <= 0x8F) &&
						(0x80 <= bytes[2] && bytes[2] <= 0xBF) &&
						(0x80 <= bytes[3] && bytes[3] <= 0xBF)
						)
				) {
				if (!callback(s_bytes, 4))return false;
				s_bytes += 4;
				continue;
			}

			return false;
		}

		return true;
	}

	//template <typename T> struct returns_bool : public std::false_type {};
	//template <typename T> struct returns_bool<std::optional<T>> : public std::true_type {};

	template <typename TTerminateChecker, typename TCallback>
	inline bool basic_utf8_foreach_impl(const char* const bytes, TTerminateChecker terminate, TCallback callback, std::false_type) {
		return basic_utf8_foreach_breakable(bytes, std::move(terminate), [callback = std::move(callback)](const char* const begin, const size_t length) {
			callback(begin, length);
			return true;
		});
	}

	template <typename TTerminateChecker, typename TCallback>
	inline bool basic_utf8_foreach_impl(const char* const bytes, TTerminateChecker terminate, TCallback callback, std::true_type) {
		return basic_utf8_foreach_breakable(bytes, std::move(terminate), callback);
	}

	template <typename TTerminateChecker, typename TCallback>
	inline bool basic_utf8_foreach(const char* const bytes, TTerminateChecker terminate, TCallback callback) {
		return basic_utf8_foreach_impl(bytes, terminate, callback, std::is_same<bool, typename std::decay<typename std::result_of<TCallback(const char*, std::size_t)>::type>::type>());
	}

	// callback(const char* begin, size_t length)
	template <typename TCallback>
	inline bool utf8_foreach(const char* const str, TCallback callback) {
		return basic_utf8_foreach(str, [](const char* const ch) {
			return *ch == 0x00;
		}, std::move(callback));
	}

	// callback(const char* begin, size_t length)
	template <typename TCallback>
	inline bool utf8_foreach(const char* const str, size_t length, TCallback callback) {
		const char* const end = str + length;
		return basic_utf8_foreach(str, [end](const char* const ch) {
			return ch == end;
		}, std::move(callback));
	}

	// callback(const char* begin, size_t length)
	// return false from callback to break
	// this function returns false if breaked or if not valid utf8, true otherwise
	template <typename TCallback>
	inline bool utf8_foreach(const std::string& str, TCallback callback) {
		return utf8_foreach(str.c_str(), std::move(callback));
	}

	inline bool utf8_validate_no_TAB_LF_CR(const std::string& str) {
		return utf8_foreach(str.c_str(), [](const char* const begin, const size_t length) {
			if (length == 1 && (begin[0] == 0x09 || begin[0] == 0x0A || begin[0] == 0x0D)) {
				return false;
			}
			return true;
		});
	}

	inline bool utf8_validate(const std::string& str) {
		return utf8_foreach(str.c_str(), [](const char* const begin, const size_t length) {
			return true;
		});
	}

	// this function strips all CR from the UTF-8 std::string
	inline bool utf8_normalize_newline(const std::string& str, std::string& out) {
		out.clear();
		bool res = utf8_foreach(str, [&out](const char* const begin, const size_t length) {
			if (!(length == 1 && begin[0] == 0x0D)) {
				out.append(begin, length);
			}
		});
		if (!res) {
			out.clear();
			return false;
		}
		return true;
	}

	// this function strips all CR from the UTF-8 std::string
	// if the std::string is not valid UTF-8, str is cleared and returns false
	inline bool utf8_normalize_newline(std::string& str) {
		std::string ret;
		bool res = utf8_normalize_newline(str, ret);
		str = std::move(ret);
		return res;
	}

	// this function replaces all consecutive LF with a single LF, removes leading or trailing LF, and strips all TAB and CR from the UTF-8 std::string
	inline bool utf8_normalize_TAB_LF_CR(const std::string& str, std::string& out) {
		out.clear();
		
		bool already_has_LF = true;
		bool res = ext::utf8_foreach(std::move(str), [&out, &already_has_LF](const char* const begin, const size_t length) {
			if (length == 1 && (begin[0] == ext::TAB || begin[0] == ext::CR)) {
				// don't copy TABs and CRs
			}
			else if (length == 1 && begin[0] == ext::LF) {
				if (!already_has_LF) {
					out.push_back(ext::LF);
					already_has_LF = true;
				}
			}
			else {
				out.append(begin, length);
				already_has_LF = false;
			}
		});
		
		
		if (!res) {
			out.clear();
			return false;
		}
		else {
			if (!out.empty() && out.back() == ext::LF) {
				out.pop_back(); // remove trailing LF if any
			}
			return true;
		}

	}

	// this function replaces all consecutive LF with a single LF, removes leading or trailing LF, and strips all TAB and CR from the UTF-8 std::string
	// if the std::string is not valid UTF-8, str is cleared and returns false
	inline bool utf8_normalize_TAB_LF_CR(std::string& str) {
		std::string ret;
		bool res = utf8_normalize_TAB_LF_CR(str, ret);
		str = std::move(ret);
		return res;
	}
}