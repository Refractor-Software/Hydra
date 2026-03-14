using System.IO;
using System.Text.Json;
using Hydra.Tools.ProjectTool.Project;

namespace Hydra.Studio.App.Studio;

public sealed class StudioEnvironment
{
    public string HydraRoot { get; }
    public HydraProject? Project { get; }

    private StudioEnvironment(string hydraRoot, HydraProject? project)
    {
        HydraRoot = hydraRoot;
        Project   = project;
    }

    public static StudioEnvironment Resolve(string[] args)
    {
        string hydraRoot = ResolveHydraRoot(args)
            ?? throw new InvalidOperationException(
                "Could not locate Hydra SDK. Set HYDRA_ROOT environment variable, " +
                "provide a hydra.config file next to the executable, " +
                "or pass --hydra-root <path> on the command line.");

        HydraProject? project = ResolveProject(args);

        return new StudioEnvironment(hydraRoot, project);
    }

    private static string? ResolveHydraRoot(string[] args)
    {
        // 1. Environment variable
        string? fromEnv = Environment.GetEnvironmentVariable("HYDRA_ROOT");
        if (!string.IsNullOrWhiteSpace(fromEnv) && Directory.Exists(fromEnv))
        {
            Console.WriteLine($"[StudioEnvironment] Hydra SDK from environment: {fromEnv}");
            return fromEnv;
        }

        // 2. Config file next to executable
        string configPath = Path.Combine(AppContext.BaseDirectory, "hydra.config");
        if (File.Exists(configPath))
        {
            var config = JsonSerializer.Deserialize<StudioConfig>(File.ReadAllText(configPath));
            if (!string.IsNullOrWhiteSpace(config?.HydraRoot) && Directory.Exists(config.HydraRoot))
            {
                Console.WriteLine($"[StudioEnvironment] Hydra SDK from config file: {config.HydraRoot}");
                return config.HydraRoot;
            }
        }

        // 3. Command-line argument
        string? fromArgs = ParseArg(args, "--hydra-root");
        if (!string.IsNullOrWhiteSpace(fromArgs) && Directory.Exists(fromArgs))
        {
            Console.WriteLine($"[StudioEnvironment] Hydra SDK from command line: {fromArgs}");
            return fromArgs;
        }

        return null;
    }

    private static HydraProject? ResolveProject(string[] args)
    {
        string? hyprojectPath = ParseArg(args, "--project");

        // Also accept a bare positional .hyproject path
        if (hyprojectPath is null)
            hyprojectPath = args.FirstOrDefault(a => a.EndsWith(".hyproject", StringComparison.OrdinalIgnoreCase));

        if (hyprojectPath is null || !File.Exists(hyprojectPath))
            return null;

        try
        {
            var project = JsonSerializer.Deserialize<HydraProject>(File.ReadAllText(hyprojectPath));
            Console.WriteLine($"[StudioEnvironment] Loaded project: {project?.Name}");
            return project;
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[StudioEnvironment] Failed to load project from '{hyprojectPath}': {ex.Message}");
            return null;
        }
    }

    private static string? ParseArg(string[] args, string flag)
    {
        for (int i = 0; i < args.Length - 1; i++)
            if (string.Equals(args[i], flag, StringComparison.OrdinalIgnoreCase))
                return args[i + 1];
        return null;
    }
}