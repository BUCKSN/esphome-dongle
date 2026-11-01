// Профиль кондиционеров AUX. Кадр: 8 байт заголовка, тело, 2 байта CRC.
const PROFILE = (() => {
  const onOff = (v) => (v ? 'вкл' : 'выкл');
  const hex = (v) => v.toString(16).toUpperCase().padStart(2, '0');
  const MODES = { 0: 'авто', 0x20: 'холод', 0x40: 'осушение', 0x80: 'тепло', 0xc0: 'вентилятор' };
  const FANS = { 0x20: 'высокая', 0x40: 'средняя', 0x60: 'низкая', 0xa0: 'авто' };
  const UNKNOWN = { 3: 0x1f, 4: 0x40, 5: 0x1f, 6: 0x3f, 7: 0x11, 8: 0xff, 9: 0xff, 10: 0xc8, 11: 0xff, 12: 0xe7 };
  // Большой пакет: температуры и вентилятор меняются сами, индекс 3 повторяет малый пакет.
  const BIG_IGNORED = new Set([0, 1, 3, 5, 6, 7, 9, 12, 13, 14, 16, 23]);

  function decode(bytes) {
    if (bytes.length < 10 || bytes[0] !== 0xbb || bytes[2] !== 0x07) return null;
    const b = bytes.slice(8, 8 + bytes[6]);
    if (b.length === 15 && b[1] === 0x11) {
      const state = {
        'Уставка': (8 + (b[2] >> 3) + b[14] / 10).toFixed(1),
        'Жалюзи вертикальные': String(b[2] & 7),
        'Жалюзи горизонтальные': hex(b[3] & 0xe0),
        'Скорость': FANS[b[5] & 0xe0] || hex(b[5] & 0xe0),
        'Турбо': onOff(b[6] & 0x40),
        'Тихо': onOff(b[6] & 0x80),
        'Режим': MODES[b[7] & 0xe0] ?? hex(b[7] & 0xe0),
        'Сон': onOff(b[7] & 4),
        'Единицы': b[7] & 2 ? 'F' : 'C',
        'Я ТУТ': onOff(b[7] & 8),
        'Питание': onOff(b[10] & 0x20),
        'ТЭН': onOff(b[10] & 0x10),
        'Чистка': onOff(b[10] & 4),
        'Ион': onOff(b[10] & 2),
        'Ион, состояние': onOff(b[10] & 1),
        'Дисплей': onOff(b[12] & 0x10),
        'Продув': onOff(b[12] & 8),
        'Лимит мощности': onOff(b[13] & 0x80),
        'Лимит, значение': String(b[13] & 0x7f),
      };
      for (const i in UNKNOWN) state[`НЕИЗВЕСТНО: малый пакет, байт ${i}`] = hex(b[i] & UNKNOWN[i]);
      return { state, counter: b[4] & 0x3f };
    }
    if (b.length === 24 && b[1] === 0x21) {
      const state = {};
      b.forEach((v, i) => { if (!BIG_IGNORED.has(i)) state[`НЕИЗВЕСТНО: большой пакет, байт ${i}`] = hex(v); });
      return { state, samples: { 'Комната': b[7] - 32 + (b[23] & 15) / 10, 'Вход хладагента': b[9] - 32, 'ШИМ вентилятора': b[6] } };
    }
    return null;
  }

  const toggle = 'Нажми, дождись изменения, нажми ещё раз';
  const steps = [
    ['Подготовка', 'Включи кондиционер, режим ТЕПЛО', 0],
    ['ТЭН', toggle, 0], ['ДИСП', toggle, 0], ['ИОН', toggle, 0], ['СОН', toggle, 0], ['ТУРБО', toggle, 0], ['Я ТУТ', toggle, 0],
    ['Обдув снаружи, левая', 'Крышка закрыта. ' + toggle, 0],
    ['Обдув снаружи, правая', 'Крышка закрыта. ' + toggle, 0],
    ['Обдув внутри', 'Крышка открыта. Нажимай, пока положения не пойдут по кругу', 0],
    ['ВЕНТ', 'Нажимай, пока скорости не пойдут по кругу', 0],
    ['РЕЖИМ', 'Нажимай, пока режимы не пойдут по кругу', 0],
    ['ТИХО в вентиляторе', 'Кнопкой РЕЖИМ выбери вентилятор. ' + toggle, 0],
    ['ЭКО в холоде', 'Нажми ХОЛОД. ' + toggle, 0],
    ['ЧИСТКА', 'Выключи кондиционер. Нажми ЧИСТКА, запиши результат. Останови кнопкой ON/OFF', 0],
    ['ПРОДУВ', 'Кондиционер выключен. Нажми ПРОДУВ, запиши сигналы', 0],
  ];
  const en = {
    'Уставка': 'Setpoint', 'Жалюзи вертикальные': 'Vertical louver', 'Жалюзи горизонтальные': 'Horizontal louver',
    'Скорость': 'Fan speed', 'Турбо': 'Turbo', 'Тихо': 'Quiet', 'Режим': 'Mode', 'Сон': 'Sleep', 'Единицы': 'Units',
    'Я ТУТ': 'I FEEL', 'Питание': 'Power', 'ТЭН': 'Electric heater', 'Чистка': 'Clean', 'Ион': 'Ion', 'Ион, состояние': 'Ion, status',
    'Дисплей': 'Display', 'Продув': 'Mildew-proof', 'Лимит мощности': 'Power limit', 'Лимит, значение': 'Power limit, value',
    'вкл': 'on', 'выкл': 'off', 'авто': 'auto', 'холод': 'cool', 'осушение': 'dry', 'тепло': 'heat', 'вентилятор': 'fan',
    'высокая': 'high', 'средняя': 'medium', 'низкая': 'low', 'Комната': 'Room', 'Вход хладагента': 'Coolant inlet', 'ШИМ вентилятора': 'Fan PWM',
    'Подготовка': 'Preparation', 'ДИСП': 'DISPLAY', 'ИОН': 'ION', 'СОН': 'SLEEP', 'ТУРБО': 'TURBO',
    'Обдув снаружи, левая': 'Outer airflow button, left', 'Обдув снаружи, правая': 'Outer airflow button, right', 'Обдув внутри': 'Inner airflow button',
    'ВЕНТ': 'FAN', 'РЕЖИМ': 'MODE', 'ТИХО в вентиляторе': 'QUIET in fan mode', 'ЭКО в холоде': 'ECO in cooling', 'ЧИСТКА': 'CLEAN', 'ПРОДУВ': 'MILDEW-PROOF',
    'Включи кондиционер, режим ТЕПЛО': 'Turn the AC on in HEAT mode',
    [toggle]: 'Press, wait for a change, press again',
    ['Крышка закрыта. ' + toggle]: 'Cover closed. Press, wait for a change, press again',
    'Крышка открыта. Нажимай, пока положения не пойдут по кругу': 'Cover open. Keep pressing until positions repeat',
    'Нажимай, пока скорости не пойдут по кругу': 'Keep pressing until speeds repeat',
    'Нажимай, пока режимы не пойдут по кругу': 'Keep pressing until modes repeat',
    ['Кнопкой РЕЖИМ выбери вентилятор. ' + toggle]: 'Select fan with MODE. Press, wait for a change, press again',
    ['Нажми ХОЛОД. ' + toggle]: 'Press COOL. Press, wait for a change, press again',
    'Выключи кондиционер. Нажми ЧИСТКА, запиши результат. Останови кнопкой ON/OFF': 'Turn the AC off. Press CLEAN, note the result. Stop with ON/OFF',
    'Кондиционер выключен. Нажми ПРОДУВ, запиши сигналы': 'AC is off. Press MILDEW-PROOF, note the beeps',
  };
  return { name: 'AUX', dongle: 'HOMMYN HDN/WFN-02-01', decode, steps, i18n: { en } };
})();
if (typeof module !== 'undefined') module.exports = PROFILE;
