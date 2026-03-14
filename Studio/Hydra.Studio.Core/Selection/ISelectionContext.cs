namespace Hydra.Studio.Core.Selection;

public interface ISelectionContext
{
    IReadOnlyList<object> SelectedObjects { get; }
    IReadOnlyList<T> GetSelected<T>() where T : class;
    void Select(IEnumerable<object> objects);
    void Clear();
    event EventHandler<IReadOnlyList<object>> SelectionChanged;
}