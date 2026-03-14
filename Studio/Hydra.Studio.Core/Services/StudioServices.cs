using CommunityToolkit.Mvvm.Messaging;
using Hydra.Studio.Core.Commands;
using Hydra.Studio.Core.History;
using Hydra.Studio.Core.Networking;
using Hydra.Studio.Core.Panels;
using Hydra.Studio.Core.Selection;

namespace Hydra.Studio.Core.Services;

public sealed class StudioServices : IStudioServices
{
    private readonly List<EditorPanelDescriptor> _panels = [];
    private readonly List<StudioCommandDescriptor> _commands = [];

    public IAssetRegistry AssetRegistry { get; }
    public ISelectionContext SelectionContext { get; }
    public IRuntimeConnection RuntimeConnection { get; }
    public IMessageProtocol MessageProtocol { get; }
    public IMessenger Messenger { get; }
    public IHistoryService History { get; }

    public IReadOnlyList<EditorPanelDescriptor> Panels => _panels;
    public IReadOnlyList<StudioCommandDescriptor> Commands => _commands;

    public StudioServices(
        IAssetRegistry assetRegistry,
        ISelectionContext selectionContext,
        IRuntimeConnection runtimeConnection,
        IMessageProtocol messageProtocol,
        IMessenger messenger,
        IHistoryService history)
    {
        AssetRegistry = assetRegistry;
        SelectionContext = selectionContext;
        RuntimeConnection = runtimeConnection;
        MessageProtocol = messageProtocol;
        Messenger = messenger;
        History = history;
    }

    public void RegisterPanel(EditorPanelDescriptor descriptor)
    {
        if (_panels.Any(p => p.Id == descriptor.Id))
            throw new InvalidOperationException($"Panel '{descriptor.Id}' is already registered.");
        _panels.Add(descriptor);
    }

    public void RegisterCommand(StudioCommandDescriptor descriptor)
    {
        if (_commands.Any(c => c.Id == descriptor.Id))
            throw new InvalidOperationException($"Command '{descriptor.Id}' is already registered.");
        _commands.Add(descriptor);
    }
}