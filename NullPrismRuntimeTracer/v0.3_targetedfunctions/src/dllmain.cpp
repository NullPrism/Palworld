#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>

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
        static inline std::atomic_uint64_t s_call_number{0};
        static inline std::mutex s_log_mutex{};

        static constexpr std::size_t MAX_PARAMETER_BYTES = 256;

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

        static auto is_target_function(
            const std::string& function_name
        ) -> bool
        {
            const auto name = to_lower_ascii(function_name);

            static constexpr std::string_view targets[] = {
                ":onstartcooprequest",
                ":getspawnedotomoid",
                ":onrequestcoop",
                ":setridemarker_toall",
                ":setridingflag",
                ":onendcooprequest",
                ":onride",
                ":onplaycoopfail",
                ":onplayskillfail"
            };

            for (const auto target : targets)
            {
                if (name.find(target) != std::string::npos)
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

                output << std::setw(2)
                       << static_cast<unsigned int>(bytes[offset]);
            }

            if (size > bytes_to_read)
            {
                output << " ...";
            }

            return output.str();
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

            const auto function_full_name =
                function->GetFullName();

            const auto function_name =
                RC::to_string(function_full_name);

            if (!is_target_function(function_name))
            {
                return;
            }

            const auto context_full_name =
                context->GetFullName();

            const auto call_number =
                s_call_number.fetch_add(
                    1,
                    std::memory_order_relaxed
                ) + 1;

            const auto parameter_size =
                static_cast<std::size_t>(
                    function->GetParmsSize()
                );

            const auto parameter_hex =
                bytes_to_hex(parameters, parameter_size);

            std::scoped_lock lock{s_log_mutex};

            Output::send<LogLevel::Warning>(
                STR(
                    "[NullPrismTracerV3] ===== CALL {} {} =====\n"
                ),
                call_number,
                RC::ensure_str(std::string{phase})
            );

            Output::send(
                STR("[NullPrismTracerV3] Context: {}\n"),
                context_full_name
            );

            Output::send(
                STR("[NullPrismTracerV3] Function: {}\n"),
                function_full_name
            );

            Output::send(
                STR(
                    "[NullPrismTracerV3] Params pointer: {}\n"
                ),
                parameters
            );

            Output::send(
                STR(
                    "[NullPrismTracerV3] Params size: {}\n"
                ),
                parameter_size
            );

            Output::send(
                STR(
                    "[NullPrismTracerV3] Params hex: {}\n"
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
            ModVersion = STR("0.3.0");
            ModName = STR("NullPrism Runtime Tracer");
            ModAuthors = STR("NullPrism");
            ModDescription = STR(
                "Targeted Palworld ProcessEvent parameter tracer"
            );

            Output::send<LogLevel::Warning>(
                STR(
                    "[NullPrismTracerV3] DLL constructed. "
                    "Version 0.3.0.\n"
                )
            );
        }

        ~RuntimeTracer() override
        {
            Output::send<LogLevel::Warning>(
                STR("[NullPrismTracerV3] DLL destructed.\n")
            );
        }

        auto on_unreal_init() -> void override
        {
            Output::send<LogLevel::Warning>(
                STR("[NullPrismTracerV3] Unreal initialized.\n")
            );

            if (!UE4SSRuntime::IsProcessEventAvailable())
            {
                Output::send<LogLevel::Error>(
                    STR(
                        "[NullPrismTracerV3] ProcessEvent is "
                        "unavailable; UE4SS's AOB scan may have "
                        "failed.\n"
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
                    STR("RuntimeTracerV3Pre")
                }
            );

            Hook::RegisterProcessEventPostCallback(
                process_event_post_callback,
                {
                    false,
                    false,
                    STR("NullPrism"),
                    STR("RuntimeTracerV3Post")
                }
            );

            Output::send<LogLevel::Warning>(
                STR(
                    "[NullPrismTracerV3] PRE and POST callbacks "
                    "registered. Targeted tracing is active.\n"
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
