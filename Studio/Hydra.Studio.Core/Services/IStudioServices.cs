using CommunityToolkit.Mvvm.Messaging;
using Hydra.Studio.Core.Commands;
using Hydra.Studio.Core.History;
using Hydra.Studio.Core.Networking;
using Hydra.Studio.Core.Panels;
using Hydra.Studio.Core.Selection;

namespace Hydra.Studio.Core.Services;

public interface IStudioServices
{
    IAssetRegistry AssetRegistry { get; }
    ISelectionContext SelectionContext { get; }
    IRuntimeConnection RuntimeConnection { get; }
    IMessageProtocol MessageProtocol { get; }
    IHistoryService History { get; }
    IMessenger Messenger { get; }

    void RegisterPanel(EditorPanelDescriptor descriptor);
    void RegisterCommand(StudioCommandDescriptor descriptor);
}