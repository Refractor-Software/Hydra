using System.Buffers.Binary;
using System.Net.Sockets;

namespace Hydra.Studio.Core.Networking;

public sealed class RuntimeConnection : IRuntimeConnection, IAsyncDisposable
{
    private TcpClient? _client;
    private NetworkStream? _stream;
    private CancellationTokenSource? _readCts;
    private Task? _readLoop;

    public bool IsConnected => _client?.Connected ?? false;

    public event EventHandler<ReadOnlyMemory<byte>>? MessageReceived;
    public event EventHandler? Connected;
    public event EventHandler? Disconnected;

    public async Task ConnectAsync(string host, int port, CancellationToken ct = default)
    {
        if (IsConnected)
            throw new InvalidOperationException("Already connected.");

        _client = new TcpClient();
        await _client.ConnectAsync(host, port, ct);
        _stream = _client.GetStream();

        _readCts = CancellationTokenSource.CreateLinkedTokenSource(ct);
        _readLoop = Task.Run(() => ReadLoopAsync(_readCts.Token), _readCts.Token);

        Connected?.Invoke(this, EventArgs.Empty);
    }

    public async Task DisconnectAsync()
    {
        if (_readCts is not null)
        {
            await _readCts.CancelAsync();
            if (_readLoop is not null)
                await _readLoop.ConfigureAwait(false);
        }

        _stream?.Dispose();
        _client?.Dispose();
        _stream = null;
        _client = null;
        _readCts = null;
        _readLoop = null;

        Disconnected?.Invoke(this, EventArgs.Empty);
    }

    public async Task SendAsync(ReadOnlyMemory<byte> data, CancellationToken ct = default)
    {
        if (_stream is null)
            throw new InvalidOperationException("Not connected.");

        // Length-prefix frame: 4 bytes big-endian length + payload
        var header = new byte[4];
        BinaryPrimitives.WriteInt32BigEndian(header, data.Length);

        await _stream.WriteAsync(header, ct);
        await _stream.WriteAsync(data, ct);
    }

    private async Task ReadLoopAsync(CancellationToken ct)
    {
        var header = new byte[4];

        try
        {
            while (!ct.IsCancellationRequested)
            {
                // Read 4-byte length prefix
                await ReadExactAsync(header, ct);
                var length = BinaryPrimitives.ReadInt32BigEndian(header);

                if (length <= 0 || length > 64 * 1024 * 1024) // 64MB sanity limit
                    throw new InvalidDataException($"Invalid message length: {length}");

                // Read payload
                var payload = new byte[length];
                await ReadExactAsync(payload, ct);

                MessageReceived?.Invoke(this, payload.AsMemory());
            }
        }
        catch (OperationCanceledException)
        {
            // Normal shutdown — swallow
        }
        catch (Exception)
        {
            // Connection dropped unexpectedly
            Disconnected?.Invoke(this, EventArgs.Empty);
        }
    }

    private async Task ReadExactAsync(byte[] buffer, CancellationToken ct)
    {
        var offset = 0;
        while (offset < buffer.Length)
        {
            var read = await _stream!.ReadAsync(buffer.AsMemory(offset), ct);
            if (read == 0)
                throw new EndOfStreamException("Connection closed by remote.");
            offset += read;
        }
    }

    public async ValueTask DisposeAsync()
    {
        await DisconnectAsync();
    }
}