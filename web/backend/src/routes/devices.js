const router = require('express').Router();
const { PrismaClient } = require('@prisma/client');
const { body, validationResult } = require('express-validator');
const authMiddleware = require('../middleware/auth');

const prisma = new PrismaClient();

router.use(authMiddleware);

/* Короткий код привязки устройства: 6 символов без похожих знаков (0/O, 1/I/L).
   Пользователь вводит его на странице настройки SmartTag вместо длинного UUID. */
function genPairingCode() {
  const alphabet = 'ABCDEFGHJKMNPQRSTUVWXYZ23456789';
  let s = '';
  for (let i = 0; i < 6; i++) s += alphabet[Math.floor(Math.random() * alphabet.length)];
  return s;
}

/* Сгенерировать код, гарантированно не занятый другим устройством */
async function uniquePairingCode() {
  for (let attempt = 0; attempt < 10; attempt++) {
    const code = genPairingCode();
    const exists = await prisma.device.findUnique({ where: { pairingCode: code } });
    if (!exists) return code;
  }
  throw new Error('could not generate unique pairing code');
}

// GET /api/devices — все устройства пользователя
router.get('/', async (req, res) => {
  try {
    const devices = await prisma.device.findMany({
      where: { userId: req.userId },
      include: {
        profiles: {
          orderBy: { position: 'asc' },
        },
      },
    });
    res.json({ devices });
  } catch (err) {
    console.error(err);
    res.status(500).json({ error: 'Ошибка сервера' });
  }
});

// POST /api/devices — добавить устройство
router.post('/', [
  body('name').trim().notEmpty().withMessage('Название обязательно'),
], async (req, res) => {
  const errors = validationResult(req);
  if (!errors.isEmpty()) {
    return res.status(400).json({ errors: errors.array() });
  }

  try {
    const pairingCode = await uniquePairingCode();

    const device = await prisma.device.create({
      data: {
        userId: req.userId,
        name: req.body.name,
        pairingCode,
      },
    });
    res.status(201).json({ device });
  } catch (err) {
    console.error(err);
    res.status(500).json({ error: 'Ошибка сервера' });
  }
});

// PUT /api/devices/:id — переименовать устройство
router.put('/:id', [
  body('name').trim().notEmpty().withMessage('Название обязательно'),
], async (req, res) => {
  const errors = validationResult(req);
  if (!errors.isEmpty()) {
    return res.status(400).json({ errors: errors.array() });
  }

  try {
    const id = parseInt(req.params.id);

    const device = await prisma.device.findFirst({
      where: { id, userId: req.userId },
    });
    if (!device) return res.status(404).json({ error: 'Устройство не найдено' });

    const updated = await prisma.device.update({
      where: { id },
      data: { name: req.body.name },
    });

    res.json({ device: updated });
  } catch (err) {
    console.error(err);
    res.status(500).json({ error: 'Ошибка сервера' });
  }
});

// DELETE /api/devices/:id — удалить устройство
router.delete('/:id', async (req, res) => {
  try {
    const id = parseInt(req.params.id);

    const device = await prisma.device.findFirst({
      where: { id, userId: req.userId },
    });
    if (!device) return res.status(404).json({ error: 'Устройство не найдено' });

    // Удаляем профили устройства и само устройство
    await prisma.profile.deleteMany({ where: { deviceId: id } });
    await prisma.device.delete({ where: { id } });

    res.json({ message: 'Устройство удалено' });
  } catch (err) {
    console.error(err);
    res.status(500).json({ error: 'Ошибка сервера' });
  }
});

module.exports = router;