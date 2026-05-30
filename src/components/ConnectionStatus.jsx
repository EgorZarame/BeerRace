export default function ConnectionStatus({ connected }) {
  return (
    <div className={`connection-status ${connected ? 'online' : 'offline'}`}>
      <span className="connection-status__dot" aria-hidden="true" />
      <span className="connection-status__label">
        {connected ? 'ESP32 Online' : 'Device Offline'}
      </span>
    </div>
  );
}
