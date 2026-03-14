using System.IO;
using System.Text.Json;
using Hydra.Tools.ProjectTool.Project;

namespace Hydra.Studio.App.Studio;

public sealed class StudioEnvironment
{
    public string HydraRoot { get; }
    public HydraProject? Project { get; }
    public string? ProjectDirectory { get; }

    private StudioEnvironment(string hydraRoot, HydraProject? project, string? projectDirectory)
    {
        HydraRoot = hydraRoot;
        Project = project;
        ProjectDirectory = projectDirectory;
    }

    public static StudioEnvironment Resolve(string[] args)
    {
        string hydraRoot = ResolveHydraRoot(args)
                           ?? throw new InvalidOperationException(
                               "Could not locate Hydra SDK. Set HYDRA_ROOT environment variable, " +
                               "provide a hydra.config file next to the executable, " +
                               "or pass --hydra-root <path> on the command line.");

        (HydraProject? project, string? projectDirectory) = ResolveProject(args);

        return new StudioEnvironment(hydraRoot, project, projectDirectory);
    }

    private static string? ResolveHydraRoot(string[] args)
    {
        // 1. Environment variable
        string? fromEnv = Environment.GetEnvironmentVariable("HYDRA_ROOT");
        if (IsValidHydraRoot(fromEnv))
        {
            Console.WriteLine($"[StudioEnvironment] Hydra SDK from environment: {fromEnv}");
            return fromEnv;
        }

        // 2. Config file next to executable
        string configPath = Path.Combine(AppContext.BaseDirectory, "hydra.config");
        if (File.Exists(configPath))
        {
            var config = JsonSerializer.Deserialize<StudioConfig>(File.ReadAllText(configPath));
            if (IsValidHydraRoot(config?.HydraRoot))
            {
                Console.WriteLine($"[StudioEnvironment] Hydra SDK from config file: {config!.HydraRoot}");
                return config.HydraRoot;
            }
        }

        // 3. Command-line argument
        string? fromArgs = ParseArg(args, "--hydra-root");
        if (IsValidHydraRoot(fromArgs))
        {
            Console.WriteLine($"[StudioEnvironment] Hydra SDK from command line: {fromArgs}");
            return fromArgs;
        }

        // 4. Development repo fallback — walk up from executable looking for a Hydra repo root
        string? devRoot = FindDevRepoRoot(AppContext.BaseDirectory);
        if (devRoot is not null)
        {
            Console.WriteLine($"[StudioEnvironment] Hydra dev repo detected: {devRoot}");
            return devRoot;
        }

        return null;
    }

    private static (HydraProject? Project, string? ProjectDirectory) ResolveProject(string[] args)
    {
        string? hyprojectPath = ParseArg(args, "--project");

        // Also accept a bare positional .hyproject path
        if (hyprojectPath is null)
            hyprojectPath = args.FirstOrDefault(a => a.EndsWith(".hyproject", StringComparison.OrdinalIgnoreCase));

        if (hyprojectPath is null || !File.Exists(hyprojectPath))
            return (null, null);

        string fullPath = Path.GetFullPath(hyprojectPath);
        string projectDirectory = Path.GetDirectoryName(fullPath)!;

        try
        {
            var project = JsonSerializer.Deserialize<HydraProject>(File.ReadAllText(fullPath));
            Console.WriteLine($"[StudioEnvironment] Loaded project: {project?.Name}");
            return (project, projectDirectory);
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[StudioEnvironment] Failed to load project from '{fullPath}': {ex.Message}");
            return (null, null);
        }
    }

    private static string? ParseArg(string[] args, string flag)
    {
        for (int i = 0; i < args.Length - 1; i++)
            if (string.Equals(args[i], flag, StringComparison.OrdinalIgnoreCase))
                return args[i + 1];
        return null;
    }

    private static bool IsValidHydraRoot(string? path) =>
        !string.IsNullOrWhiteSpace(path) && Directory.Exists(path);

    private static string? FindDevRepoRoot(string startDir)
    {
        var dir = new DirectoryInfo(startDir);
        while (dir is not null)
        {
            if (File.Exists(Path.Combine(dir.FullName, "CMakeLists.txt")) &&
                Directory.Exists(Path.Combine(dir.FullName, "Packages")) &&
                Directory.Exists(Path.Combine(dir.FullName, "Runtime")))
            {
                return dir.FullName;
            }

            dir = dir.Parent;
        }

        return null;
    }
}