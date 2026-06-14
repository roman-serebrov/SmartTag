const router = require('express').Router();

// POST /api/device/sync - device sends analytics, receives updated profiles
router.post('/sync', async (req, res) => {
  // TODO: save scan data, return updated profiles
  res.json({ profiles: [], firmware_update: null });
});

// GET /api/device/profiles/:deviceId - device fetches its profiles
router.get('/profiles/:deviceId', async (req, res) => {
  // TODO: fetch profiles for device
  res.json({ profiles: [] });
});

// GET /api/device/ota/:deviceId - check for firmware update
router.get('/ota/:deviceId', async (req, res) => {
  // TODO: check for available firmware
  res.json({ available: false, version: null, url: null });
});

module.exports = router;