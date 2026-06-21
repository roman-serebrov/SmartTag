const express = require('express');
const cors = require('cors');
const helmet = require('helmet');
const rateLimit = require('express-rate-limit');
const cookieParser = require('cookie-parser');
require('dotenv').config();

const app = express();
const PORT = process.env.PORT || 3001;

// Security headers
app.use(helmet());

// CORS
app.use(cors({
  origin: process.env.FRONTEND_URL || 'http://localhost:5173',
  credentials: true, // нужно для cookie
}));

app.use(express.json());
app.use(cookieParser());

// Rate limiting на auth эндпоинты
const authLimiter = rateLimit({
  windowMs: 15 * 60 * 1000, // 15 минут
  max: 10, // максимум 10 попыток
  message: { error: 'Слишком много запросов, попробуйте позже' },
});

// Routes
app.use('/api/auth', authLimiter, require('./routes/auth'));
app.use('/api/profiles', require('./routes/profiles'));
app.use('/api/devices', require('./routes/heartbeat')); 
app.use('/api/devices', require('./routes/devices'));

// Health check
app.get('/api/health', (req, res) => {
  res.json({ status: 'ok', timestamp: new Date().toISOString() });
});

app.listen(PORT, () => {
  console.log(`SmartTag API running on port ${PORT}`);
});