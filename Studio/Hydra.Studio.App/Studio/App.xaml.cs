using System.Windows;
using CommunityToolkit.Mvvm.Messaging;
using Hydra.Studio.App.AssetRegistry;
using Hydra.Studio.App.Package;
using Hydra.Studio.Core.History;
using Hydra.Studio.Core.Networking;
using Hydra.Studio.Core.Selection;
using Hydra.Studio.Core.Services;
using Hydra.Studio.Shell.Windows;

namespace Hydra.Studio.App.Studio;

public partial class App : Application
{
    private StudioServices? _services;
    private PackageLoader? _packageLoader;

    protected override void OnStartup(StartupEventArgs e)
    {
        base.OnStartup(e);

        var environment = StudioEnvironment.Resolve(e.Args);

        _services      = CreateServices();
        _packageLoader = new PackageLoader(_services, environment);
        _packageLoader.LoadAll();

        var mainWindow = new MainWindow(_services);
        mainWindow.Show();
    }

    protected override async void OnExit(ExitEventArgs e)
    {
        _packageLoader?.UnloadAll();

        if (_services?.RuntimeConnection is RuntimeConnection rc)
            await rc.DisposeAsync();

        base.OnExit(e);
    }

    private static StudioServices CreateServices()
    {
        var runtimeConnection = new RuntimeConnection();
        var messageProtocol   = new StubMessageProtocol(runtimeConnection);

        return new StudioServices(
            assetRegistry:     new StubAssetRegistry(),
            selectionContext:  new SelectionContext(),
            runtimeConnection: runtimeConnection,
            messageProtocol:   messageProtocol,
            messenger:         WeakReferenceMessenger.Default,
            history:           new HistoryService()
        );
    }
}