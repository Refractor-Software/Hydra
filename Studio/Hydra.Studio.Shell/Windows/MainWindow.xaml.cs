using System.Windows;
using Hydra.Studio.Core.Services;

namespace Hydra.Studio.Shell.Windows;

public partial class MainWindow : Window
{
    private readonly StudioServices _services;

    public MainWindow(StudioServices services)
    {
        _services = services;
        InitializeComponent();
    }
}