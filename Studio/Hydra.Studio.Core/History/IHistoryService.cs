namespace Hydra.Studio.Core.History;

public interface IHistoryService
{
    bool CanUndo { get; }
    bool CanRedo { get; }
    string? NextUndoDescription { get; }
    string? NextRedoDescription { get; }

    void Push(IUndoableAction action);
    void Undo();
    void Redo();
    void Clear();

    event EventHandler HistoryChanged;
}