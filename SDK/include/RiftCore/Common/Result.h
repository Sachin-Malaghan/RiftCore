// ============================================================
// Result.h
// Type-safe error handling — no raw exceptions in engine API.
// Every function that can fail returns Result<T> or Result<>.
//
// Usage:
//   Result<int> Divide(int a, int b) {
//       if (b == 0) return Err("Division by zero");
//       return Ok(a / b);
//   }
//
//   auto result = Divide(10, 2);
//   if (result.IsOk())  { int val = result.Value(); }
//   if (result.IsErr()) { auto msg = result.Error();  }
// ============================================================
#pragma once

#include "Platform.h"
#include "Types.h"
#include <variant>
#include <optional>
#include <stdexcept>
#include <functional>

namespace RiftCore {

    // ── Error Type ───────────────────────────────────────────
    struct EngineError {
        String  message;
        i32     code    = 0;
        String  file;
        i32     line    = 0;

        EngineError() = default;
        EngineError(String msg, i32 code = -1,
                    String file = "", i32 line = 0)
            : message(std::move(msg))
            , code(code)
            , file(std::move(file))
            , line(line)
        {}

        String ToString() const {
            if (file.empty()) return message;
            return message + " [" + file + ":" + std::to_string(line) + "]";
        }
    };

    // ── Result<T> ────────────────────────────────────────────
    // Holds either a value T or an EngineError
    template<typename T>
    class Result {
    public:
        // Construct success
        static Result Ok(T value) {
            Result r;
            r.storage_ = std::move(value);
            return r;
        }

        // Construct failure
        static Result Err(EngineError error) {
            Result r;
            r.storage_ = std::move(error);
            return r;
        }

        static Result Err(String message, i32 code = -1) {
            return Err(EngineError{std::move(message), code});
        }

        // ── Query ─────────────────────────────────────────
        bool IsOk()  const { return std::holds_alternative<T>(storage_); }
        bool IsErr() const { return std::holds_alternative<EngineError>(storage_); }

        // ── Access value (only call if IsOk()) ────────────
        T& Value() {
            RIFTCORE_ASSERT_MSG(IsOk(), "Accessing Value() on an error Result");
            return std::get<T>(storage_);
        }

        const T& Value() const {
            RIFTCORE_ASSERT_MSG(IsOk(), "Accessing Value() on an error Result");
            return std::get<T>(storage_);
        }

        // ── Access error (only call if IsErr()) ───────────
        EngineError& Error() {
            RIFTCORE_ASSERT_MSG(IsErr(), "Accessing Error() on a success Result");
            return std::get<EngineError>(storage_);
        }

        const EngineError& Error() const {
            RIFTCORE_ASSERT_MSG(IsErr(), "Accessing Error() on a success Result");
            return std::get<EngineError>(storage_);
        }

        // ── Value or default ──────────────────────────────
        T ValueOr(T defaultValue) const {
            if (IsOk()) return std::get<T>(storage_);
            return defaultValue;
        }

        // ── Monadic map: transform value if Ok ────────────
        template<typename Fn>
        auto Map(Fn&& fn) const -> Result<decltype(fn(std::declval<T>()))> {
            using U = decltype(fn(std::declval<T>()));
            if (IsOk()) return Result<U>::Ok(fn(Value()));
            return Result<U>::Err(Error());
        }

        // ── Unwrap (crashes if error — use in tests only) ─
        T Unwrap() const {
            if (IsErr()) {
                RIFTCORE_ASSERT_MSG(false, Error().message.c_str());
            }
            return std::get<T>(storage_);
        }

    private:
        Result() = default;
        std::variant<T, EngineError> storage_;
    };

    // ── Result<void> specialization ──────────────────────────
    // For functions that either succeed or fail with no return value
    template<>
    class Result<void> {
    public:
        static Result Ok() {
            Result r;
            r.isOk_ = true;
            return r;
        }

        static Result Err(EngineError error) {
            Result r;
            r.isOk_  = false;
            r.error_ = std::move(error);
            return r;
        }

        static Result Err(String message, i32 code = -1) {
            return Err(EngineError{std::move(message), code});
        }

        bool IsOk()  const { return isOk_;  }
        bool IsErr() const { return !isOk_; }

        EngineError& Error() {
            RIFTCORE_ASSERT_MSG(IsErr(), "Accessing Error() on a success Result<void>");
            return error_;
        }

        const EngineError& Error() const {
            RIFTCORE_ASSERT_MSG(IsErr(), "Accessing Error() on a success Result<void>");
            return error_;
        }

        void Unwrap() const {
            if (IsErr()) {
                RIFTCORE_ASSERT_MSG(false, error_.message.c_str());
            }
        }

    private:
        Result() = default;
        bool        isOk_  = false;
        EngineError error_;
    };

    // ── Convenience aliases ──────────────────────────────────
    using VoidResult = Result<void>;

    // ── Helper macros ────────────────────────────────────────
    // Propagate error upward (like Rust's ? operator)
    // Usage: RIFTCORE_TRY(SomeFunction());
    #define RIFTCORE_TRY(expr)                      \
        do {                                         \
            auto _result = (expr);                   \
            if (_result.IsErr()) {                   \
                return decltype(_result)::Err(       \
                    _result.Error()                  \
                );                                   \
            }                                        \
        } while(0)

    // Make error with file/line info
    #define RIFTCORE_ERR(msg) \
        RiftCore::EngineError{(msg), -1, __FILE__, __LINE__}

    #define RIFTCORE_ERR_CODE(msg, code) \
        RiftCore::EngineError{(msg), (code), __FILE__, __LINE__}

} // namespace RiftCore