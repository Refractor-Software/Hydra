using Hydra.Studio.Core.Services;

namespace Hydra.Studio.Core.Modules;

public interface IStudioModule
{
    string Id { get; }
    string DisplayName { get; }
    void Initialize(IStudioServices services);
    void Shutdown();
}