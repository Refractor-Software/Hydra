namespace Hydra.Studio.Core.Commands;

public sealed record StudioCommandDescriptor(
    string Id,
    string Label,
    string? MenuPath,
    Func<IStudioCommand> Factory
);