namespace Hydra.Studio.Core.Networking;

public interface IMessageProtocol
{
    Task SendAsync<T>(T message, CancellationToken ct = default);
    void RegisterHandler<T>(Action<T> handler);
}