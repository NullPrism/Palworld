#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <DynamicOutput/Output.hpp>
#include <Helpers/String.hpp>
#include <Mod/CppUserModBase.hpp>
#include <UE4SSRuntime.hpp>

#include <Unreal/Hooks.hpp>
#include <Unreal/UObject.hpp>
#include <Unreal/UObjectGlobals.hpp>
#include <Unreal/CoreUObject/UObject/Class.hpp>

namespace NullPrism
{
    using namespace RC;
    using namespace RC::Unreal;

    class AstersNoSaddle final : public CppUserModBase
    {
    private:
        /*
         * UPalPartnerSkillParameterComponent::RestrictionItems
         *
         * TArray layout:
         *   +0x00 Data
         *   +0x08 Num
         *   +0x0C Max
         */
        static constexpr std::size_t RESTRICTION_ITEMS_OFFSET = 0x140;
        static constexpr std::size_t TARRAY_NUM_OFFSET = 0x08;
        static constexpr std::size_t TARRAY_MAX_OFFSET = 0x0C;

        /*
         * UPalRideMarkerComponent::RidePositionType
         */
        static constexpr std::size_t RIDE_POSITION_TYPE_OFFSET = 0x5F0;

        enum class RidePosition : std::uint8_t
        {
            None            = 0,
            HorseRide       = 1,
            BiggerHorseRide = 2,
            SitRide         = 3,
            BackRide        = 4
        };

        struct UtilityObjectReturnParams
        {
            UObject* WorldContextObject{};
            UObject* ReturnValue{};
        };

        /*
         * Reflected UPalRiderComponent::Ride layout:
         *
         * +0x00 UPalRideMarkerComponent* Marker
         * +0x08 bool bIsSkipAnimation
         * +0x09 bool ReturnValue
         *
         * Do not represent this with a normal C++ struct: native alignment
         * would expand it to 16 bytes, while Unreal expects exactly 10.
         */
        static constexpr std::size_t RIDE_PARAMETER_SIZE = 10;
        static constexpr std::size_t RIDE_MARKER_OFFSET = 0x00;
        static constexpr std::size_t RIDE_SKIP_ANIMATION_OFFSET = 0x08;
        static constexpr std::size_t RIDE_RETURN_VALUE_OFFSET = 0x09;

        struct PendingMarkerRestore
        {
            std::uint8_t OriginalRidePosition{};
        };

        static inline thread_local bool s_internal_call{false};

        /*
         * A marker remains here only while a successful mount is waiting
         * for SetRidingFlag(true). It is restored as soon as the normal
         * authoritative riding sequence reaches that point.
         */
        static inline std::unordered_map<
            UObject*,
            PendingMarkerRestore
        > s_pending_marker_restores{};

        static auto contains(
            const std::string& value,
            std::string_view term
        ) -> bool
        {
            return value.find(term) != std::string::npos;
        }

        static auto function_name(
            UFunction* function
        ) -> std::string
        {
            if (function == nullptr)
            {
                return {};
            }

            return RC::to_string(function->GetFullName());
        }

        static auto function_matches(
            UFunction* function,
            std::string_view term
        ) -> bool
        {
            return contains(function_name(function), term);
        }

        static auto object_path(
            UObject* object
        ) -> std::string
        {
            if (object == nullptr)
            {
                return {};
            }

            auto value = RC::to_string(object->GetFullName());

            const auto first_space = value.find(' ');

            if (first_space != std::string::npos)
            {
                value.erase(0, first_space + 1);
            }

            return value;
        }

        static auto owner_actor_path_from_component(
            UObject* component
        ) -> std::string
        {
            auto value = object_path(component);

            const auto final_dot = value.rfind('.');

            if (final_dot != std::string::npos)
            {
                value.erase(final_dot);
            }

            return value;
        }

        static auto get_pal_utility() -> UObject*
        {
            static UObject* utility{};

            if (utility == nullptr)
            {
                utility =
                    UObjectGlobals::StaticFindObject<UObject*>(
                        nullptr,
                        nullptr,
                        STR("/Script/Pal.Default__PalUtility")
                    );
            }

            return utility;
        }

        static auto call_pal_utility_object_function(
            UObject* world_context,
            const CharType* requested_function
        ) -> UObject*
        {
            auto* utility = get_pal_utility();

            if (utility == nullptr)
            {
                Output::send<LogLevel::Error>(
                    STR(
                        "[AstersNoSaddle] Default__PalUtility "
                        "was not found.\n"
                    )
                );

                return nullptr;
            }

            auto* function =
                utility->GetFunctionByName(requested_function);

            if (function == nullptr)
            {
                Output::send<LogLevel::Error>(
                    STR(
                        "[AstersNoSaddle] PalUtility function "
                        "was not found: {}\n"
                    ),
                    requested_function
                );

                return nullptr;
            }

            UtilityObjectReturnParams params{};
            params.WorldContextObject = world_context;

            s_internal_call = true;
            utility->ProcessEvent(function, &params);
            s_internal_call = false;

            return params.ReturnValue;
        }

        static auto get_active_partner_component(
            UObject* world_context
        ) -> UObject*
        {
            return call_pal_utility_object_function(
                world_context,
                STR("GetSpawnedOtomoPalPartnerSkill")
            );
        }

        static auto get_local_player_character(
            UObject* world_context
        ) -> UObject*
        {
            return call_pal_utility_object_function(
                world_context,
                STR("GetPlayerCharacter")
            );
        }

        static auto clear_restriction_items(
            UObject* partner_component,
            bool log_change
        ) -> bool
        {
            if (partner_component == nullptr)
            {
                return false;
            }

            auto* bytes =
                reinterpret_cast<std::uint8_t*>(
                    partner_component
                );

            auto* array =
                bytes + RESTRICTION_ITEMS_OFFSET;

            auto* num =
                reinterpret_cast<std::int32_t*>(
                    array + TARRAY_NUM_OFFSET
                );

            auto* max =
                reinterpret_cast<std::int32_t*>(
                    array + TARRAY_MAX_OFFSET
                );

            /*
             * Refuse to modify memory if the presumed TArray layout does
             * not look internally consistent.
             */
            if (*num < 0 ||
                *max < 0 ||
                *num > *max ||
                *max > 1024)
            {
                Output::send<LogLevel::Error>(
                    STR(
                        "[AstersNoSaddle] Suspicious RestrictionItems "
                        "layout on {}; refusing modification. "
                        "Num={} Max={}\n"
                    ),
                    partner_component->GetFullName(),
                    *num,
                    *max
                );

                return false;
            }

            if (*num == 0)
            {
                return true;
            }

            const auto previous_num = *num;
            *num = 0;

            if (log_change)
            {
                Output::send<LogLevel::Warning>(
                    STR(
                        "[AstersNoSaddle] Cleared {} saddle restriction "
                        "item(s) from {}.\n"
                    ),
                    previous_num,
                    partner_component->GetFullName()
                );
            }

            return true;
        }

        static auto find_active_pal_marker(
            UObject* partner_component
        ) -> UObject*
        {
            if (partner_component == nullptr)
            {
                return nullptr;
            }

            const auto pal_actor_path =
                owner_actor_path_from_component(
                    partner_component
                );

            if (pal_actor_path.empty())
            {
                return nullptr;
            }

            std::vector<UObject*> markers{};

            UObjectGlobals::FindAllOf(
                std::string_view{"PalRideMarkerComponent"},
                markers
            );

            UObject* match{};

            for (auto* marker : markers)
            {
                if (marker == nullptr)
                {
                    continue;
                }

                const auto marker_name =
                    RC::to_string(marker->GetFullName());

                if (!contains(marker_name, pal_actor_path))
                {
                    continue;
                }

                if (match != nullptr)
                {
                    Output::send<LogLevel::Error>(
                        STR(
                            "[AstersNoSaddle] Multiple ride markers "
                            "matched active Pal {}; refusing guess.\n"
                        ),
                        RC::ensure_str(pal_actor_path)
                    );

                    return nullptr;
                }

                match = marker;
            }

            return match;
        }

        static auto find_local_rider_component(
            UObject* player_character
        ) -> UObject*
        {
            if (player_character == nullptr)
            {
                return nullptr;
            }

            const auto player_path =
                object_path(player_character);

            if (player_path.empty())
            {
                return nullptr;
            }

            std::vector<UObject*> rider_components{};

            UObjectGlobals::FindAllOf(
                std::string_view{"PalRiderComponent"},
                rider_components
            );

            UObject* match{};

            for (auto* rider : rider_components)
            {
                if (rider == nullptr)
                {
                    continue;
                }

                const auto rider_name =
                    RC::to_string(rider->GetFullName());

                if (!contains(rider_name, player_path))
                {
                    continue;
                }

                if (match != nullptr)
                {
                    Output::send<LogLevel::Error>(
                        STR(
                            "[AstersNoSaddle] Multiple RiderComponents "
                            "matched local player {}; refusing guess.\n"
                        ),
                        RC::ensure_str(player_path)
                    );

                    return nullptr;
                }

                match = rider;
            }

            return match;
        }

        static auto call_zero_parameter_bool(
            UObject* object,
            const CharType* requested_function,
            bool& available
        ) -> bool
        {
            available = false;

            if (object == nullptr)
            {
                return false;
            }

            auto* function =
                object->GetFunctionByName(requested_function);

            if (function == nullptr ||
                function->GetParmsSize() != 1)
            {
                return false;
            }

            std::uint8_t result{};

            s_internal_call = true;
            object->ProcessEvent(function, &result);
            s_internal_call = false;

            available = true;

            return result != 0;
        }

        static auto ride_with_position(
            UObject* rider,
            UObject* marker,
            RidePosition requested_position
        ) -> bool
        {
            if (rider == nullptr || marker == nullptr)
            {
                return false;
            }

            auto* ride_function =
                rider->GetFunctionByName(STR("Ride"));

            if (ride_function == nullptr)
            {
                Output::send<LogLevel::Error>(
                    STR(
                        "[AstersNoSaddle] "
                        "UPalRiderComponent::Ride was not found.\n"
                    )
                );

                return false;
            }

            const auto parameter_size =
                static_cast<std::size_t>(
                    ride_function->GetParmsSize()
                );

            if (parameter_size != RIDE_PARAMETER_SIZE)
            {
                Output::send<LogLevel::Error>(
                    STR(
                        "[AstersNoSaddle] Unexpected Ride parameter "
                        "size {}. Expected {}.\n"
                    ),
                    parameter_size,
                    RIDE_PARAMETER_SIZE
                );

                return false;
            }

            auto* marker_bytes =
                reinterpret_cast<std::uint8_t*>(marker);

            const auto original_position =
                marker_bytes[RIDE_POSITION_TYPE_OFFSET];

            marker_bytes[RIDE_POSITION_TYPE_OFFSET] =
                static_cast<std::uint8_t>(
                    requested_position
                );

            std::array<std::uint8_t, RIDE_PARAMETER_SIZE> params{};

            std::memcpy(
                params.data() + RIDE_MARKER_OFFSET,
                &marker,
                sizeof(marker)
            );

            params[RIDE_SKIP_ANIMATION_OFFSET] = 0;
            params[RIDE_RETURN_VALUE_OFFSET] = 0;

            s_internal_call = true;
            rider->ProcessEvent(
                ride_function,
                params.data()
            );
            s_internal_call = false;

            const bool ride_succeeded =
                params[RIDE_RETURN_VALUE_OFFSET] != 0;

            if (!ride_succeeded)
            {
                /*
                 * Nothing started, so restoration is safe immediately.
                 */
                marker_bytes[RIDE_POSITION_TYPE_OFFSET] =
                    original_position;

                return false;
            }

            /*
             * Do not restore yet. Ride() reads the marker type while it
             * initializes its action and networking state. Restoration is
             * deferred until SetRidingFlag(true) confirms that the normal
             * authoritative ride sequence has started.
             */
            s_pending_marker_restores.insert_or_assign(
                marker,
                PendingMarkerRestore{
                    .OriginalRidePosition = original_position
                }
            );

            return true;
        }

        static auto begin_mount(
            UObject* controller
        ) -> bool
        {
            auto* partner =
                get_active_partner_component(controller);

            if (partner == nullptr)
            {
                return false;
            }

            if (!clear_restriction_items(partner, false))
            {
                return false;
            }

            auto* marker =
                find_active_pal_marker(partner);

            if (marker == nullptr)
            {
                Output::send<LogLevel::Warning>(
                    STR(
                        "[AstersNoSaddle] Active Pal has no unambiguous "
                        "ride marker; normal game handling will continue.\n"
                    )
                );

                return false;
            }

            auto* player =
                get_local_player_character(controller);

            auto* rider =
                find_local_rider_component(player);

            if (rider == nullptr)
            {
                Output::send<LogLevel::Error>(
                    STR(
                        "[AstersNoSaddle] Local RiderComponent "
                        "could not be resolved.\n"
                    )
                );

                return false;
            }

            bool is_riding_available{};
            const bool is_riding =
                call_zero_parameter_bool(
                    rider,
                    STR("IsRiding"),
                    is_riding_available
                );

            if (is_riding_available && is_riding)
            {
                /*
                 * Do not interfere with the normal dismount input path.
                 */
                return false;
            }

            const auto* marker_bytes =
                reinterpret_cast<const std::uint8_t*>(
                    marker
                );

            const auto original_position =
                static_cast<RidePosition>(
                    marker_bytes[RIDE_POSITION_TYPE_OFFSET]
                );

            /*
             * Preserve markers that already have a normal ride action.
             *
             * SitRide is the confirmed Neptilius case. None is also
             * allowed to try the safe HorseRide fallback. BackRide and
             * BiggerHorseRide are tried natively first.
             */
            std::array<RidePosition, 5> attempts{};
            std::size_t attempt_count{};

            const auto add_attempt =
                [&attempts, &attempt_count](
                    RidePosition position
                )
                {
                    for (std::size_t index = 0;
                         index < attempt_count;
                         ++index)
                    {
                        if (attempts[index] == position)
                        {
                            return;
                        }
                    }

                    attempts[attempt_count++] = position;
                };

            switch (original_position)
            {
                case RidePosition::HorseRide:
                case RidePosition::BiggerHorseRide:
                case RidePosition::BackRide:
                    add_attempt(original_position);
                    break;

                case RidePosition::SitRide:
                case RidePosition::None:
                    break;

                default:
                    Output::send<LogLevel::Warning>(
                        STR(
                            "[AstersNoSaddle] Unknown RidePositionType {} "
                            "on {}; refusing modification.\n"
                        ),
                        static_cast<unsigned int>(
                            marker_bytes[
                                RIDE_POSITION_TYPE_OFFSET
                            ]
                        ),
                        marker->GetFullName()
                    );

                    return false;
            }

            /*
             * Safe fallbacks. HorseRide is confirmed to work for
             * Neptilius. BiggerHorseRide and BackRide are retained as
             * secondary choices for differently shaped Pals.
             */
            add_attempt(RidePosition::HorseRide);
            add_attempt(RidePosition::BiggerHorseRide);
            add_attempt(RidePosition::BackRide);

            for (std::size_t index = 0;
                 index < attempt_count;
                 ++index)
            {
                const auto position = attempts[index];

                if (ride_with_position(
                        rider,
                        marker,
                        position
                    ))
                {
                    return true;
                }
            }

            Output::send<LogLevel::Warning>(
                STR(
                    "[AstersNoSaddle] Ride() rejected every supported "
                    "ride position for {}.\n"
                ),
                marker->GetFullName()
            );

            return false;
        }

        static auto restore_marker_position(
            UObject* marker
        ) -> void
        {
            if (marker == nullptr)
            {
                return;
            }

            const auto pending =
                s_pending_marker_restores.find(marker);

            if (pending ==
                s_pending_marker_restores.end())
            {
                return;
            }

            auto* marker_bytes =
                reinterpret_cast<std::uint8_t*>(marker);

            marker_bytes[RIDE_POSITION_TYPE_OFFSET] =
                pending->second.OriginalRidePosition;

            s_pending_marker_restores.erase(pending);
        }

        static auto is_partner_activation_function(
            UFunction* function
        ) -> bool
        {
            const auto name = function_name(function);

            if (!contains(
                    name,
                    "PalPartnerSkillParameterComponent:"
                ))
            {
                return false;
            }

            return
                contains(name, ":OnInitializedCharacter") ||
                contains(name, ":OnActivatedAsPartner") ||
                contains(name, ":OnActivatedAsOtomoHolder") ||
                contains(name, ":OnOwnerCharacterSpawned");
        }

        static auto process_event_pre(
            Hook::TCallbackIterationData<void>&,
            UObject* context,
            UFunction* function,
            void* parameters
        ) -> void
        {
            if (s_internal_call ||
                context == nullptr ||
                function == nullptr)
            {
                return;
            }

            /*
             * Remove the saddle restriction immediately before the
             * game's native restriction query evaluates it.
             *
             * Only modify a partner-skill component whose owning Pal
             * has an unambiguous ride marker. The original query then
             * continues normally and reports the unrestricted state to
             * the interaction UI.
             */
            if (function_matches(
                    function,
                    "PalPartnerSkillParameterComponent:IsRestrictedByItems"
                ))
            {
                auto* marker =
                    find_active_pal_marker(context);

                if (marker != nullptr)
                {
                    clear_restriction_items(
                        context,
                        false
                    );
                }

                return;
            }

            /*
             * Preserve the working first-interaction fallback. If the
             * earlier restriction-query path is missed, pressing F can
             * still enter the normal mounting pipeline.
             */
            if (function_matches(
                    function,
                    ":OnStartCoopRequest"
                ))
            {
                begin_mount(context);
                return;
            }

            /*
             * Restore the original marker position only after the
             * normal ride pipeline confirms riding has started.
             */
            if (function_matches(
                    function,
                    "PalRideMarkerComponent:SetRidingFlag"
                ) &&
                parameters != nullptr)
            {
                const auto is_enable =
                    *static_cast<const std::uint8_t*>(
                        parameters
                    ) != 0;

                if (is_enable)
                {
                    restore_marker_position(context);
                }
            }
        }



        static auto process_event_post(
            Hook::TCallbackIterationData<void>&,
            UObject* context,
            UFunction* function,
            void*
        ) -> void
        {
            if (s_internal_call ||
                context == nullptr ||
                function == nullptr)
            {
                return;
            }

            /*
             * Retain the original activation-time optimization without
             * probe logging. On builds where PalUtility already exposes
             * the active partner here, clear it immediately.
             */
            if (is_partner_activation_function(function))
            {
                auto* active_partner =
                    get_active_partner_component(context);

                if (active_partner == context)
                {
                    clear_restriction_items(
                        context,
                        false
                    );
                }
            }
        }



    public:
        AstersNoSaddle()
        {
            ModVersion = STR("1.0.5");
            ModName = STR("Aster's No Saddle");
            ModAuthors = STR("Aster");
            ModDescription = STR(
                "Allows compatible Pals to use their native ride "
                "pipeline without requiring a saddle item"
            );

            Output::send<LogLevel::Warning>(
                STR(
                    "[AstersNoSaddle] DLL constructed. "
                    "Version 1.0.5.\n"
                )
            );
        }

        ~AstersNoSaddle() override
        {
            /*
             * Best-effort restoration if the mod is unloaded while a
             * marker is awaiting authoritative confirmation.
             */
            for (const auto& [marker, restore] :
                 s_pending_marker_restores)
            {
                if (marker == nullptr)
                {
                    continue;
                }

                auto* marker_bytes =
                    reinterpret_cast<std::uint8_t*>(marker);

                marker_bytes[RIDE_POSITION_TYPE_OFFSET] =
                    restore.OriginalRidePosition;
            }

            s_pending_marker_restores.clear();

            Output::send<LogLevel::Warning>(
                STR("[AstersNoSaddle] DLL destructed.\n")
            );
        }

        auto on_unreal_init() -> void override
        {
            Output::send<LogLevel::Warning>(
                STR("[AstersNoSaddle] Unreal initialized.\n")
            );

            if (!UE4SSRuntime::IsProcessEventAvailable())
            {
                Output::send<LogLevel::Error>(
                    STR(
                        "[AstersNoSaddle] ProcessEvent is unavailable; "
                        "the mod cannot start.\n"
                    )
                );

                return;
            }

            Hook::RegisterProcessEventPreCallback(
                process_event_pre,
                {
                    false,
                    false,
                    STR("NullPrism"),
                    STR("NoSaddleMountPre")
                }
            );

            Hook::RegisterProcessEventPostCallback(
                process_event_post,
                {
                    false,
                    false,
                    STR("NullPrism"),
                    STR("NoSaddleMountPost")
                }
            );

            Output::send<LogLevel::Warning>(
                STR(
                    "[AstersNoSaddle] Ready. Partner restrictions will be removed when evaluated.\n"
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
        return new NullPrism::AstersNoSaddle();
    }

    MOD_EXPORT void uninstall_mod(
        RC::CppUserModBase* mod
    )
    {
        delete mod;
    }
}
