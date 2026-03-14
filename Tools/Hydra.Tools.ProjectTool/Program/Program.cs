using System.Text.Json;
using Hydra.Tools.ProjectTool.CMake;
using Hydra.Tools.ProjectTool.Project;

// ---------------------------------------------------------------------------
// hydra — Hydra Engine project tool
//
// Commands:
//   new    <ProjectName> --path <dir> [--hydra-version <ver>] [--platforms <p1,p2>]
//   update <path-to.hyproject>
// ---------------------------------------------------------------------------

if (args.Length == 0)
{
    PrintHelp();
    return 1;
}

var command = args[0].ToLowerInvariant();

return command switch
{
    "new" => RunNew(args[1..]),
    "update" => RunUpdate(args[1..]),
    "--help" or "-h" or "help" => PrintHelp(),
    _ => Error($"Unknown command '{args[0]}'. Run 'hydra --help' for usage.")
};

// ---------------------------------------------------------------------------
// new
// ---------------------------------------------------------------------------

static int RunNew(string[] args)
{
    // --- Parse args ---
    string? projectName = null;
    string? outputPath = null;
    string? hydraVersion = null;
    var platforms = new List<string> { "Windows" };

    for (var i = 0; i < args.Length; i++)
    {
        switch (args[i])
        {
            case "--path" or "-p" when i + 1 < args.Length:
                outputPath = args[++i];
                break;
            case "--hydra-version" or "-hv" when i + 1 < args.Length:
                hydraVersion = args[++i];
                break;
            case "--platforms" when i + 1 < args.Length:
                platforms = [.. args[++i].Split(',', StringSplitOptions.RemoveEmptyEntries)];
                break;
            default:
                if (!args[i].StartsWith("--") && projectName is null)
                    projectName = args[i];
                else
                    Console.WriteLine($"Warning: unknown argument '{args[i]}' ignored.");
                break;
        }
    }

    if (string.IsNullOrWhiteSpace(projectName))
        return Error("Project name is required. Usage: hydra new <ProjectName> --path <dir>");

    outputPath ??= Path.Combine(Directory.GetCurrentDirectory(), projectName);

    // --- Build descriptor ---
    var project = new HydraProject
    {
        Name = projectName,
        HydraVersion = hydraVersion ?? "",
        Platforms = platforms
    };

    Console.WriteLine($"Creating project '{projectName}' at {outputPath}");
    Directory.CreateDirectory(outputPath);

    // --- Generate ---
    ProjectStructureGenerator.Generate(outputPath, project);
    CMakePresetsGenerator.Generate(outputPath, project);
    SaveDescriptor(outputPath, project);

    Console.WriteLine();
    Console.WriteLine("Next steps:");
    Console.WriteLine($"  1. Edit CMakeUserPresets.json and set Hydra_ROOT to your engine install path.");
    Console.WriteLine($"  2. Run: hydra update {Path.Combine(outputPath, $"{projectName}.hyproject")}");
    Console.WriteLine($"     (or run cmake --preset Windows-VisualStudio manually)");

    return 0;
}

// ---------------------------------------------------------------------------
// update
// ---------------------------------------------------------------------------

static int RunUpdate(string[] args)
{
    if (args.Length == 0)
        return Error("Usage: hydra update <path-to.hyproject>");

    var descriptorPath = args[0];
    if (!File.Exists(descriptorPath))
        return Error($"Project descriptor not found: {descriptorPath}");

    HydraProject? project;
    try
    {
        var json = File.ReadAllText(descriptorPath);
        project = JsonSerializer.Deserialize<HydraProject>(json);
    }
    catch (Exception ex)
    {
        return Error($"Failed to read descriptor: {ex.Message}");
    }

    if (project is null)
        return Error("Descriptor was empty or invalid.");

    var projectDir = Path.GetDirectoryName(Path.GetFullPath(descriptorPath))!;

    Console.WriteLine($"Updating project '{project.Name}' at {projectDir}");

    // Regenerate presets (always safe to overwrite — they're generated artifacts)
    CMakePresetsGenerator.GeneratePresets(projectDir, project);

    // Re-run CMake to refresh project files
    Console.WriteLine("Regenerating project files...");
    var ok = CMakeRunner.GenerateProjectFiles(projectDir, project);

    return ok ? 0 : 1;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static void SaveDescriptor(string projectDir, HydraProject project)
{
    var options = new JsonSerializerOptions { WriteIndented = true };
    var json = JsonSerializer.Serialize(project, options);
    var path = Path.Combine(projectDir, $"{project.Name}.hyproject");
    File.WriteAllText(path, json);
    Console.WriteLine($"  Created {project.Name}.hyproject");
}

static int PrintHelp()
{
    Console.WriteLine("""
                      hydra — Hydra project tool

                      Usage:
                        hydra new <ProjectName> [options]
                        hydra update <path-to.hyproject>

                      new options:
                        --path, -p <dir>            Output directory (default: ./<ProjectName>)
                        --hydra-version, -hv <ver>  Engine version to target
                        --platforms <p1,p2,...>     Target platforms (default: Windows)

                      Examples:
                        hydra new MyGame --path C:/Projects/MyGame --hydra-version 0.1.0
                        hydra update C:/Projects/MyGame/MyGame.hyproject
                      """);
    return 0;
}

static int Error(string message)
{
    Console.Error.WriteLine($"Error: {message}");
    return 1;
}