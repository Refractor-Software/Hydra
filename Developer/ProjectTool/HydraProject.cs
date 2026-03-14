using System.Text.Json.Serialization;

namespace Hydra.ProjectTool;

/// <summary>
/// Root descriptor for a Hydra game project. Persisted as &lt;ProjectName&gt;.hyproject.
/// </summary>
public sealed class HydraProject
{
    [JsonPropertyName("name")]
    public string Name { get; set; } = "";

    [JsonPropertyName("version")]
    public string Version { get; set; } = "0.1.0";

    [JsonPropertyName("hydra_version")]
    public string HydraVersion { get; set; } = "";

    /// <summary>
    /// Runtime modules to link (beyond the default set).
    /// Default set is always included; this is for opt-in extras.
    /// </summary>
    [JsonPropertyName("modules")]
    public List<string> Modules { get; set; } = [];

    /// <summary>
    /// Hydra packages (plugins) to include.
    /// </summary>
    [JsonPropertyName("packages")]
    public List<string> Packages { get; set; } = [];

    /// <summary>
    /// Target platforms. Determines which presets are generated.
    /// </summary>
    [JsonPropertyName("platforms")]
    public List<string> Platforms { get; set; } = ["Windows"];
}
