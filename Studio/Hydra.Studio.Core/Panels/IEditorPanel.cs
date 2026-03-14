using System.ComponentModel;

namespace Hydra.Studio.Core.Panels;

public interface IEditorPanel
{
    string Id { get; }
    string Title { get; }
    object CreateView(); // returns a WPF UIElement — object to avoid WPF dep in Core
    INotifyPropertyChanged CreateViewModel();
}