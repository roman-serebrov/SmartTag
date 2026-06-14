import { useState, useEffect } from 'react';
import api from '../api/client';
import './Profiles.css';

function Profiles() {
  const [devices, setDevices] = useState([]);
  const [selectedDevice, setSelectedDevice] = useState(null);
  const [profiles, setProfiles] = useState([]);
  const [loading, setLoading] = useState(true);
  const [pushing, setPushing] = useState(false);
  const [error, setError] = useState('');
  const [showForm, setShowForm] = useState(false);
  const [editingProfile, setEditingProfile] = useState(null);
  const [deviceIp, setDeviceIp] = useState('');

  const [form, setForm] = useState({
    title: '',
    url: '',
    icon: '',
    position: 0,
    isRating: false,
  });

  useEffect(() => {
    fetchDevices();
  }, []);

  useEffect(() => {
    if (selectedDevice) fetchProfiles(selectedDevice);
  }, [selectedDevice]);

  const fetchDevices = async () => {
    try {
      const res = await api.get('/devices');
      setDevices(res.data.devices);
      if (res.data.devices.length > 0) {
        setSelectedDevice(res.data.devices[0].id);
      }
    } catch {
      setError('Не удалось загрузить устройства');
    } finally {
      setLoading(false);
    }
  };

  const fetchProfiles = async (deviceId) => {
    try {
      const res = await api.get(`/profiles/${deviceId}`);
      setProfiles(res.data.profiles);
    } catch {
      setError('Не удалось загрузить профили');
    }
  };

  const resetForm = () => {
    setForm({ title: '', url: '', icon: '', position: profiles.length, isRating: false });
    setEditingProfile(null);
    setShowForm(false);
  };

  const handleSubmit = async (e) => {
    e.preventDefault();
    try {
      if (editingProfile) {
        await api.put(`/profiles/${selectedDevice}/${editingProfile.id}`, form);
      } else {
        await api.post(`/profiles/${selectedDevice}`, form);
      }
      await fetchProfiles(selectedDevice);
      resetForm();
    } catch (err) {
      setError(err.response?.data?.error || 'Ошибка сохранения');
    }
  };

  const deleteProfile = async (id) => {
    if (!confirm('Удалить профиль?')) return;
    try {
      await api.delete(`/profiles/${selectedDevice}/${id}`);
      setProfiles(prev => prev.filter(p => p.id !== id));
    } catch {
      setError('Не удалось удалить профиль');
    }
  };

  const startEdit = (profile) => {
    setForm({
      title: profile.title,
      url: profile.url,
      icon: profile.icon || '',
      position: profile.position,
      isRating: profile.isRating,
    });
    setEditingProfile(profile);
    setShowForm(true);
  };

  const pushToDevice = async () => {
    if (!deviceIp) {
      setError('Введите IP устройства');
      return;
    }
    setPushing(true);
    try {
      await api.post(`/profiles/${selectedDevice}/push`, { deviceIp });
      alert('Профили отправлены на устройство!');
    } catch {
      setError('Не удалось подключиться к устройству');
    } finally {
      setPushing(false);
    }
  };

  if (loading) return <div>Загрузка...</div>;

  return (
    <div className="profiles-page">
      <div className="profiles-header">
        <h1>Профили</h1>
        <div className="profiles-header-actions">
          {devices.length > 1 && (
            <select
              value={selectedDevice}
              onChange={(e) => setSelectedDevice(parseInt(e.target.value))}
            >
              {devices.map(d => (
                <option key={d.id} value={d.id}>{d.name}</option>
              ))}
            </select>
          )}
          <button className="btn-primary" onClick={() => { resetForm(); setShowForm(true); }}>
            + Добавить профиль
          </button>
        </div>
      </div>

      {error && <div className="profiles-error">{error}</div>}

      {showForm && (
        <div className="profile-form-card">
          <h2>{editingProfile ? 'Редактировать профиль' : 'Новый профиль'}</h2>
          <form onSubmit={handleSubmit}>
            <div className="form-row">
              <label>Название</label>
              <input
                type="text"
                placeholder="Instagram"
                value={form.title}
                onChange={(e) => setForm(p => ({ ...p, title: e.target.value }))}
                required
              />
            </div>
            <div className="form-row">
              <label>Ссылка</label>
              <input
                type="url"
                placeholder="https://instagram.com/..."
                value={form.url}
                onChange={(e) => setForm(p => ({ ...p, url: e.target.value }))}
                required
              />
            </div>
            <div className="form-row">
              <label>Иконка (имя файла)</label>
              <input
                type="text"
                placeholder="instagram.bin"
                value={form.icon}
                onChange={(e) => setForm(p => ({ ...p, icon: e.target.value }))}
              />
            </div>
            <div className="form-row">
              <label>Позиция</label>
              <input
                type="number"
                min="0"
                value={form.position}
                onChange={(e) => setForm(p => ({ ...p, position: parseInt(e.target.value) }))}
              />
            </div>
            <div className="form-row form-row-check">
              <label>
                <input
                  type="checkbox"
                  checked={form.isRating}
                  onChange={(e) => setForm(p => ({ ...p, isRating: e.target.checked }))}
                />
                Экран рейтинга
              </label>
            </div>
            <div className="form-actions">
              <button type="submit" className="btn-primary">
                {editingProfile ? 'Сохранить' : 'Добавить'}
              </button>
              <button type="button" className="btn-secondary" onClick={resetForm}>
                Отмена
              </button>
            </div>
          </form>
        </div>
      )}

      <div className="profiles-list">
        {profiles.length === 0 && (
          <div className="profiles-empty">Нет профилей. Добавьте первый!</div>
        )}
        {profiles.map(profile => (
          <div key={profile.id} className="profile-card">
            <div className="profile-pos">{profile.position + 1}</div>
            <div className="profile-info">
              <div className="profile-title">
                {profile.title}
                {profile.isRating && <span className="badge-rating">⭐ Рейтинг</span>}
              </div>
              <div className="profile-url">{profile.url}</div>
              {profile.icon && <div className="profile-icon-name">🖼 {profile.icon}</div>}
            </div>
            <div className="profile-actions">
              <button className="btn-secondary" onClick={() => startEdit(profile)}>
                Изменить
              </button>
              <button className="btn-danger" onClick={() => deleteProfile(profile.id)}>
                Удалить
              </button>
            </div>
          </div>
        ))}
      </div>

      {profiles.length > 0 && (
        <div className="push-section">
          <h2>Отправить на устройство</h2>
          <div className="push-row">
            <input
              type="text"
              placeholder="IP устройства (например 192.168.1.100)"
              value={deviceIp}
              onChange={(e) => setDeviceIp(e.target.value)}
            />
            <button
              className="btn-push"
              onClick={pushToDevice}
              disabled={pushing}
            >
              {pushing ? 'Отправка...' : '📡 Обновить устройство'}
            </button>
          </div>
        </div>
      )}
    </div>
  );
}

export default Profiles;