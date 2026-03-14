using System.Diagnostics;

namespace Hydra.ProjectTool;

/// <summary>
/// Runs CMake to configure and generate project files.
/// </summary>
public static class CMakeRunner
{
    /// <summary>
    /// Runs cmake --preset for the Visual Studio preset on each platform,
    /// producing a .sln in the Intermediate directory.
    /// </summary>
    public static bool GenerateProjectFiles(string projectDir, HydraProject project)
    {
        var allSucceeded = true;

        foreach (var platform in project.Platforms)
        {
            var preset = $"{platform}-VisualStudio";
            Console.WriteLine($"  Running cmake --preset {preset}...");

            var success = RunCMake(projectDir, ["--preset", preset]);
            if (success)
            {
                var slnPath = Path.Combine(projectDir, "Intermediate", $"{platform}-VisualStudio", $"{project.Name}.sln");
                Console.WriteLine($"  Solution: {slnPath}");
            }
            else
            {
                Console.Error.WriteLine($"  cmake --preset {preset} failed.");
                allSucceeded = false;
            }
        }

        return allSucceeded;
    }

    private static bool RunCMake(string workingDir, string[] args)
    {
        var cmakeArgs = string.Join(" ", args);

        var psi = new ProcessStartInfo
        {
            FileName = "cmake",
            Arguments = cmakeArgs,
            WorkingDirectory = workingDir,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            UseShellExecute = false
        };

        using var process = Process.Start(psi);
        if (process is null)
        {
            Console.Error.WriteLine("Failed to start cmake. Is it on your PATH?");
            return false;
        }

        // Stream output in real time
        process.OutputDataReceived += (_, e) => { if (e.Data is not null) Console.WriteLine($"    {e.Data}"); };
        process.ErrorDataReceived  += (_, e) => { if (e.Data is not null) Console.Error.WriteLine($"    {e.Data}"); };
        process.BeginOutputReadLine();
        process.BeginErrorReadLine();
        process.WaitForExit();

        return process.ExitCode == 0;
    }
}
