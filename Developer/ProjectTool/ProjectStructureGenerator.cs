namespace Hydra.ProjectTool;

/// <summary>
/// Creates the on-disk folder structure and boilerplate files for a new Hydra game project.
/// </summary>
public static class ProjectStructureGenerator
{
    public static void Generate(string projectDir, HydraProject project)
    {
        // Core directories
        Directory.CreateDirectory(Path.Combine(projectDir, "Source"));
        Directory.CreateDirectory(Path.Combine(projectDir, "Content"));
        Directory.CreateDirectory(Path.Combine(projectDir, "Config"));

        // Gitignore intermediate and binary output
        WriteGitIgnore(projectDir);

        // Stub CMakeLists.txt
        WriteCMakeLists(projectDir, project);

        // Stub main.cpp — will be replaced by HydraMain module later
        WriteMainCpp(projectDir, project);

        Console.WriteLine($"  Created project structure at {projectDir}");
    }

    private static void WriteGitIgnore(string projectDir)
    {
        var path = Path.Combine(projectDir, ".gitignore");
        if (File.Exists(path)) return;

        File.WriteAllText(path, """
                                Intermediate/
                                Binaries/
                                CMakeUserPresets.json
                                """);
    }

    private static void WriteCMakeLists(string projectDir, HydraProject project)
    {
        var path = Path.Combine(projectDir, "CMakeLists.txt");
        if (File.Exists(path)) return;

        // Build the target_link_libraries list.
        // HydraFoundation is always included; additional modules appended.
        var modules = new List<string> { "Hydra::HydraFoundation" };
        foreach (var module in project.Modules)
            modules.Add($"Hydra::{module}");

        var linkLibraries = string.Join("\n    ", modules);

        File.WriteAllText(path, $"""
                                 cmake_minimum_required(VERSION 4.1)
                                 project({project.Name})

                                 find_package(Hydra REQUIRED)

                                 add_executable({project.Name} Source/main.cpp)
                                 target_link_libraries({project.Name} PRIVATE
                                     {linkLibraries}
                                 )
                                 """);
    }

    private static void WriteMainCpp(string projectDir, HydraProject project)
    {
        var path = Path.Combine(projectDir, "Source", "main.cpp");
        if (File.Exists(path)) return;

        File.WriteAllText(path, $$"""
                                  // {{project.Name}}
                                  // Stub entry point — replace with HydraMain module once available.

                                  int main()
                                  {
                                      return 0;
                                  }
                                  """);
    }
}