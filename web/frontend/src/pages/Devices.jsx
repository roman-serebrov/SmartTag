import { useState, useEffect } from 'react';
import api from '../api/client';
import './Devices.css';

function Devices() {
  const [devices, setDevices] = useState([]);
  const [loading, setLoading] = useState(true);
  const [newName, setNewName] = useState('');
  const [adding, setAdding] = useState(false);
  const [error, setError] = useState('');

  useEffect(() => {
    fetchDevices();
  }, []);

  const fetchDevices = async () => {
    try {
      const res = await api.get('/devices');
      setDevices(res.data.devices);
    } catch {
      setError('Не удалось загрузить устройства');
    } finally {
      setLoading(false);
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
        {devices.map(device => (
          <div key={device.id} className="device-card">
            <div className="device-info">
              <div className="device-icon">📟</div>
              <div>
                <div className="device-name">{device.name}</div>
                <div className="device-meta">
                  Профилей: {device.profiles?.length ?? 0} · Токен: {device.token.slice(0, 8)}...
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
        ))}
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
      </div>
    </div>
  );
}

export default Devices;