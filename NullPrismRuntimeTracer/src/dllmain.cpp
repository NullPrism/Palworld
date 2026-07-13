#include <algorithm>
#include <atomic>
#include <cctype>
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
        static inline std::atomic_uint64_t s_total_matches{0};
        static inline std::mutex s_count_mutex{};
        static inline std::unordered_map<std::string, std::uint32_t> s_function_counts{};

        static constexpr std::uint64_t MAX_TOTAL_MATCHES = 20000;
        static constexpr std::uint32_t MAX_PER_FUNCTION = 250;

        static auto to_lower_ascii(std::string value) -> std::string
        {
            std::transform(
                value.begin(),
                value.end(),
                value.begin(),
                [](unsigned char character) {
                    return static_cast<char>(std::tolower(character));
                }
            );

            return value;
        }

        static auto matches_filter(
            const std::string& context_name,
            const std::string& function_name
        ) -> bool
        {
            const auto searchable = to_lower_ascii(
                context_name + " " + function_name
            );

            static constexpr std::string_view filters[] = {
                "ride",
                "riding",
                "mount",
                "saddle",
                "partner",
                "partnerskill",
                "coop",
                "otomo",
                "supportpal",
                "palskill",
                "playercontroller",
                "keyguide"
            };

            for (const auto filter : filters)
            {
                if (searchable.find(filter) != std::string::npos)
                {
                    return true;
                }
            }

            return false;
        }

        static auto should_log_function(
            const std::string& function_name
        ) -> bool
        {
            if (s_total_matches.load(std::memory_order_relaxed)
                >= MAX_TOTAL_MATCHES)
            {
                return false;
            }

            std::scoped_lock lock{s_count_mutex};

            auto& count = s_function_counts[function_name];

            if (count >= MAX_PER_FUNCTION)
            {
                return false;
            }

            ++count;
            s_total_matches.fetch_add(1, std::memory_order_relaxed);

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

            const auto context_full_name = context->GetFullName();
            const auto function_full_name = function->GetFullName();

            const auto context_name_narrow =
                RC::to_string(context_full_name);

            const auto function_name_narrow =
                RC::to_string(function_full_name);

            if (!matches_filter(
                    context_name_narrow,
                    function_name_narrow))
            {
                return;
            }

            if (!should_log_function(function_name_narrow))
            {
                return;
            }

            Output::send(
                STR("[NullPrismTracer] Context: {}\n"),
                context_full_name
            );

            Output::send(
                STR("[NullPrismTracer] Function: {}\n"),
                function_full_name
            );
        }

    public:
        RuntimeTracer()
        {
            ModVersion = STR("0.1.0");
            ModName = STR("NullPrism Runtime Tracer");
            ModAuthors = STR("NullPrism");
            ModDescription =
                STR("Filtered UObject::ProcessEvent runtime tracer");

            Output::send<LogLevel::Warning>(
                STR("[NullPrismTracer] DLL constructed.\n")
            );
        }

        ~RuntimeTracer() override
        {
            /*
             * This diagnostic build is intended to remain loaded until
             * Palworld exits. Do not hot-unload it during testing.
             */
            Output::send<LogLevel::Warning>(
                STR("[NullPrismTracer] DLL destructed.\n")
            );
        }

        auto on_unreal_init() -> void override
        {
            Output::send<LogLevel::Warning>(
                STR("[NullPrismTracer] Unreal initialized.\n")
            );

            if (!UE4SSRuntime::IsProcessEventAvailable())
            {
                Output::send<LogLevel::Error>(
                    STR(
                        "[NullPrismTracer] ProcessEvent is unavailable; "
                        "the UE4SS AOB scan may have failed.\n"
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
                    STR("RuntimeTracer")
                }
            );

            Output::send<LogLevel::Warning>(
                STR(
                    "[NullPrismTracer] ProcessEvent callback registered. "
                    "Tracing is active.\n"
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

    MOD_EXPORT void uninstall_mod(RC::CppUserModBase* mod)
    {
        delete mod;
    }
}
