const router = require('express').Router();
const { PrismaClient } = require('@prisma/client');
const { body, validationResult } = require('express-validator');
const authMiddleware = require('../middleware/auth');

const prisma = new PrismaClient();

// Все роуты защищены авторизацией
router.use(authMiddleware);

// GET /api/profiles/:deviceId — получить все профили устройства
router.get('/:deviceId', async (req, res) => {
  try {
    const deviceId = parseInt(req.params.deviceId);

    // Проверяем что устройство принадлежит пользователю
    const device = await prisma.device.findFirst({
      where: { id: deviceId, userId: req.userId },
    });
    if (!device) return res.status(404).json({ error: 'Устройство не найдено' });

    const profiles = await prisma.profile.findMany({
      where: { deviceId },
      orderBy: { position: 'asc' },
    });

    res.json({ profiles });
  } catch (err) {
    console.error(err);
    res.status(500).json({ error: 'Ошибка сервера' });
  }
});

// POST /api/profiles/:deviceId — создать профиль
router.post('/:deviceId', [
  body('title').trim().notEmpty().withMessage('Название обязательно'),
  body('url').isURL().withMessage('Некорректный URL'),
  body('icon').optional().trim(),
  body('position').optional().isInt({ min: 0 }),
  body('isRating').optional().isBoolean(),
], async (req, res) => {
  const errors = validationResult(req);
  if (!errors.isEmpty()) {
    return res.status(400).json({ errors: errors.array() });
  }

  try {
    const deviceId = parseInt(req.params.deviceId);

    const device = await prisma.device.findFirst({
      where: { id: deviceId, userId: req.userId },
    });
    if (!device) return res.status(404).json({ error: 'Устройство не найдено' });

    const { title, url, icon, position, isRating } = req.body;

    const profile = await prisma.profile.create({
      data: { deviceId, title, url, icon, position: position ?? 0, isRating: isRating ?? false },
    });

    res.status(201).json({ profile });
  } catch (err) {
    console.error(err);
    res.status(500).json({ error: 'Ошибка сервера' });
  }
});

// PUT /api/profiles/:deviceId/:id — обновить профиль
router.put('/:deviceId/:id', [
  body('title').optional().trim().notEmpty(),
  body('url').optional().isURL(),
  body('icon').optional().trim(),
  body('position').optional().isInt({ min: 0 }),
  body('isRating').optional().isBoolean(),
  body('active').optional().isBoolean(),
], async (req, res) => {
  const errors = validationResult(req);
  if (!errors.isEmpty()) {
    return res.status(400).json({ errors: errors.array() });
  }

  try {
    const deviceId = parseInt(req.params.deviceId);
    const id = parseInt(req.params.id);

    const device = await prisma.device.findFirst({
      where: { id: deviceId, userId: req.userId },
    });
    if (!device) return res.status(404).json({ error: 'Устройство не найдено' });

    const profile = await prisma.profile.update({
      where: { id },
      data: req.body,
    });

    res.json({ profile });
  } catch (err) {
    console.error(err);
    res.status(500).json({ error: 'Ошибка сервера' });
  }
});

// DELETE /api/profiles/:deviceId/:id — удалить профиль
router.delete('/:deviceId/:id', async (req, res) => {
  try {
    const deviceId = parseInt(req.params.deviceId);
    const id = parseInt(req.params.id);

    const device = await prisma.device.findFirst({
      where: { id: deviceId, userId: req.userId },
    });
    if (!device) return res.status(404).json({ error: 'Устройство не найдено' });

    await prisma.profile.delete({ where: { id } });

    res.json({ message: 'Профиль удалён' });
  } catch (err) {
    console.error(err);
    res.status(500).json({ error: 'Ошибка сервера' });
  }
});

// POST /api/profiles/:deviceId/push — отправить профили на устройство
router.post('/:deviceId/push', async (req, res) => {
  try {
    const deviceId = parseInt(req.params.deviceId);

    const device = await prisma.device.findFirst({
      where: { id: deviceId, userId: req.userId },
    });
    if (!device) return res.status(404).json({ error: 'Устройство не найдено' });

    const profiles = await prisma.profile.findMany({
      where: { deviceId, active: true },
      orderBy: { position: 'asc' },
    });

    // Отправляем на ESP8266
    const deviceIp = req.body.deviceIp; // IP устройства передаёт клиент
    if (!deviceIp) return res.status(400).json({ error: 'Не указан IP устройства' });

    const response = await fetch(`http://${deviceIp}/update`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ profiles }),
    });

    if (!response.ok) throw new Error('Устройство не ответило');

    res.json({ message: 'Профили отправлены на устройство', count: profiles.length });
  } catch (err) {
    console.error(err);
    res.status(500).json({ error: 'Не удалось подключиться к устройству' });
  }
});

module.exports = router;