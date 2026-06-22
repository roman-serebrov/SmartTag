/* web/backend/src/routes/heartbeat.js
 *
 * Приём сигнала «я онлайн» от устройства (ESP8266).
 * Авторизация по короткому коду (pairingCode).
 * Дополнительно: привязка к АППАРАТНОМУ id (hwId = MAC), чтобы одну и ту же
 * физическую железку нельзя было завести в системе дважды.
 *   - hwId в схеме помечен @unique → две записи с одним MAC невозможны.
 *   - при первом heartbeat MAC прикрепляется к записи;
 *   - если этот MAC уже прикреплён к ДРУГОЙ записи — отказ (409).
 */

const router = require('express').Router();
const { PrismaClient } = require('@prisma/client');

const prisma = new PrismaClient();

// POST /api/devices/heartbeat  body: { code, hwId, localIp }
router.post('/heartbeat', async (req, res) => {
  try {
    const { code, hwId, localIp } = req.body || {};

    if (!code) {
      return res.status(400).json({ error: 'code required' });
    }

    const pairingCode = String(code).trim().toUpperCase();
    const mac = typeof hwId === 'string' && hwId.length ? hwId.trim().toUpperCase() : null;

    const device = await prisma.device.findUnique({ where: { pairingCode } });
    if (!device) {
      return res.status(401).json({ error: 'unknown device code' });
    }

    // Защита от двойной привязки одной железки
    if (mac) {
      const other = await prisma.device.findUnique({ where: { hwId: mac } });
      if (other && other.id !== device.id) {
        return res.status(409).json({ error: 'this hardware is already registered to another device entry' });
      }
    }

    await prisma.device.update({
      where: { id: device.id },
      data: {
        lastSeen: new Date(),
        localIp: typeof localIp === 'string' ? localIp.slice(0, 64) : null,
        // hwId проставляем один раз — при первом heartbeat
        ...(mac && !device.hwId ? { hwId: mac } : {}),
      },
    });

    res.json({ status: 'ok' });
  } catch (err) {
    // P2002 — нарушение уникального индекса (на случай гонки)
    if (err && err.code === 'P2002') {
      return res.status(409).json({ error: 'hardware already registered' });
    }
    console.error('heartbeat error:', err);
    res.status(500).json({ error: 'server error' });
  }
});

// GET /api/devices/profiles?code=CODE
// Устройство само тянет свои профили из базы (источник истины).
// Возвращаем СРАЗУ в формате протокола STM32 — ESP просто пересылает тело в UART.
router.get('/profiles', async (req, res) => {
  try {
    const code = String(req.query.code || '').trim().toUpperCase();
    if (!code) return res.status(400).type('text/plain').send('');

    const device = await prisma.device.findUnique({ where: { pairingCode: code } });
    if (!device) return res.status(401).type('text/plain').send('');

    const profiles = await prisma.profile.findMany({
      where: { deviceId: device.id, active: true },
      orderBy: { position: 'asc' },
    });

    // те же правила очистки, что и в push: убираем символы, ломающие парсер
    const clean = (s) => String(s ?? '').replace(/["|\r\n]/g, ' ').trim();

    let out = '';
    profiles.forEach((p, i) => {
      out += `P:${i}:${clean(p.title)}|${clean(p.url)}|${clean(p.icon)}|${p.isRating ? 1 : 0}\n`;
    });
    out += `PD:${profiles.length}\n`;

    res.type('text/plain').send(out);
  } catch (err) {
    console.error('pull profiles error:', err);
    res.status(500).type('text/plain').send('');
  }
});

module.exports = router;