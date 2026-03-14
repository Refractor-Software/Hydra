namespace Hydra.Studio.Core.Selection;

public sealed class SelectionContext : ISelectionContext
{
    private List<object> _selected = [];

    public IReadOnlyList<object> SelectedObjects => _selected;

    public event EventHandler<IReadOnlyList<object>>? SelectionChanged;

    public IReadOnlyList<T> GetSelected<T>() where T : class =>
        _selected.OfType<T>().ToList();

    public void Select(IEnumerable<object> objects)
    {
        _selected = [.. objects];
        SelectionChanged?.Invoke(this, _selected);
    }

    public void Clear()
    {
        _selected = [];
        SelectionChanged?.Invoke(this, _selected);
    }
}