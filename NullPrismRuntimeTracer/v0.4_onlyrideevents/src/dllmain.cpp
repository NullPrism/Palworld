#include <algorithm>
#include <atomic>
#include <chrono>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>

#include <DynamicOutput/Output.hpp>
#include <Helpers/String.hpp>
#include <Mod/CppUserModBase.hpp>
#include <UE4SSRuntime.hpp>
#include <Unreal/Hooks.hpp>
#include <Unreal/UObject.hpp>
#include <Unreal/CoreUObject/UObject/Class.hpp>

namespace NullPrism
{
    using namespace RC;
    using namespace RC::Unreal;

    class RuntimeTracer final : public RC::CppUserModBase
    {
    private:
        using Clock = std::chrono::steady_clock;

        static inline std::atomic_bool s_capture_active{false};
        static inline std::atomic<std::int64_t> s_capture_end_ns{0};
        static inline std::atomic_uint64_t s_capture_number{0};
        static inline std::atomic_uint64_t s_event_number{0};

        static inline std::mutex s_log_mutex{};
        static inline std::mutex s_count_mutex{};

        static inline std::unordered_map<std::string, std::uint32_t>
            s_function_counts{};

        static constexpr auto CAPTURE_DURATION =
            std::chrono::milliseconds{1500};

        static constexpr std::size_t MAX_PARAMETER_BYTES = 256;
        static constexpr std::uint64_t MAX_EVENTS_PER_CAPTURE = 1000;
        static constexpr std::uint32_t MAX_CALLS_PER_FUNCTION = 40;

        static auto now_ns() -> std::int64_t
        {
            return std::chrono::duration_cast<std::chrono::nanoseconds>(
                Clock::now().time_since_epoch()
            ).count();
        }

        static auto to_lower_ascii(std::string value) -> std::string
        {
            std::transform(
                value.begin(),
                value.end(),
                value.begin(),
                [](unsigned char character)
                {
                    return static_cast<char>(std::tolower(character));
                }
            );

            return value;
        }

        static auto contains(
            const std::string& haystack,
            std::string_view needle
        ) -> bool
        {
            return haystack.find(needle) != std::string::npos;
        }

        static auto is_trigger(
            const std::string& lower_function_name
        ) -> bool
        {
            return contains(
                lower_function_name,
                ":onstartcooprequest"
            );
        }

        static auto is_interesting(
            const std::string& lower_context_name,
            const std::string& lower_function_name
        ) -> bool
        {
            const auto searchable =
                lower_context_name + " " + lower_function_name;

            static constexpr std::string_view terms[] = {
                "ride",
                "rider",
                "riding",
                "mount",
                "saddle",
                "coop",
                "otomo",
                "partner",
                "supportpal",
                "palskill"
            };

            for (const auto term : terms)
            {
                if (contains(searchable, term))
                {
                    return true;
                }
            }

            return false;
        }

        static auto is_noise(
            const std::string& lower_function_name
        ) -> bool
        {
            static constexpr std::string_view noise[] = {
                ":getspawnedotomoid",
                ":isrunning",
                ":receivetick",
                ":tick",
                ":actiontick",
                ":received_notifytick",
                ":blueprintthreadsafeupdateanimation",
                ":blueprintpostevaluateanimation",
                ":blueprintupdateanimation",
                ":onpaint",
                ":receivedrawhud",
                ":calcscreenposition",
                ":setcolorandopacity",
                ":setoffsets",
                ":getvisibility",
                ":gettext",
                ":getbrush"
            };

            for (const auto term : noise)
            {
                if (contains(lower_function_name, term))
                {
                    return true;
                }
            }

            return false;
        }

        static auto bytes_to_hex(
            const void* data,
            std::size_t size
        ) -> std::string
        {
            if (data == nullptr)
            {
                return "<null>";
            }

            if (size == 0)
            {
                return "<no parameter bytes>";
            }

            const auto* bytes =
                static_cast<const std::uint8_t*>(data);

            const auto bytes_to_read =
                std::min(size, MAX_PARAMETER_BYTES);

            std::ostringstream output;

            output
                << std::hex
                << std::uppercase
                << std::setfill('0');

            for (std::size_t offset = 0;
                 offset < bytes_to_read;
                 ++offset)
            {
                if (offset != 0)
                {
                    output << ' ';
                }

                output
                    << std::setw(2)
                    << static_cast<unsigned int>(bytes[offset]);
            }

            if (size > bytes_to_read)
            {
                output << " ...";
            }

            return output.str();
        }

        static auto begin_capture(
            const StringType& context_name,
            const StringType& function_name
        ) -> void
        {
            const auto capture_number =
                s_capture_number.fetch_add(
                    1,
                    std::memory_order_relaxed
                ) + 1;

            s_event_number.store(0, std::memory_order_relaxed);

            {
                std::scoped_lock lock{s_count_mutex};
                s_function_counts.clear();
            }

            const auto end_time =
                Clock::now() + CAPTURE_DURATION;

            s_capture_end_ns.store(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    end_time.time_since_epoch()
                ).count(),
                std::memory_order_release
            );

            s_capture_active.store(
                true,
                std::memory_order_release
            );

            std::scoped_lock lock{s_log_mutex};

            Output::send<LogLevel::Warning>(
                STR(
                    "[NullPrismTracerV4] ===== CAPTURE {} START =====\n"
                ),
                capture_number
            );

            Output::send(
                STR("[NullPrismTracerV4] Trigger context: {}\n"),
                context_name
            );

            Output::send(
                STR("[NullPrismTracerV4] Trigger function: {}\n"),
                function_name
            );
        }

        static auto finish_capture_if_expired() -> void
        {
            if (!s_capture_active.load(std::memory_order_acquire))
            {
                return;
            }

            if (now_ns()
                <= s_capture_end_ns.load(std::memory_order_acquire))
            {
                return;
            }

            s_capture_active.store(
                false,
                std::memory_order_release
            );

            std::scoped_lock lock{s_log_mutex};

            Output::send<LogLevel::Warning>(
                STR(
                    "[NullPrismTracerV4] ===== CAPTURE {} END; "
                    "{} event(s) logged =====\n"
                ),
                s_capture_number.load(std::memory_order_relaxed),
                s_event_number.load(std::memory_order_relaxed)
            );
        }

        static auto should_log(
            const std::string& function_name
        ) -> bool
        {
            if (s_event_number.load(std::memory_order_relaxed)
                >= MAX_EVENTS_PER_CAPTURE)
            {
                return false;
            }

            std::scoped_lock lock{s_count_mutex};

            auto& count = s_function_counts[function_name];

            if (count >= MAX_CALLS_PER_FUNCTION)
            {
                return false;
            }

            ++count;

            s_event_number.fetch_add(
                1,
                std::memory_order_relaxed
            );

            return true;
        }

        static auto log_event(
            std::string_view phase,
            UObject* context,
            UFunction* function,
            void* parameters
        ) -> void
        {
            if (context == nullptr || function == nullptr)
            {
                return;
            }

            const auto context_full_name =
                context->GetFullName();

            const auto function_full_name =
                function->GetFullName();

            const auto context_name =
                RC::to_string(context_full_name);

            const auto function_name =
                RC::to_string(function_full_name);

            const auto lower_context =
                to_lower_ascii(context_name);

            const auto lower_function =
                to_lower_ascii(function_name);

            if (is_trigger(lower_function)
                && phase == "PRE")
            {
                begin_capture(
                    context_full_name,
                    function_full_name
                );
            }

            finish_capture_if_expired();

            if (!s_capture_active.load(std::memory_order_acquire))
            {
                return;
            }

            if (!is_interesting(lower_context, lower_function))
            {
                return;
            }

            if (is_noise(lower_function))
            {
                return;
            }

            const auto count_key =
                std::string{phase} + " " + function_name;

            if (!should_log(count_key))
            {
                return;
            }

            const auto parameter_size =
                static_cast<std::size_t>(
                    function->GetParmsSize()
                );

            const auto parameter_hex =
                bytes_to_hex(
                    parameters,
                    parameter_size
                );

            const auto event_number =
                s_event_number.load(
                    std::memory_order_relaxed
                );

            std::scoped_lock lock{s_log_mutex};

            Output::send<LogLevel::Warning>(
                STR(
                    "[NullPrismTracerV4] [{}:{}] {} {}\n"
                ),
                s_capture_number.load(
                    std::memory_order_relaxed
                ),
                event_number,
                RC::ensure_str(std::string{phase}),
                function_full_name
            );

            Output::send(
                STR("[NullPrismTracerV4] Context: {}\n"),
                context_full_name
            );

            Output::send(
                STR(
                    "[NullPrismTracerV4] Params size: {}\n"
                ),
                parameter_size
            );

            Output::send(
                STR(
                    "[NullPrismTracerV4] Params hex: {}\n"
                ),
                RC::ensure_str(parameter_hex)
            );
        }

        static auto process_event_pre_callback(
            Hook::TCallbackIterationData<void>&,
            UObject* context,
            UFunction* function,
            void* parameters
        ) -> void
        {
            log_event(
                "PRE",
                context,
                function,
                parameters
            );
        }

        static auto process_event_post_callback(
            Hook::TCallbackIterationData<void>&,
            UObject* context,
            UFunction* function,
            void* parameters
        ) -> void
        {
            log_event(
                "POST",
                context,
                function,
                parameters
            );
        }

    public:
        RuntimeTracer()
        {
            ModVersion = STR("0.4.0");
            ModName = STR("NullPrism Runtime Tracer");
            ModAuthors = STR("NullPrism");
            ModDescription = STR(
                "Triggered ride and co-op ProcessEvent tracer"
            );

            Output::send<LogLevel::Warning>(
                STR(
                    "[NullPrismTracerV4] DLL constructed. "
                    "Version 0.4.0.\n"
                )
            );
        }

        ~RuntimeTracer() override
        {
            Output::send<LogLevel::Warning>(
                STR("[NullPrismTracerV4] DLL destructed.\n")
            );
        }

        auto on_unreal_init() -> void override
        {
            Output::send<LogLevel::Warning>(
                STR("[NullPrismTracerV4] Unreal initialized.\n")
            );

            if (!UE4SSRuntime::IsProcessEventAvailable())
            {
                Output::send<LogLevel::Error>(
                    STR(
                        "[NullPrismTracerV4] ProcessEvent unavailable.\n"
                    )
                );

                return;
            }

            Hook::RegisterProcessEventPreCallback(
                process_event_pre_callback,
                {
                    false,
                    false,
                    STR("NullPrism"),
                    STR("RuntimeTracerV4Pre")
                }
            );

            Hook::RegisterProcessEventPostCallback(
                process_event_post_callback,
                {
                    false,
                    false,
                    STR("NullPrism"),
                    STR("RuntimeTracerV4Post")
                }
            );

            Output::send<LogLevel::Warning>(
                STR(
                    "[NullPrismTracerV4] PRE and POST callbacks "
                    "registered. Waiting for OnStartCoopRequest.\n"
                )
            );
        }
    };
}

#define MOD_EXPORT __declspec(dllexport)

extern "C"
{
    MOD_EXPORT RC::CppUserModBase* start_mod()
    {
        return new NullPrism::RuntimeTracer();
    }

    MOD_EXPORT void uninstall_mod(
        RC::CppUserModBase* mod
    )
    {
        delete mod;
    }
}
