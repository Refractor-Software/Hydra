using System.Text.Json;
using System.Text.Json.Nodes;
using Hydra.Tools.ProjectTool.Project;

namespace Hydra.Tools.ProjectTool.CMake;

/// <summary>
/// Generates CMakePresets.json and CMakeUserPresets.json for a Hydra game project.
/// Mirrors the engine's platform/config matrix so presets stay in sync.
/// </summary>
public static class CMakePresetsGenerator
{
    private static readonly JsonSerializerOptions JsonOptions = new()
    {
        WriteIndented = true
    };

    // Configs mirror the engine's CMAKE_CONFIGURATION_TYPES
    private static readonly string[] Configs = ["Debug", "RelWithDebInfo", "Release"];

    public static void Generate(string projectDir, HydraProject project)
    {
        GeneratePresets(projectDir, project);
        GenerateUserPresets(projectDir, project);
    }

    public static void GenerateUserPresets(string projectDir, HydraProject project)
    {
        // CMakeUserPresets.json is gitignored and per-developer.
        // Only generate if it doesn't already exist — never overwrite,
        // since developers may have customised it.
        var userPresetsPath = Path.Combine(projectDir, "CMakeUserPresets.json");
        if (File.Exists(userPresetsPath))
            return;

        var configurePresetsArray = new JsonArray();
        foreach (var node in BuildUserConfigurePresets([.. project.Platforms]))
            configurePresetsArray.Add(node);

        var userPresets = new JsonObject
        {
            ["version"] = 3,
            ["configurePresets"] = configurePresetsArray
        };

        File.WriteAllText(userPresetsPath, userPresets.ToJsonString(JsonOptions));
        Console.WriteLine($"  Created CMakeUserPresets.json (set Hydra_ROOT inside to point at your engine install)");
    }

    private static JsonNode[] BuildUserConfigurePresets(string[] projectPlatforms)
    {
        var presets = new List<JsonNode>();
        foreach (var platform in projectPlatforms)
        {
            var baseName = $"{platform}-VisualStudio";
            presets.Add(new JsonObject
            {
                ["name"] = $"{baseName}-User",
                ["inherits"] = baseName,
                ["cacheVariables"] = new JsonObject
                {
                    // Developer sets this to their local Hydra install path.
                    // e.g. "C:/HydraSDK"
                    ["Hydra_ROOT"] = "FILL_ME_IN"
                }
            });
        }
        return [.. presets];
    }

    public static void GeneratePresets(string projectDir, HydraProject project)
    {
        var configurePresets = new JsonArray();
        var buildPresets = new JsonArray();

        foreach (var platform in project.Platforms)
        {
            // --- Visual Studio (multi-config) ---
            var vsPresetName = $"{platform}-VisualStudio";
            configurePresets.Add(BuildVSConfigurePreset(platform, vsPresetName, project));

            foreach (var config in Configs)
            {
                buildPresets.Add(new JsonObject
                {
                    ["name"] = $"{vsPresetName}-{config}",
                    ["configurePreset"] = vsPresetName,
                    ["configuration"] = config
                });
            }

            // --- Ninja (single-config, one preset per config) ---
            var ninjaBase = BuildNinjaBasePreset(platform);
            configurePresets.Add(ninjaBase);

            foreach (var config in Configs)
            {
                configurePresets.Add(BuildNinjaConfigPreset(platform, config, project));
                buildPresets.Add(new JsonObject
                {
                    ["name"] = $"{platform}-Ninja-{config}",
                    ["configurePreset"] = $"{platform}-Ninja-{config}"
                });
            }
        }

        var presets = new JsonObject
        {
            ["version"] = 3,
            ["cmakeMinimumRequired"] = new JsonObject
            {
                ["major"] = 4, ["minor"] = 1, ["patch"] = 0
            },
            ["configurePresets"] = configurePresets,
            ["buildPresets"] = buildPresets
        };

        var path = Path.Combine(projectDir, "CMakePresets.json");
        File.WriteAllText(path, presets.ToJsonString(JsonOptions));
        Console.WriteLine($"  Generated CMakePresets.json ({project.Platforms.Count} platform(s), {Configs.Length} config(s) each)");
    }

    private static JsonObject BuildVSConfigurePreset(string platform, string name, HydraProject project)
    {
        return new JsonObject
        {
            ["name"] = name,
            ["displayName"] = $"{platform} (Visual Studio)",
            ["generator"] = PlatformGenerator(platform),
            ["architecture"] = new JsonObject
            {
                ["value"] = PlatformArchitecture(platform),
                ["strategy"] = "set"
            },
            ["binaryDir"] = $"${{sourceDir}}/Intermediate/{name}",
            ["cacheVariables"] = new JsonObject
            {
                ["CMAKE_CXX_STANDARD"] = "20",
                ["CMAKE_CONFIGURATION_TYPES"] = string.Join(";", Configs),
                ["CMAKE_RUNTIME_OUTPUT_DIRECTORY"] = $"${{sourceDir}}/Binaries/{platform}",
                ["CMAKE_ARCHIVE_OUTPUT_DIRECTORY"] = $"${{sourceDir}}/Binaries/{platform}",
                ["CMAKE_LIBRARY_OUTPUT_DIRECTORY"] = $"${{sourceDir}}/Binaries/{platform}"
            },
            ["condition"] = new JsonObject
            {
                ["type"] = "equals",
                ["lhs"] = "${hostSystemName}",
                ["rhs"] = HostSystemName(platform)
            }
        };
    }

    private static JsonObject BuildNinjaBasePreset(string platform)
    {
        return new JsonObject
        {
            ["name"] = $"{platform}-Ninja",
            ["hidden"] = true,
            ["generator"] = "Ninja",
            ["architecture"] = new JsonObject
            {
                ["value"] = PlatformArchitecture(platform),
                ["strategy"] = "external"
            },
            ["cacheVariables"] = NinjaCompilerVars(platform),
            ["condition"] = new JsonObject
            {
                ["type"] = "equals",
                ["lhs"] = "${hostSystemName}",
                ["rhs"] = HostSystemName(platform)
            }
        };
    }

    private static JsonObject BuildNinjaConfigPreset(string platform, string config, HydraProject project)
    {
        return new JsonObject
        {
            ["name"] = $"{platform}-Ninja-{config}",
            ["displayName"] = $"{platform} (Ninja) {config}",
            ["inherits"] = $"{platform}-Ninja",
            ["binaryDir"] = $"${{sourceDir}}/Intermediate/{platform}-Ninja/{config}",
            ["cacheVariables"] = new JsonObject
            {
                ["CMAKE_BUILD_TYPE"] = config,
                ["CMAKE_RUNTIME_OUTPUT_DIRECTORY"] = $"${{sourceDir}}/Binaries/{platform}/{config}",
                ["CMAKE_ARCHIVE_OUTPUT_DIRECTORY"] = $"${{sourceDir}}/Binaries/{platform}/{config}",
                ["CMAKE_LIBRARY_OUTPUT_DIRECTORY"] = $"${{sourceDir}}/Binaries/{platform}/{config}"
            }
        };
    }

    // --- Platform helpers ---
    // Extend these as new platforms are added to the engine.

    private static string PlatformGenerator(string platform) => platform switch
    {
        "Windows" => "Visual Studio 17 2022",
        _ => throw new NotSupportedException($"No VS generator defined for platform '{platform}'")
    };

    private static string PlatformArchitecture(string platform) => platform switch
    {
        "Windows" => "x64",
        _ => "x64"
    };

    private static string HostSystemName(string platform) => platform switch
    {
        "Windows" => "Windows",
        "Linux"   => "Linux",
        "Mac"     => "Darwin",
        _ => platform
    };

    private static JsonObject NinjaCompilerVars(string platform) => platform switch
    {
        "Windows" => new JsonObject
        {
            ["CMAKE_C_COMPILER"]   = "cl.exe",
            ["CMAKE_CXX_COMPILER"] = "cl.exe",
            ["CMAKE_CXX_STANDARD"] = "20",
            ["CMAKE_CXX_FLAGS"]    = "/std:c++20"
        },
        _ => new JsonObject
        {
            ["CMAKE_CXX_STANDARD"] = "20"
        }
    };
}
