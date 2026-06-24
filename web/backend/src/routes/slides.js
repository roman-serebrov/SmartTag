const router = require('express').Router();
const { PrismaClient } = require('@prisma/client');
const authMiddleware = require('../middleware/auth');
const multer = require('multer');
const path = require('path');
const fs = require('fs');
const { imageToSlideBin } = require('../utils/slideConvert');

const prisma = new PrismaClient();
router.use(authMiddleware);

// Папка, куда складываем готовые .bin слайды (мастер-копии на сервере)
const SLIDES_DIR = path.join(__dirname, '..', '..', 'uploads', 'slides');
fs.mkdirSync(SLIDES_DIR, { recursive: true });

// Принимаем картинку в память (до 10 МБ), конвертацию делаем сами
const upload = multer({
  storage: multer.memoryStorage(),
  limits: { fileSize: 10 * 1024 * 1024 },
  fileFilter: (req, file, cb) => {
    if (/^image\//.test(file.mimetype)) cb(null, true);
    else cb(new Error('Только изображения'));
  },
});

// Проверка, что устройство принадлежит пользователю
async function ownDevice(deviceId, userId) {
  return prisma.device.findFirst({ where: { id: deviceId, userId } });
}

// GET /api/slides/:deviceId — список слайдов устройства
router.get('/:deviceId', async (req, res) => {
  try {
    const deviceId = parseInt(req.params.deviceId);
    if (!(await ownDevice(deviceId, req.userId)))
      return res.status(404).json({ error: 'Устройство не найдено' });

    const slides = await prisma.slide.findMany({
      where: { deviceId },
      orderBy: { position: 'asc' },
    });
    res.json({ slides });
  } catch (err) {
    console.error(err);
    res.status(500).json({ error: 'Ошибка сервера' });
  }
});

// POST /api/slides/:deviceId — загрузить картинку -> конвертировать -> сохранить .bin
router.post('/:deviceId', upload.single('image'), async (req, res) => {
  try {
    const deviceId = parseInt(req.params.deviceId);
    if (!(await ownDevice(deviceId, req.userId)))
      return res.status(404).json({ error: 'Устройство не найдено' });

    if (!req.file) return res.status(400).json({ error: 'Файл не получен' });

    // 1. Конвертация в формат экрана (320x240 RGB565 BGR, 153600 байт)
    const bin = await imageToSlideBin(req.file.buffer, { bgr: true });

    // 2. Сохраняем .bin на диск сервера
    const fileName = `slide_${deviceId}_${Date.now()}.bin`;
    fs.writeFileSync(path.join(SLIDES_DIR, fileName), bin);

    // 3. Запись в базу
    const last = await prisma.slide.findFirst({
      where: { deviceId },
      orderBy: { position: 'desc' },
    });
    const position = last ? last.position + 1 : 0;

    const slide = await prisma.slide.create({
      data: {
        deviceId,
        name: req.body.name?.trim() || req.file.originalname || `Слайд ${position + 1}`,
        binFile: fileName,
        sizeBytes: bin.length,   // всегда 153600 — но пусть будет, для контроля
        position,
        ready: true,             // конвертация прошла => готов к отправке
      },
    });

    res.status(201).json({ slide });
  } catch (err) {
    console.error('slide upload error:', err);
    res.status(500).json({ error: 'Не удалось обработать картинку' });
  }
});

// GET /api/slides/:deviceId/:id/preview — отдать .bin (для проверки/будущей отправки)
router.get('/:deviceId/:id/raw', async (req, res) => {
  try {
    const deviceId = parseInt(req.params.deviceId);
    if (!(await ownDevice(deviceId, req.userId)))
      return res.status(404).json({ error: 'Устройство не найдено' });

    const slide = await prisma.slide.findFirst({
      where: { id: parseInt(req.params.id), deviceId },
    });
    if (!slide) return res.status(404).json({ error: 'Слайд не найден' });

    const filePath = path.join(SLIDES_DIR, slide.binFile);
    if (!fs.existsSync(filePath)) return res.status(404).json({ error: 'Файл отсутствует' });

    res.type('application/octet-stream').send(fs.readFileSync(filePath));
  } catch (err) {
    console.error(err);
    res.status(500).json({ error: 'Ошибка сервера' });
  }
});

// DELETE /api/slides/:deviceId/:id — удалить слайд
router.delete('/:deviceId/:id', async (req, res) => {
  try {
    const deviceId = parseInt(req.params.deviceId);
    if (!(await ownDevice(deviceId, req.userId)))
      return res.status(404).json({ error: 'Устройство не найдено' });

    const slide = await prisma.slide.findFirst({
      where: { id: parseInt(req.params.id), deviceId },
    });
    if (!slide) return res.status(404).json({ error: 'Слайд не найден' });

    // удаляем файл с диска
    const filePath = path.join(SLIDES_DIR, slide.binFile);
    if (fs.existsSync(filePath)) fs.unlinkSync(filePath);

    await prisma.slide.delete({ where: { id: slide.id } });
    res.json({ message: 'Слайд удалён' });
  } catch (err) {
    console.error(err);
    res.status(500).json({ error: 'Ошибка сервера' });
  }
});

module.exports = router;