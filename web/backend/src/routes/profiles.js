const router = require('express').Router();

// GET /api/profiles - all profiles for user
router.get('/', async (req, res) => {
  // TODO: fetch from DB
  res.json({ profiles: [] });
});

// POST /api/profiles - create profile
router.post('/', async (req, res) => {
  // TODO: save to DB
  res.json({ message: 'create profile - coming soon' });
});

// PUT /api/profiles/:id - update profile
router.put('/:id', async (req, res) => {
  // TODO: update in DB
  res.json({ message: 'update profile - coming soon' });
});

// DELETE /api/profiles/:id - delete profile
router.delete('/:id', async (req, res) => {
  // TODO: delete from DB
  res.json({ message: 'delete profile - coming soon' });
});

module.exports = router;