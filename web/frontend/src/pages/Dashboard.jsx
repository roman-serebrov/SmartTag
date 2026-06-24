import { useState, useEffect } from 'react';
import api from '../api/client';

const ONLINE_WINDOW_MS = 2 * 60 * 1000;
const isOnline = (d) => d.lastSeen && (Date.now() - new Date(d.lastSeen).getTime() < ONLINE_WINDOW_MS);

function Dashboard() {
  const [devices, setDevices] = useState([]);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    load();
    const t = setInterval(load, 30000); // живое обновление статистики
    return () => clearInterval(t);
  }, []);

  const load = async () => {
    try {
      const res = await api.get('/devices');
      setDevices(res.data.devices);
    } catch {
      /* молча — попробуем в следующий тик */
    } finally {
      setLoading(false);
    }
  };

  if (loading) return <div style={{ padding: 24 }}>Загрузка…</div>;

  // Сводные числа
  const totalDevices = devices.length;
  const onlineDevices = devices.filter(isOnline).length;
  const totalScans = devices.reduce((s, d) => s + (d.scanTotal || 0), 0);

  // Все профили со счётчиками, отсортированы по переходам
  const allProfiles = devices
    .flatMap(d => (d.profiles || []).map(p => ({ ...p, deviceName: d.name })))
    .sort((a, b) => (b.scanCount || 0) - (a.scanCount || 0));

  const maxScan = Math.max(1, ...allProfiles.map(p => p.scanCount || 0));

  const card = {
    background: '#1b1e24', border: '1px solid #2a2e36',
    borderRadius: 12, padding: 18,
  };

  return (
    <div style={{ padding: 24, maxWidth: 900, margin: '0 auto' }}>
      <h1 style={{ marginBottom: 4 }}>Дашборд</h1>
      <p style={{ color: '#9aa0a6', marginTop: 0 }}>Статистика переходов и обзор устройств</p>

      {/* Сводка */}
      <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: 14, margin: '20px 0' }}>
        <div style={card}>
          <div style={{ color: '#9aa0a6', fontSize: 13 }}>Переходов всего</div>
          <div style={{ fontSize: 30, fontWeight: 700, marginTop: 4 }}>{totalScans}</div>
        </div>
        <div style={card}>
          <div style={{ color: '#9aa0a6', fontSize: 13 }}>Устройств</div>
          <div style={{ fontSize: 30, fontWeight: 700, marginTop: 4 }}>{totalDevices}</div>
        </div>
        <div style={card}>
          <div style={{ color: '#9aa0a6', fontSize: 13 }}>Сейчас онлайн</div>
          <div style={{ fontSize: 30, fontWeight: 700, marginTop: 4, color: onlineDevices ? '#22c55e' : '#e8eaed' }}>
            {onlineDevices}
          </div>
        </div>
      </div>

      {/* Переходы по профилям */}
      <h2 style={{ fontSize: 18, marginBottom: 12 }}>Переходы по профилям</h2>
      {allProfiles.length === 0 ? (
        <div style={{ color: '#9aa0a6' }}>Профилей пока нет. Добавьте их во вкладке «Профили».</div>
      ) : (
        <div style={{ ...card, padding: 8 }}>
          {allProfiles.map((p, i) => {
            const val = p.scanCount || 0;
            const pct = Math.round((val / maxScan) * 100);
            return (
              <div key={p.id} style={{ padding: '10px 12px', borderBottom: i < allProfiles.length - 1 ? '1px solid #2a2e36' : 'none' }}>
                <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: 6 }}>
                  <span>
                    <strong>{p.title || '(без названия)'}</strong>
                    {p.isRating && <span style={{ marginLeft: 6, color: '#f59e0b', fontSize: 12 }}>⭐ рейтинг</span>}
                    <span style={{ color: '#6b7280', fontSize: 12, marginLeft: 8 }}>{p.deviceName}</span>
                  </span>
                  <strong>{val}</strong>
                </div>
                <div style={{ height: 6, background: '#11141a', borderRadius: 4, overflow: 'hidden' }}>
                  <div style={{ width: `${pct}%`, height: '100%', background: '#4c8bf5' }} />
                </div>
              </div>
            );
          })}
        </div>
      )}

      {/* Устройства */}
      <h2 style={{ fontSize: 18, margin: '24px 0 12px' }}>Устройства</h2>
      {devices.length === 0 ? (
        <div style={{ color: '#9aa0a6' }}>Нет устройств. Добавьте первое во вкладке «Устройства».</div>
      ) : (
        <div style={{ display: 'grid', gap: 10 }}>
          {devices.map(d => {
            const online = isOnline(d);
            return (
              <div key={d.id} style={{ ...card, display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                <div>
                  <div style={{ fontWeight: 600 }}>{d.name}</div>
                  <div style={{ color: '#9aa0a6', fontSize: 13, marginTop: 2 }}>
                    Профилей: {d.profiles?.length ?? 0} · Переходов: {d.scanTotal || 0}
                    {d.localIp ? ` · ${d.localIp}` : ''}
                  </div>
                </div>
                <span style={{
                  display: 'inline-flex', alignItems: 'center', gap: 6, fontSize: 13,
                  color: online ? '#22c55e' : '#9aa0a6',
                }}>
                  <span style={{ width: 8, height: 8, borderRadius: '50%', background: online ? '#22c55e' : '#9aa0a6' }} />
                  {online ? 'В сети' : 'Не в сети'}
                </span>
              </div>
            );
          })}
        </div>
      )}
    </div>
  );
}

export default Dashboard;