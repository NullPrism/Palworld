#include <algorithm>
#include <atomic>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <mutex>
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
        static inline std::atomic_bool s_end_marker_written{false};

        static inline std::atomic<std::int64_t> s_capture_end_ns{0};
        static inline std::atomic_uint64_t s_capture_number{0};
        static inline std::atomic_uint64_t s_capture_event_number{0};

        static inline std::mutex s_count_mutex{};
        static inline std::unordered_map<std::string, std::uint32_t>
            s_per_function_counts{};

        static constexpr auto CAPTURE_DURATION =
            std::chrono::milliseconds{2000};

        static constexpr std::uint64_t MAX_EVENTS_PER_CAPTURE = 5000;
        static constexpr std::uint32_t MAX_CALLS_PER_FUNCTION = 500;

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

        static auto contains_case_insensitive(
            const std::string& value,
            const std::string_view needle
        ) -> bool
        {
            return to_lower_ascii(value).find(needle)
                != std::string::npos;
        }

        static auto is_capture_trigger(
            const std::string& function_name
        ) -> bool
        {
            return contains_case_insensitive(
                function_name,
                "onstartcooprequest"
            );
        }

        static auto is_pal_related(
            const std::string& context_name,
            const std::string& function_name
        ) -> bool
        {
            const auto searchable =
                to_lower_ascii(context_name + " " + function_name);

            static constexpr std::string_view required_terms[] = {
                "/script/pal.",
                "/game/pal/",
                "bp_pal",
                "wbp_pal",
                "partner",
                "coop",
                "otomo",
                "rider",
                "ride",
                "supportpal"
            };

            for (const auto term : required_terms)
            {
                if (searchable.find(term) != std::string::npos)
                {
                    return true;
                }
            }

            return false;
        }

        static auto is_known_noise(
            const std::string& function_name
        ) -> bool
        {
            const auto name = to_lower_ascii(function_name);

            static constexpr std::string_view noise_terms[] = {
                ":receivetick",
                ":tick",
                ":actiontick",
                ":isrunning",
                ":updateanimation",
                ":blueprintupdateanimation",
                ":animgraph",
                ":evaluategraph",
                ":ontick",
                ":updatewidget",
                ":updateinputicon",
                ":updatestatus",
                ":refresh",
                ":construct",
                ":destruct",
                ":paint",
                ":preconstruct",
                ":getvisibility",
                ":getpercent",
                ":gettext",
                ":getbrush",
                ":getcolorandopacity",
                ":getrenderopacity"
            };

            for (const auto term : noise_terms)
            {
                if (name.find(term) != std::string::npos)
                {
                    return true;
                }
            }

            return false;
        }

        static auto reset_capture_counts() -> void
        {
            s_capture_event_number.store(
                0,
                std::memory_order_relaxed
            );

            std::scoped_lock lock{s_count_mutex};
            s_per_function_counts.clear();
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

            reset_capture_counts();

            const auto end_time =
                Clock::now() + CAPTURE_DURATION;

            const auto end_time_ns =
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    end_time.time_since_epoch()
                ).count();

            s_capture_end_ns.store(
                end_time_ns,
                std::memory_order_release
            );

            s_end_marker_written.store(
                false,
                std::memory_order_release
            );

            s_capture_active.store(
                true,
                std::memory_order_release
            );

            Output::send<LogLevel::Warning>(
                STR(
                    "[NullPrismTracerV2] ===== CAPTURE {} START =====\n"
                ),
                capture_number
            );

            Output::send(
                STR("[NullPrismTracerV2] Trigger context: {}\n"),
                context_name
            );

            Output::send(
                STR("[NullPrismTracerV2] Trigger function: {}\n"),
                function_name
            );

            Output::send(
                STR(
                    "[NullPrismTracerV2] Capture window: {} ms\n"
                ),
                CAPTURE_DURATION.count()
            );
        }

        static auto finish_capture_if_expired() -> bool
        {
            if (!s_capture_active.load(std::memory_order_acquire))
            {
                return false;
            }

            if (now_ns()
                <= s_capture_end_ns.load(std::memory_order_acquire))
            {
                return false;
            }

            s_capture_active.store(
                false,
                std::memory_order_release
            );

            if (!s_end_marker_written.exchange(
                    true,
                    std::memory_order_acq_rel))
            {
                Output::send<LogLevel::Warning>(
                    STR(
                        "[NullPrismTracerV2] ===== CAPTURE {} END; "
                        "{} event(s) logged =====\n"
                    ),
                    s_capture_number.load(
                        std::memory_order_relaxed
                    ),
                    s_capture_event_number.load(
                        std::memory_order_relaxed
                    )
                );
            }

            return true;
        }

        static auto should_log_function(
            const std::string& function_name
        ) -> bool
        {
            const auto current_total =
                s_capture_event_number.load(
                    std::memory_order_relaxed
                );

            if (current_total >= MAX_EVENTS_PER_CAPTURE)
            {
                return false;
            }

            std::scoped_lock lock{s_count_mutex};

            auto& count =
                s_per_function_counts[function_name];

            if (count >= MAX_CALLS_PER_FUNCTION)
            {
                return false;
            }

            ++count;

            s_capture_event_number.fetch_add(
                1,
                std::memory_order_relaxed
            );

            return true;
        }

        static auto process_event_callback(
            Hook::TCallbackIterationData<void>&,
            UObject* context,
            UFunction* function,
            void*
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

            /*
             * The trigger itself is logged separately and starts or
             * refreshes a capture window.
             */
            if (is_capture_trigger(function_name))
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

            if (!is_pal_related(context_name, function_name))
            {
                return;
            }

            if (is_known_noise(function_name))
            {
                return;
            }

            if (!should_log_function(function_name))
            {
                return;
            }

            const auto event_number =
                s_capture_event_number.load(
                    std::memory_order_relaxed
                );

            Output::send(
                STR(
                    "[NullPrismTracerV2] [{}:{}] Context: {}\n"
                ),
                s_capture_number.load(
                    std::memory_order_relaxed
                ),
                event_number,
                context_full_name
            );

            Output::send(
                STR(
                    "[NullPrismTracerV2] [{}:{}] Function: {}\n"
                ),
                s_capture_number.load(
                    std::memory_order_relaxed
                ),
                event_number,
                function_full_name
            );
        }

    public:
        RuntimeTracer()
        {
            ModVersion = STR("0.2.0");
            ModName = STR("NullPrism Runtime Tracer");
            ModAuthors = STR("NullPrism");
            ModDescription = STR(
                "Triggered Palworld ProcessEvent runtime tracer"
            );

            Output::send<LogLevel::Warning>(
                STR(
                    "[NullPrismTracerV2] DLL constructed. "
                    "Version 0.2.0.\n"
                )
            );
        }

        ~RuntimeTracer() override
        {
            Output::send<LogLevel::Warning>(
                STR("[NullPrismTracerV2] DLL destructed.\n")
            );
        }

        auto on_unreal_init() -> void override
        {
            Output::send<LogLevel::Warning>(
                STR("[NullPrismTracerV2] Unreal initialized.\n")
            );

            if (!UE4SSRuntime::IsProcessEventAvailable())
            {
                Output::send<LogLevel::Error>(
                    STR(
                        "[NullPrismTracerV2] ProcessEvent is "
                        "unavailable; UE4SS's AOB scan may have "
                        "failed.\n"
                    )
                );

                return;
            }

            Hook::RegisterProcessEventPreCallback(
                process_event_callback,
                {
                    false,
                    false,
                    STR("NullPrism"),
                    STR("RuntimeTracerV2")
                }
            );

            Output::send<LogLevel::Warning>(
                STR(
                    "[NullPrismTracerV2] ProcessEvent callback "
                    "registered. Waiting for "
                    "OnStartCoopRequest.\n"
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
