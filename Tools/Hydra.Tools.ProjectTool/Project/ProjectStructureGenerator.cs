namespace Hydra.Tools.ProjectTool.Project;

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
        string path = Path.Combine(projectDir, "CMakeLists.txt");
        if (File.Exists(path)) return;

        // Build target_link_libraries list
        var modules = new List<string> { "Hydra::HydraFoundation" };
        foreach (string module in project.Modules)
            modules.Add($"Hydra::{module}");

        string linkLibraries = string.Join("\n    ", modules);

        // Build hydra_enable_packages block — only emit if packages are specified
        string enablePackages = project.Packages.Count > 0
            ? $"\nhydra_enable_packages(\n    {string.Join("\n    ", project.Packages)}\n)\n"
            : "";

        File.WriteAllText(path, $$"""
                                  cmake_minimum_required(VERSION 4.1)
                                  project({{project.Name}})

                                  find_package(Hydra REQUIRED)
                                  {{enablePackages}}
                                  add_executable({{project.Name}} Source/main.cpp)
                                  target_link_libraries({{project.Name}} PRIVATE
                                      {{linkLibraries}}
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