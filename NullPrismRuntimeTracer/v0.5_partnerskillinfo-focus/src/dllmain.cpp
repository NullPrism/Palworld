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

    class RuntimeTracer final : public CppUserModBase
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
            std::chrono::milliseconds{1200};

        static constexpr std::size_t MAX_PARAMETER_BYTES = 256;
        static constexpr std::uint64_t MAX_EVENTS_PER_CAPTURE = 800;
        static constexpr std::uint32_t MAX_CALLS_PER_FUNCTION = 30;

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
            const std::string& value,
            std::string_view term
        ) -> bool
        {
            return value.find(term) != std::string::npos;
        }

        static auto is_trigger(
            const std::string& function_name
        ) -> bool
        {
            return contains(
                function_name,
                "wbp_partnerskillinfo_c:triggerinput"
            );
        }

        static auto is_relevant(
            const std::string& context_name,
            const std::string& function_name
        ) -> bool
        {
            const auto combined =
                context_name + " " + function_name;

            static constexpr std::string_view context_terms[] = {
                "wbp_partnerskillinfo",
                "palpartnerskill",
                "partner skill",
                "bp_palplayercontroller",
                "palplayercontroller",
                "bp_otomopalholdercomponent",
                "palridercomponent",
                "palridemarkercomponent",
                "bp_monsteraicontroller_otomo",
                "bp_aiactionridecall"
            };

            for (const auto term : context_terms)
            {
                if (contains(combined, term))
                {
                    return true;
                }
            }

            static constexpr std::string_view function_terms[] = {
                "triggerinput",
                "releaseinput",
                "restricted",
                "restriction",
                "unlock",
                "locked",
                "required",
                "requirement",
                "item",
                "equip",
                "technology",
                "recipe",
                "canuse",
                "canactivate",
                "canexecute",
                "canride",
                "canmount",
                "cancoop",
                "requestcoop",
                "cooprequest",
                "startcoop",
                "endcoop",
                "ride",
                "rider",
                "mount",
                "saddle",
                "otomo",
                "supportpal"
            };

            for (const auto term : function_terms)
            {
                if (contains(function_name, term))
                {
                    return true;
                }
            }

            return false;
        }

        static auto is_noise(
            const std::string& function_name
        ) -> bool
        {
            static constexpr std::string_view terms[] = {
                ":tick",
                ":ontick",
                ":receivetick",
                ":actiontick",
                ":isrunning",
                ":received_notifytick",
                ":received_notifybegin",
                ":received_notifyend",
                ":blueprintthreadsafeupdateanimation",
                ":blueprintpostevaluateanimation",
                ":blueprintupdateanimation",
                ":onanimationstarted",
                ":onanimationfinished",
                ":onpaint",
                ":receivedrawhud",
                ":updatehp",
                ":updatehunger",
                ":updatesp",
                ":getspawnedotomoid"
            };

            for (const auto term : terms)
            {
                if (contains(function_name, term))
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

            const auto length =
                std::min(size, MAX_PARAMETER_BYTES);

            std::ostringstream output;

            output
                << std::hex
                << std::uppercase
                << std::setfill('0');

            for (std::size_t index = 0; index < length; ++index)
            {
                if (index != 0)
                {
                    output << ' ';
                }

                output
                    << std::setw(2)
                    << static_cast<unsigned int>(bytes[index]);
            }

            if (size > length)
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
                    "[NullPrismTracerV5] ===== CAPTURE {} START =====\n"
                ),
                capture_number
            );

            Output::send(
                STR("[NullPrismTracerV5] Trigger context: {}\n"),
                context_name
            );

            Output::send(
                STR("[NullPrismTracerV5] Trigger function: {}\n"),
                function_name
            );
        }

        static auto finish_capture_if_expired() -> void
        {
            if (!s_capture_active.load(std::memory_order_acquire))
            {
                return;
            }

            if (now_ns() <=
                s_capture_end_ns.load(std::memory_order_acquire))
            {
                return;
            }

            s_capture_active.store(false, std::memory_order_release);

            std::scoped_lock lock{s_log_mutex};

            Output::send<LogLevel::Warning>(
                STR(
                    "[NullPrismTracerV5] ===== CAPTURE {} END; "
                    "{} event(s) logged =====\n"
                ),
                s_capture_number.load(std::memory_order_relaxed),
                s_event_number.load(std::memory_order_relaxed)
            );
        }

        static auto reserve_event(
            const std::string& key
        ) -> std::uint64_t
        {
            if (s_event_number.load(std::memory_order_relaxed) >=
                MAX_EVENTS_PER_CAPTURE)
            {
                return 0;
            }

            std::scoped_lock lock{s_count_mutex};

            auto& count = s_function_counts[key];

            if (count >= MAX_CALLS_PER_FUNCTION)
            {
                return 0;
            }

            ++count;

            return s_event_number.fetch_add(
                1,
                std::memory_order_relaxed
            ) + 1;
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

            if (phase == "PRE" && is_trigger(lower_function))
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

            if (!is_relevant(lower_context, lower_function))
            {
                return;
            }

            if (is_noise(lower_function))
            {
                return;
            }

            const auto event_number = reserve_event(
                std::string{phase} + " " + function_name
            );

            if (event_number == 0)
            {
                return;
            }

            const auto parameter_size =
                static_cast<std::size_t>(
                    function->GetParmsSize()
                );

            const auto parameter_hex =
                bytes_to_hex(parameters, parameter_size);

            std::scoped_lock lock{s_log_mutex};

            Output::send<LogLevel::Warning>(
                STR(
                    "[NullPrismTracerV5] [{}:{}] {} {}\n"
                ),
                s_capture_number.load(std::memory_order_relaxed),
                event_number,
                RC::ensure_str(std::string{phase}),
                function_full_name
            );

            Output::send(
                STR("[NullPrismTracerV5] Context: {}\n"),
                context_full_name
            );

            Output::send(
                STR("[NullPrismTracerV5] Params size: {}\n"),
                parameter_size
            );

            Output::send(
                STR("[NullPrismTracerV5] Params hex: {}\n"),
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
            log_event("PRE", context, function, parameters);
        }

        static auto process_event_post_callback(
            Hook::TCallbackIterationData<void>&,
            UObject* context,
            UFunction* function,
            void* parameters
        ) -> void
        {
            log_event("POST", context, function, parameters);
        }

    public:
        RuntimeTracer()
        {
            ModVersion = STR("0.5.0");
            ModName = STR("NullPrism Runtime Tracer");
            ModAuthors = STR("NullPrism");
            ModDescription = STR(
                "Partner Skill input and ride-gate tracer"
            );

            Output::send<LogLevel::Warning>(
                STR(
                    "[NullPrismTracerV5] DLL constructed. "
                    "Version 0.5.0.\n"
                )
            );
        }

        ~RuntimeTracer() override
        {
            Output::send<LogLevel::Warning>(
                STR("[NullPrismTracerV5] DLL destructed.\n")
            );
        }

        auto on_unreal_init() -> void override
        {
            Output::send<LogLevel::Warning>(
                STR("[NullPrismTracerV5] Unreal initialized.\n")
            );

            if (!UE4SSRuntime::IsProcessEventAvailable())
            {
                Output::send<LogLevel::Error>(
                    STR(
                        "[NullPrismTracerV5] ProcessEvent unavailable.\n"
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
                    STR("RuntimeTracerV5Pre")
                }
            );

            Hook::RegisterProcessEventPostCallback(
                process_event_post_callback,
                {
                    false,
                    false,
                    STR("NullPrism"),
                    STR("RuntimeTracerV5Post")
                }
            );

            Output::send<LogLevel::Warning>(
                STR(
                    "[NullPrismTracerV5] PRE and POST callbacks "
                    "registered. Waiting for TriggerInput.\n"
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

    MOD_EXPORT void uninstall_mod(CppUserModBase* mod)
    {
        delete mod;
    }
}
