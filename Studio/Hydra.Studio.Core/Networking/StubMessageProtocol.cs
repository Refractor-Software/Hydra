namespace Hydra.Studio.Core.Networking;

public sealed class StubMessageProtocol : IMessageProtocol
{
    public StubMessageProtocol(IRuntimeConnection runtimeConnection)
    {
    }

    public Task SendAsync<T>(T message, CancellationToken ct = default)
        => Task.CompletedTask;

    public void RegisterHandler<T>(Action<T> handler)
    {
        // No-op — implement when runtime message types are defined
    }
}