include_external_msproject(
    Hydra.Studio.Core
    "${CMAKE_SOURCE_DIR}/Studio/Hydra.Studio.Core/Hydra.Studio.Core.csproj"
    TYPE "{FAE04EC0-301F-11D3-BF4B-00C04F79EFBC}"
)
set_target_properties(Hydra.Studio.Core PROPERTIES FOLDER "Studio")

include_external_msproject(
    Hydra.Studio.Shell
    "${CMAKE_SOURCE_DIR}/Studio/Hydra.Studio.Shell/Hydra.Studio.Shell.csproj"
    TYPE "{FAE04EC0-301F-11D3-BF4B-00C04F79EFBC}"
)
set_target_properties(Hydra.Studio.Shell PROPERTIES FOLDER "Studio")

include_external_msproject(
    Hydra.Studio.App
    "${CMAKE_SOURCE_DIR}/Studio/Hydra.Studio.App/Hydra.Studio.App.csproj"
    TYPE "{FAE04EC0-301F-11D3-BF4B-00C04F79EFBC}"
)
set_target_properties(Hydra.Studio.App PROPERTIES FOLDER "Studio")