using System.Text.Json.Serialization;

namespace Hydra.Studio.App.Studio;

public sealed class StudioConfig
{
    [JsonPropertyName("hydra_root")]
    public string? HydraRoot { get; set; }
}