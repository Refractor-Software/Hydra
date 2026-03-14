namespace Hydra.Studio.Core.Panels;

public sealed record EditorPanelDescriptor(
    string Id,
    string Title,
    Func<IEditorPanel> Factory
);