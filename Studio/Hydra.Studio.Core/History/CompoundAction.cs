namespace Hydra.Studio.Core.History;

public sealed class CompoundAction : IUndoableAction
{
    private readonly IReadOnlyList<IUndoableAction> _actions;

    public string Description { get; }

    public CompoundAction(string description, IEnumerable<IUndoableAction> actions)
    {
        Description = description;
        _actions = [.. actions];
    }

    public void Execute()
    {
        foreach (var action in _actions)
        {
            action.Execute();
        }
    }

    public void Undo()
    {
        foreach (var action in _actions.Reverse())
        {
            action.Undo();
        }
    }
}