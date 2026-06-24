import { useState, useEffect } from 'react';
import api from '../api/client';

const ONLINE_WINDOW_MS = 2 * 60 * 1000;
const isOnline = (d) => d && d.lastSeen && (Date.now() - new Date(d.lastSeen).getTime() < ONLINE_WINDOW_MS);

function Slides() {
  const [devices, setDevices] = useState([]);
  const [selectedDevice, setSelectedDevice] = useState(null);
  const [slides, setSlides] = useState([]);
  const [loading, setLoading] = useState(true);
  const [uploading, setUploading] = useState(false);
  const [error, setError] = useState('');

  useEffect(() => { fetchDevices(); }, []);
  useEffect(() => { if (selectedDevice) fetchSlides(selectedDevice); }, [selectedDevice]);

  const fetchDevices = async () => {
    try {
      const res = await api.get('/devices');
      setDevices(res.data.devices);
      if (res.data.devices.length > 0) setSelectedDevice(res.data.devices[0].id);
    } catch {
      setError('Не удалось загрузить устройства');
    } finally {
      setLoading(false);
    }
  };

  const fetchSlides = async (deviceId) => {
    try {
      const res = await api.get(`/slides/${deviceId}`);
      setSlides(res.data.slides);
    } catch {
      setError('Не удалось загрузить слайды');
    }
  };

  const handleUpload = async (e) => {
    const file = e.target.files?.[0];
    if (!file || !selectedDevice) return;
    setError('');
    setUploading(true);
    try {
      const form = new FormData();
      form.append('image', file);
      form.append('name', file.name.replace(/\.[^.]+$/, ''));
      await api.post(`/slides/${selectedDevice}`, form, {
        headers: { 'Content-Type': 'multipart/form-data' },
      });
      await fetchSlides(selectedDevice);
    } catch (err) {
      setError(err.response?.data?.error || 'Не удалось загрузить картинку');
    } finally {
      setUploading(false);
      e.target.value = ''; // сброс, чтобы можно было выбрать тот же файл снова
    }
  };

  const handleDelete = async (id) => {
    if (!confirm('Удалить слайд?')) return;
    try {
      await api.delete(`/slides/${selectedDevice}/${id}`);
      setSlides(prev => prev.filter(s => s.id !== id));
    } catch {
      setError('Не удалось удалить слайд');
    }
  };

  if (loading) return <div style={{ padding: 24 }}>Загрузка…</div>;

  const dev = devices.find(d => d.id === selectedDevice);
  const online = isOnline(dev);

  const card = { background: '#1b1e24', border: '1px solid #2a2e36', borderRadius: 12, padding: 16 };

  return (
    <div style={{ padding: 24, maxWidth: 820, margin: '0 auto' }}>
      <h1 style={{ marginBottom: 4 }}>Слайды</h1>
      <p style={{ color: '#9aa0a6', marginTop: 0 }}>
        Промо-картинки, которые устройство показывает в простое (заставка).
      </p>

      {error && (
        <div style={{ background: '#3b1d1d', border: '1px solid #5e2a2a', color: '#f3b4b4',
          padding: 12, borderRadius: 10, margin: '12px 0' }}>{error}</div>
      )}

      {devices.length === 0 ? (
        <div style={{ color: '#9aa0a6' }}>Сначала добавьте устройство во вкладке «Устройства».</div>
      ) : (
        <>
          {/* Выбор устройства */}
          <div style={{ margin: '16px 0', display: 'flex', alignItems: 'center', gap: 12 }}>
            <label style={{ color: '#9aa0a6', fontSize: 14 }}>Устройство:</label>
            <select
              value={selectedDevice || ''}
              onChange={(e) => setSelectedDevice(parseInt(e.target.value))}
              style={{ padding: '8px 12px', background: '#11141a', color: '#e8eaed',
                border: '1px solid #2a2e36', borderRadius: 8, fontSize: 14 }}
            >
              {devices.map(d => <option key={d.id} value={d.id}>{d.name}</option>)}
            </select>
            <span style={{ fontSize: 13, color: online ? '#22c55e' : '#9aa0a6' }}>
              {online ? '● в сети' : '○ не в сети'}
            </span>
          </div>

          {/* Загрузка */}
          <div style={{ ...card, marginBottom: 20 }}>
            <label style={{
              display: 'inline-block', padding: '12px 18px', borderRadius: 10,
              background: uploading ? '#2a2e36' : '#4c8bf5', color: '#fff',
              fontWeight: 600, cursor: uploading ? 'default' : 'pointer',
            }}>
              {uploading ? 'Обработка…' : '＋ Загрузить картинку'}
              <input type="file" accept="image/*" onChange={handleUpload}
                disabled={uploading} style={{ display: 'none' }} />
            </label>
            <p style={{ color: '#6b7280', fontSize: 12, marginTop: 10, marginBottom: 0 }}>
              Любая картинка автоматически обрежется под экран 320×240. После обработки
              слайд помечается «готов» и его можно будет отправить на устройство.
            </p>
          </div>

          {/* Список слайдов */}
          <h2 style={{ fontSize: 18, marginBottom: 12 }}>Слайды устройства</h2>
          {slides.length === 0 ? (
            <div style={{ color: '#9aa0a6' }}>Слайдов пока нет. Загрузите первый.</div>
          ) : (
            <div style={{ display: 'grid', gap: 10 }}>
              {slides.map(s => (
                <div key={s.id} style={{ ...card, display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                  <div>
                    <div style={{ fontWeight: 600 }}>{s.name}</div>
                    <div style={{ color: '#9aa0a6', fontSize: 13, marginTop: 2 }}>
                      {s.ready
                        ? <span style={{ color: '#22c55e' }}>✓ готов к отправке</span>
                        : <span style={{ color: '#f59e0b' }}>обработка…</span>}
                      <span style={{ color: '#6b7280' }}> · {(s.sizeBytes / 1024).toFixed(0)} КБ</span>
                    </div>
                  </div>
                  <div style={{ display: 'flex', gap: 8 }}>
                    <button
                      disabled
                      title="Появится на следующем этапе (передача по UART)"
                      style={{ padding: '8px 14px', borderRadius: 8, border: '1px solid #2a2e36',
                        background: 'transparent', color: '#6b7280', cursor: 'not-allowed' }}
                    >
                      📡 На устройство
                    </button>
                    <button
                      onClick={() => handleDelete(s.id)}
                      style={{ padding: '8px 14px', borderRadius: 8, border: 0,
                        background: '#3b1d1d', color: '#f3b4b4', cursor: 'pointer' }}
                    >
                      Удалить
                    </button>
                  </div>
                </div>
              ))}
            </div>
          )}
        </>
      )}
    </div>
  );
}

export default Slides;