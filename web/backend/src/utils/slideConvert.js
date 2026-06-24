const sharp = require('sharp');

const SCREEN_W = 320;
const SCREEN_H = 240;

/**
 * Конвертирует картинку (Buffer любого формата) в сырой RGB565-поток
 * для экрана ST7796U: 320x240, порядок строк сверху-вниз, пиксель 16 бит.
 * Экран в режиме BGR — учитываем при упаковке.
 * Возвращает Buffer ровно 320*240*2 = 153600 байт.
 */
async function imageToSlideBin(inputBuffer, { bgr = true } = {}) {
  // ресайз с заполнением кадра (cover) + центр-кроп под 320x240
  const { data, info } = await sharp(inputBuffer)
    .resize(SCREEN_W, SCREEN_H, { fit: 'cover', position: 'centre' })
    .removeAlpha()
    .raw()
    .toBuffer({ resolveWithObject: true });
  // data — это RGB по 3 байта на пиксель (info.channels === 3)

  const out = Buffer.alloc(SCREEN_W * SCREEN_H * 2);
  let o = 0;
  for (let i = 0; i < data.length; i += 3) {
    const r = data[i], g = data[i + 1], b = data[i + 2];
    // RGB565: 5 бит R, 6 бит G, 5 бит B
    const r5 = r >> 3, g6 = g >> 2, b5 = b >> 3;
    let pixel;
    if (bgr) {
      // экран ждёт BGR: меняем местами R и B
      pixel = (b5 << 11) | (g6 << 5) | r5;
    } else {
      pixel = (r5 << 11) | (g6 << 5) | b5;
    }
    // little-endian (младший байт первым) — как пишет STM32 в память
    out[o++] = pixel & 0xFF;
    out[o++] = (pixel >> 8) & 0xFF;
  }
  return out;
}

module.exports = { imageToSlideBin, SCREEN_W, SCREEN_H };

// --- самопроверка (можно удалить) ---
if (require.main === module) {
  (async () => {
    // сделаем тестовую картинку: красный квадрат, чтобы проверить байты
    const test = await sharp({
      create: { width: 400, height: 300, channels: 3, background: { r: 255, g: 0, b: 0 } }
    }).png().toBuffer();

    const bin = await imageToSlideBin(test, { bgr: true });
    console.log('Размер .bin:', bin.length, '(ожидаем 153600):', bin.length === 153600 ? 'OK' : 'FAIL');
    // чистый красный в RGB565: r5=31,g6=0,b5=0. В BGR-упаковке -> (0<<11)|(0<<5)|31 = 0x001F
    // little-endian: байты 1F 00
    console.log('Первый пиксель (BGR, красный) байты:', bin[0].toString(16), bin[1].toString(16), '(ожидаем 1f 0)');
  })();
}