# NullPrism Runtime Tracer

A UE4SS C++ runtime tracing tool for Palworld.

# v0.1

https://github.com/NullPrism/Palworld/tree/main/NullPrismRuntimeTracer/v0.1_fulltrace

This version traces all calls. Be warned, it creates a lot of noise.

# v0.2

https://github.com/NullPrism/Palworld/tree/main/NullPrismRuntimeTracer/v0.2_twosecond

This version traces for two seconds after OnStartCoopRequest is triggered and traces every Pal-related ProcessEvent. It also suppresses high-frequency noise.

# v0.3

https://github.com/NullPrism/Palworld/tree/main/NullPrismRuntimeTracer/v0.3_targetedfunctions

This version traces only these functions and records their parameter buffers both before and after execution:

```
BP_PalPlayerController:OnStartCoopRequest
BP_OtomoPalHolderComponent:GetSpawnedOtomoID
WBP_PlayerUI:OnRequestCoop
PalRiderComponent:SetRideMarker_ToALL
PalRideMarkerComponent:SetRidingFlag
```

SHA256SUM: `e7a86c2aa13bb6662f9192bc1613d3a72fa36df88123ebda83b673cff622db92`

# v0.4

https://github.com/NullPrism/Palworld/tree/main/NullPrismRuntimeTracer/v0.4_onlyrideevents

Capture only ride/co-op-related events for 1.5 seconds after `OnStartCoopRequest`.

SHA256: `e44aa16db419e85d835dbaa2ce63a2ba714f6d47d49eaff65361a31da0bd42bc`
