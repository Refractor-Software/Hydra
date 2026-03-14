namespace Hydra.Studio.Core.Networking;

public interface IRuntimeConnection
{
    bool IsConnected { get; }
    Task ConnectAsync(string host, int port, CancellationToken ct = default);
    Task DisconnectAsync();
    Task SendAsync(ReadOnlyMemory<byte> data, CancellationToken ct = default);
    event EventHandler<ReadOnlyMemory<byte>> MessageReceived;
    event EventHandler Connected;
    event EventHandler Disconnected;
}