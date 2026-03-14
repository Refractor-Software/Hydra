namespace Hydra.Studio.Core.Commands;

public interface IStudioCommand
{
    string Id { get; }
    string Label { get; }
    string? MenuPath { get; }
    bool CanExecute(object? parameter);
    void Execute(object? parameter);
    event EventHandler? CanExecuteChanged;
}