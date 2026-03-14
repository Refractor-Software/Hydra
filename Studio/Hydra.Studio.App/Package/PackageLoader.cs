using System.Diagnostics;
using System.IO;
using System.Runtime.Loader;
using Hydra.Studio.App.Studio;
using Hydra.Studio.Core.Modules;
using Hydra.Studio.Core.Services;

namespace Hydra.Studio.App.Package;

public sealed class PackageLoader
{
    private readonly IStudioServices _services;
    private readonly StudioEnvironment _environment;
    private readonly List<(AssemblyLoadContext Context, IStudioModule Module)> _loaded = [];

    public PackageLoader(IStudioServices services, StudioEnvironment environment)
    {
        _services = services;
        _environment = environment;
    }

    public void LoadAll()
    {
        // 1. Engine built-in packages from Hydra SDK
        string sdkPackages = Path.Combine(_environment.HydraRoot, "Packages");
        if (Directory.Exists(sdkPackages))
            LoadFromPackageRoot(sdkPackages);

        // 2. Game project packages
        if (_environment.Project is not null)
        {
            // Packages/ sibling to the .hyproject file — convention from project structure
            string? projectDir = Path.GetDirectoryName(
                Path.GetFullPath(
                    Environment.GetCommandLineArgs()
                        .FirstOrDefault(a => a.EndsWith(".hyproject", StringComparison.OrdinalIgnoreCase)
                        ) ?? ""
                )
            );

            if (!string.IsNullOrEmpty(projectDir))
            {
                string projectPackages = Path.Combine(projectDir, "Packages");
                if (Directory.Exists(projectPackages))
                    LoadFromPackageRoot(projectPackages);
            }
        }

        // 3. Built-in tool modules next to the executable
        string builtInModules = Path.Combine(AppContext.BaseDirectory, "StudioModules");
        if (Directory.Exists(builtInModules))
            LoadFromDirectory(builtInModules);
    }

    // Scans <root>/Package.*/Studio/*.dll
    private void LoadFromPackageRoot(string packagesRoot)
    {
        foreach (string packageDir in Directory.EnumerateDirectories(packagesRoot, "Package.*"))
        {
            string studioDir = Path.Combine(packageDir, "Studio");
            if (Directory.Exists(studioDir))
                LoadFromDirectory(studioDir);
        }
    }

    private void LoadFromDirectory(string directory)
    {
        foreach (string dll in Directory.EnumerateFiles(directory, "*.dll"))
            TryLoad(dll);
    }

    public void UnloadAll()
    {
        foreach (var (context, module) in _loaded)
        {
            try
            {
                module.Shutdown();
            }
            catch (Exception ex)
            {
                Debug.WriteLine($"Error shutting down module '{module.Id}': {ex}");
            }

            context.Unload();
        }

        _loaded.Clear();
    }

    private void TryLoad(string dllPath)
    {
        try
        {
            var context = new AssemblyLoadContext(
                name: Path.GetFileNameWithoutExtension(dllPath),
                isCollectible: true
            );
            var assembly = context.LoadFromAssemblyPath(dllPath);

            var moduleTypes = assembly.GetTypes()
                .Where(t => typeof(IStudioModule).IsAssignableFrom(t)
                            && !t.IsAbstract
                            && !t.IsInterface);

            foreach (var type in moduleTypes)
            {
                var module = (IStudioModule)Activator.CreateInstance(type)!;
                module.Initialize(_services);
                _loaded.Add((context, module));
                Debug.WriteLine($"Loaded studio module '{module.DisplayName}' from {Path.GetFileName(dllPath)}");
            }
        }
        catch (Exception ex)
        {
            Debug.WriteLine($"Failed to load package '{dllPath}': {ex}");
        }
    }
}