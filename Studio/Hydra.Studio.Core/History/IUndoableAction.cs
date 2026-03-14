namespace Hydra.Studio.Core.History;

public interface IUndoableAction
{
    string Description { get; }
    void Execute();
    void Undo();
}