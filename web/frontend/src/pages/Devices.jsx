import { useState, useEffect } from 'react';
import api from '../api/client';
import './Devices.css';

const ONLINE_WINDOW_MS = 2 * 60 * 1000; // онлайн = heartbeat был свежее 2 минут

function isOnline(device) {
  if (!device.lastSeen) return false;
  return Date.now() - new Date(device.lastSeen).getTime() < ONLINE_WINDOW_MS;
}

function Devices() {
  const [devices, setDevices] = useState([]);
  const [loading, setLoading] = useState(true);
  const [newName, setNewName] = useState('');
  const [adding, setAdding] = useState(false);
  const [error, setError] = useState('');

  useEffect(() => {
    fetchDevices();
    const t = setInterval(() => fetchDevices(true), 30000); // тихое автообновление статуса
    return () => clearInterval(t);
  }, []);

  const fetchDevices = async (silent = false) => {
    try {
      const res = await api.get('/devices');
      setDevices(res.data.devices);
    } catch {
      if (!silent) setError('Не удалось загрузить устройства');
    } finally {
      if (!silent) setLoading(false);
    }
  };

  const addDevice = async (e) => {
    e.preventDefault();
    if (!newName.trim()) return;
    setAdding(true);
    try {
      const res = await api.post('/devices', { name: newName });
      setDevices(prev => [...prev, res.data.device]);
      setNewName('');
    } catch {
      setError('Не удалось добавить устройство');
    } finally {
      setAdding(false);
    }
  };

  const deleteDevice = async (id) => {
    if (!confirm('Удалить устройство и все его профили?')) return;
    try {
      await api.delete(`/devices/${id}`);
      setDevices(prev => prev.filter(d => d.id !== id));
    } catch {
      setError('Не удалось удалить устройство');
    }
  };

  if (loading) return <div className="devices-loading">Загрузка...</div>;

  return (
    <div className="devices-page">
      <h1>Устройства</h1>

      {error && <div className="devices-error">{error}</div>}

      <div className="devices-list">
        {devices.length === 0 && (
          <div className="devices-empty">Нет устройств. Добавьте первое!</div>
        )}
        {devices.map(device => {
          const online = isOnline(device);
          return (
          <div key={device.id} className="device-card">
            <div className="device-info">
              <div className="device-icon">📟</div>
              <div>
                <div className="device-name">{device.name}</div>
                <div className="device-meta">
                  Профилей: {device.profiles?.length ?? 0}
                  {device.localIp ? ` · IP: ${device.localIp}` : ''}
                </div>

                {/* Статус подключения по heartbeat */}
                <div style={{
                  display: 'inline-flex', alignItems: 'center', gap: 6,
                  marginTop: 6, fontSize: 13,
                  color: online ? '#22c55e' : '#9aa0a6',
                }}>
                  <span style={{
                    width: 8, height: 8, borderRadius: '50%',
                    background: online ? '#22c55e' : '#9aa0a6',
                  }} />
                  {online ? 'Wi-Fi подключён' : 'Не в сети'}
                </div>

                {/* Код привязки — вводится на экране настройки устройства */}
                <div style={{ marginTop: 8, fontSize: 13, color: '#9aa0a6' }}>
                  Код для привязки:{' '}
                  <span style={{
                    fontFamily: 'monospace', fontSize: 16, fontWeight: 700,
                    letterSpacing: 1, color: '#e8eaed',
                    background: '#1b1e24', padding: '2px 8px', borderRadius: 6,
                  }}>
                    {device.pairingCode || '—'}
                  </span>
                </div>
              </div>
            </div>
            <div className="device-actions">
              <button
                className="btn-danger"
                onClick={() => deleteDevice(device.id)}
              >
                Удалить
              </button>
            </div>
          </div>
          );
        })}
      </div>

      <div className="add-device">
        <h2>Добавить устройство</h2>
        <form onSubmit={addDevice}>
          <input
            type="text"
            placeholder="Название устройства"
            value={newName}
            onChange={(e) => setNewName(e.target.value)}
          />
          <button type="submit" disabled={adding}>
            {adding ? 'Добавление...' : 'Добавить'}
          </button>
        </form>
        <p style={{ color: '#9aa0a6', fontSize: 13, marginTop: 10 }}>
          После добавления введите показанный «код для привязки» на экране
          настройки устройства — вместе с Wi-Fi сетью и паролем.
        </p>
      </div>
    </div>
  );
}

export default Devices;